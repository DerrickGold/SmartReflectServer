/*===========================================================
  plugin.c:

===========================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <libwebsockets.h>

#include "pluginSocket.h"
#include "plugin.h"
#include "scripts.h"
#include "pluginComLib.h"
#include "misc.h"

#define CSS_HASH_INIT_SIZE 73


#define PLUGIN_SAVED_CSS_FILE "position.txt"
#define PLUGIN_SAVED_CSS_LOCATION "%s/"PLUGIN_SAVED_CSS_FILE

#define PLUGIN_CLIENT_LOADED_MSG "PluginClient Loaded"


#define PLUGIN_COMMAND_TAG_WRITE 0
#define PLUGIN_COMMAND_TAG_CLEAR 1
#define PLUGIN_COMMAND_TAG_ALERT 2
#define PLUGIN_COMMAND_TAG_RELOAD 3
#define PLUGIN_COMMAND_TAG_SETCSS 4

#define PLUGIN_LOAD_STR "{\"css\":\"%s\",\"js\":\"%s\"}"


const char Plugin_Commands[][MAX_COMMANDS] = {
        "write",
        "clear",
        "alert",
        "reload",
        "setcss"
};


static size_t sstrlen(char *string) {

  if (!string)
    return 0;

  return strlen(string);
}

//helper function for instantiating a plugin object
Plugin_t *Plugin_Create(void) {

  Plugin_t *newPlugin = calloc(sizeof(Plugin_t), 1);
  if (!newPlugin) {
    SYSLOG(LOG_ERR, "Plugin Init: Error allocating memory...");
    return NULL;
  }

  //allocate hash table for plugin css attributes
  newPlugin->cssAttr = HashTable_init(CSS_HASH_INIT_SIZE);
  if (!newPlugin->cssAttr) {
    SYSLOG(LOG_ERR, "Plugin Init: Error allocating hash table for plugin css");
    HashTable_destroy(newPlugin->config.table);
    free(newPlugin);
    return NULL;
  }


  //generate uuid for it
  uuid_t id;
  uuid_generate(id);
  uuid_unparse(id, newPlugin->uuid);

  //get short version of uuid (no dashes)
  char *s = newPlugin->uuid, *d = newPlugin->uuidShort;
  size_t shortLen = sizeof(newPlugin->uuidShort) - 1;
  while (*s != '\0' && shortLen > 0) {
    if (*s == '-') {
      s++;
      continue;
    }
    *d++ = *s++;
    shortLen--;
  }
  *d = '\0';

  newPlugin->bgScriptPID = -1;
  newPlugin->socketInstance = NULL;
  newPlugin->externSocketInstance = NULL;
  Plugin_ClientFreeResponse(newPlugin);

  return newPlugin;
}

/*
 * This socket callback is used between this daemon and the webpage front end.
 */
static int Plugin_SocketCallback(struct lws *wsi, websocket_callback_type reason, void *user, void *in, size_t len) {

  struct lws_protocols *proto = NULL;
  if (wsi) {
    proto = (struct lws_protocols *) lws_get_protocol(wsi);
  }

  switch (reason) {
    case LWS_CALLBACK_SERVER_WRITEABLE:
      break;
    case LWS_CALLBACK_ESTABLISHED: {
      //SYSLOG(LOG_INFO, "Plugin_SocketCallback established[%s]", proto->name);
      Plugin_t *plugin = (Plugin_t *) proto->user;
      SYSLOG(LOG_INFO, "Plugin_SocketCallback get plugin %s", Plugin_GetName(plugin));
      if (!plugin->socketInstance) {
        SYSLOG(LOG_INFO, "Plugin_SocketCallback got instance![%s]", Plugin_GetName(plugin));
        plugin->socketInstance = wsi;

        //send the frontend data to the browser once the plugin connects
        if (!Plugin_isFrontendLoaded(plugin))
          Plugin_LoadFrontend(plugin);
      }
    }
      break;

    case LWS_CALLBACK_RECEIVE: {
      Plugin_t *plugin = (Plugin_t *) proto->user;
      SYSLOG(LOG_INFO, "Plugin_SocketCallback received[%s] %s", proto->name, (char *) in);

      SocketResponse_t *clientResponse = &plugin->clientResponse;
      SocketResponse_build(clientResponse, wsi, (char *) in, len);
      if (SocketResponse_done(clientResponse)) {

        char *clientData = SocketResponse_get(clientResponse);
        size_t clientSize = SocketResponse_size(clientResponse);

        //check for plugin client confirmed load
        if (!strcmp((char *) clientData, PLUGIN_CLIENT_LOADED_MSG)) {
          //set plugin as loaded
          plugin->flags |= PLUGIN_FLAG_LOADED;
          PluginCSS_sendDisplaySettings(plugin);
          SYSLOG(LOG_INFO, "Plugin_SocketCallback: confirmed plugin load: %s", proto->name);
          PluginCSS_asyncSendSaved(plugin); //start the css settings transfer
          return 0;
        } else {

          PluginCSS_asyncSendSaved(plugin);
          //if there is an external client connected specifically for this plugin, send them a response
          if (plugin->externSocketInstance)
            PluginSocket_writeToSocket(plugin->externSocketInstance, clientData, clientSize);

        }
      }
    }
      break;

    case LWS_CALLBACK_CLOSED:
      if (proto) {
        SYSLOG(LOG_INFO, "Plugin_SocketCallback disconnect[%s]", proto->name);
        Plugin_t *plugin = (Plugin_t *) proto->user;
        //if the plugin disconnects, either the plugin was unloaded, or the browser closed
        //for both situations, unload the plugin frontend
        Plugin_ClientFreeResponse(plugin);
        Plugin_UnloadFrontEnd(plugin);
        plugin->socketInstance = NULL;
      }
      break;

    default:
      break;
  }

  return 0;
}

/*
 * Create a daemon -> webpage socket handler
 */
struct lws_protocols Plugin_MakeProtocol(Plugin_t *plugin) {

  struct lws_protocols proto = {};

  //replace the old _arrayTerminate protocol with an actual protocol
  proto.name = Plugin_GetWebProtocol(plugin);
  proto.callback = &Plugin_SocketCallback;
  proto.rx_buffer_size = PLUGIN_RX_BUFFER_SIZE;
  proto.user = (void *) plugin;
  return proto;
}


/*
 * This callback handler is for handling external applications -> this daemon information. This daemon will
 * then forward messages to the proper frontend interface.
 */
static int Plugin_ExternalSocketCallback(struct lws *wsi, websocket_callback_type reason, void *user, void *in,
                                         size_t len) {

  struct lws_protocols *proto = NULL;
  if (wsi) proto = (struct lws_protocols *) lws_get_protocol(wsi);

  switch (reason) {

    case LWS_CALLBACK_ESTABLISHED: {
      SYSLOG(LOG_INFO, "Plugin_ExternalSocketCallback established[%s]", proto->name);

      if (proto) {
        Plugin_t *plugin = (Plugin_t *) proto->user;
        if (!plugin->externSocketInstance) {
          plugin->externSocketInstance = wsi;
        }
      }
    }
      break;

    case LWS_CALLBACK_RECEIVE:
      //SYSLOG(LOG_INFO, "Plugin_ExternalSocketCallback received[%s] %s", proto->name, (char*)in);
      if (proto) {
        Plugin_t *plugin = (Plugin_t *) proto->user;

        if (!plugin->socketInstance) {
          SYSLOG(LOG_ERR, "Plugin_ExternalSocketCallback: Error, no connection to front end [%s->%s]",
                 proto->name, Plugin_GetWebProtocol(plugin));

          //discard partial messages from disconnection
          SocketResponse_free(&plugin->externResponse);
          return 0;
        }

        SocketResponse_t *externResponse = &plugin->externResponse;
        SocketResponse_build(externResponse, wsi, (char *) in, len);
        if (SocketResponse_done(externResponse)) {
          /*
           * If this daemon receives a message from an external application, forward
           * it to the right plugin interface.
           */
          PluginSocket_writeToSocket(plugin->socketInstance,
                                     SocketResponse_get(externResponse),
                                     SocketResponse_size(externResponse)-1);
        }


      }
      break;
    case LWS_CALLBACK_CLOSED: {
      SYSLOG(LOG_INFO, "Plugin_ExternalSocketCallback disconnect[%s]", proto->name);
      Plugin_t *plugin = (Plugin_t *) proto->user;
      plugin->externSocketInstance = NULL;
      SocketResponse_free(&plugin->externResponse);

    }
      break;

    default:
      break;
  }

  return 0;
}

/*
 * Create an external application -> daemon socket handler
 */
struct lws_protocols Plugin_MakeExternalProtocol(Plugin_t *plugin) {

  struct lws_protocols proto = {};

  //replace the old _arrayTerminate protocol with an actual protocol
  proto.name = plugin->uuidShort;
  proto.callback = &Plugin_ExternalSocketCallback;
  proto.rx_buffer_size = PLUGIN_RX_BUFFER_SIZE;
  proto.user = (void *) plugin;
  return proto;
}


/*
  Determines if a plugin exists in the sense that it contains files that can
  be included, embedded, or executed for the index page.
*/
char Plugin_Exists(Plugin_t *plugin) {

  if (!plugin) return 0;

  int cssCount = 0;
  char **temp = PluginConf_GetCSS(plugin, &cssCount);
  if (temp)
    free(temp);

  int jsCount = 0;
  temp = PluginConf_GetJS(plugin, &jsCount);
  if (temp)
    free(temp);


  return (
          PluginConf_GetHTML(plugin) || PluginConf_GetScript(plugin) ||
          cssCount > 0 || jsCount > 0
  );
}

/*
  Cleanup all memory allocated by a plugin.
*/
static void plugin_freeSettings(Plugin_t *plugin) {

  if (plugin->config.table)
    HashTable_destroy(plugin->config.table);

  memset(&plugin->config, 0, sizeof(PluginConf_t));
}

void Plugin_Free(Plugin_t *plugin, int freeContainer) {

  if (!plugin) return;

  if (plugin->basePath) free(plugin->basePath);
  /*if (plugin->path_html) free(plugin->path_html);
  PathList_free(PluginConf_GetJS(plugin));
  PathList_free(PluginConf_GetCSS(plugin));
  if (plugin->jsMain) free(plugin->jsMain);
  if (plugin->path_script) free(plugin->path_script);
  if (plugin->path_escapeScript) free(plugin->path_escapeScript);
  */
  if (plugin->name) free(plugin->name);
  plugin_freeSettings(plugin);
  Plugin_ClientFreeResponse(plugin);
  SocketResponse_free(&plugin->externResponse);

  //free stored css attributes
  if (plugin->cssAttr)
    HashTable_destroy(plugin->cssAttr);

  memset(plugin, 0, sizeof(Plugin_t));
  if (freeContainer) free(plugin);
}

/*
  Given a directory path to where the plugin resides, a plugin is initialized
  by:

    -Storing the path to the plugin directory
    -Plugin named based on the directory name it resides in
    -'plugin.conf' file is parsed and plugin properties are assigned via
      the file

  Arguments:
    Path: refers to the base plugin directory, the directory in which all plugin
          folders are located

    pluginName: refers to the folder inside the base plugin directory for a
                specific plugin.

  Returns:
    An instantiated plugin object
*/
Plugin_t *Plugin_Init(const char *path, const char *pluginName) {
  //plugin folder must contain at least a script file
  //if no script file found, must have at least an html file

  Plugin_t *newPlugin = Plugin_Create();
  if (!newPlugin) {
    goto err;
  }

  //initialize base path for plugin directory
  int basePathSize = strlen(path) + 1;
  newPlugin->basePath = calloc(basePathSize, sizeof(char));
  if (!newPlugin->basePath) {
    SYSLOG(LOG_ERR, "Plugin Init: error allocating base path...");
    goto err;
  }
  strncpy(newPlugin->basePath, path, basePathSize);
  SYSLOG(LOG_INFO, "Plugin Init: base path: %s", newPlugin->basePath);

  //load any saved settings for plugin
  PluginCSS_load(newPlugin);

  //create plugin name
  if (Plugin_SetName(newPlugin, pluginName)) goto err;

  //get plugin details from plugin config file
  if (Plugin_loadConfig(newPlugin)) {
    SYSLOG(LOG_ERR, "Plugin Conf: failed reading plugin config file...");
    goto err;
  }

  //Plugin_writeConfig(newPlugin);
  //check that the plugin actually has entries in the config file
  if (!Plugin_Exists(newPlugin)) {
    SYSLOG(LOG_ERR, "Plugin Conf: Plugin doesn't exist?");
    goto err;
  }

  //start scheduling if the plugin is enabled
  if (Plugin_isEnabled(newPlugin)) {
    Plugin_Enable(newPlugin);
  }


  return newPlugin;

  err:
  Plugin_Free(newPlugin, 1);
  return NULL;

}

/*
 * Reload a plugin:
 *
 * -Reloads plugin config file
 * -Reschedules the plugin
 * Somethings do not change when reloading a plugin:
 * 1. The plugin directory
 * 2. The plugin name, since its based on the directory.
 * 3. uuid values
 * 4. whether the plugin is running or not for scheduling
 */
void Plugin_Reload(Plugin_t *plugin) {

  char *newName = NULL, *newPath = NULL;


  //get referennces to plugin protocols for updating
  struct lws_protocols *frontEndProto =
          PluginSocket_getProtocol(Plugin_GetWebProtocol(plugin));

  struct lws_protocols *daemonProto =
          PluginSocket_getProtocol(Plugin_GetDaemonProtocol(plugin));


  int wasRunning = Scheduler_isTicking(&plugin->scheduler) || Plugin_isEnabled(plugin);

  //preserve uuid values
  char uuid[PLUGIN_UUID_LEN], uuidShort[PLUGIN_UUID_SHORT_LEN];
  memcpy(uuid, plugin->uuid, PLUGIN_UUID_LEN);
  memcpy(uuidShort, plugin->uuidShort, PLUGIN_UUID_SHORT_LEN);

  //preserve plugin name
  /*
   * ToDo: Update name reservation for WebProtocol reservation.
   * Names should work independently of the web protocol string, however, as of now
   * protocol strings are just based on plugin names.
   */
  char *oldName = Plugin_GetWebProtocol(plugin);

  size_t nameSize = (strlen(oldName) + 1) * sizeof(char);
  newName = malloc(nameSize);

  if (!newName) {
    SYSLOG(LOG_ERR, "Plugin_Reload: Error allocating space for old plugin name: %s", oldName);
    goto _err;
  }

  memcpy(newName, oldName, nameSize);

  //next, save plugin's directory
  char *oldPath = Plugin_GetDirectory(plugin);

  size_t pathSize = (strlen(oldPath) + 1) * sizeof(char);
  newPath = malloc(pathSize);
  if (!newPath) {
    SYSLOG(LOG_ERR, "Plugin_Reload: Error allocating space for old plugin path %s", oldPath);
    goto _err;
  }

  memcpy(newPath, oldPath, pathSize);

  //next free the old plugin
  Plugin_Free(plugin, 0); //0 so we can keep the Plugin_t container allocation

  //hook up preserved data
  plugin->name = newName;
  plugin->basePath = newPath;
  memcpy(plugin->uuid, uuid, PLUGIN_UUID_LEN);
  memcpy(plugin->uuidShort, uuidShort, PLUGIN_UUID_SHORT_LEN);

  //update protocol details
  frontEndProto->name = Plugin_GetName(plugin);
  daemonProto->name = Plugin_GetDaemonProtocol(plugin);

  //start loading the rest of the plugin as normal
  int pluginStarted = 1;

  //reload details from the config file
  if (Plugin_loadConfig(plugin)) {
    SYSLOG(LOG_ERR, "Plugin Conf: failed reading plugin config file...");
    goto _err;
  }

  //check that the plugin actually has entries in the config file
  if (!Plugin_Exists(plugin)) goto _err;

  //set plugin to render on load
  if (wasRunning)
    Plugin_Enable(plugin);


  return;
  //error handling / cleanup
  _err:
  if (newName) free(newName);
  if (newPath) free(newPath);

  //last case, ditch the plugin if something errors badly
  if (pluginStarted) Plugin_Free(plugin, 1);

  return;
}

/*
  Print basic plugin information.
*/
void Plugin_Print(Plugin_t *plugin) {

  if (Plugin_GetName(plugin))
    printf("Name: %s\n", Plugin_GetName(plugin));

  char *html = PluginConf_GetHTML(plugin);
  if (html)
    printf("HTML: %s\n", html);

}


char *Plugin_GetName(Plugin_t *plugin) {

  if (!plugin)
    return NULL;

  return plugin->name;
}

int Plugin_SetName(Plugin_t *plugin, const char *name) {

  int nameLen = strlen(name);
  plugin->name = calloc(nameLen + 1, sizeof(char));
  if (!plugin->name) {
    SYSLOG(LOG_ERR, "Plugin_SetName: failed allocating name space.");
    return -1;
  }
  strncpy(plugin->name, name, nameLen);
  return 0;
}


char *Plugin_GetID(Plugin_t *plugin) {

  return plugin->uuid;
}


int Plugin_SendMsg(Plugin_t *plugin, char *command, char *data) {

  if (!plugin->socketInstance) return -1;

  char *cmd = PluginComLib_makeMsg(command, data);
  if (!cmd) return -1;

  int written = PluginSocket_writeToSocket(plugin->socketInstance, cmd, -1);
  free(cmd);

  return written;
}


void Plugin_ClientFreeResponse(Plugin_t *plugin) {
  SocketResponse_free(&plugin->clientResponse);
}


char Plugin_ClientResponseDone(Plugin_t *plugin) {
  return SocketResponse_done(&plugin->clientResponse);
}

char *Plugin_ClientGetResponse(Plugin_t *plugin) {

  return SocketResponse_get(&plugin->clientResponse);
}

size_t Plugin_ClientGetResponseSize(Plugin_t *plugin) {

  return SocketResponse_size(&plugin->clientResponse);
}





int Plugin_isEnabled(Plugin_t *plugin) {

  if (!plugin) return 0;

  return plugin->flags & PLUGIN_FLAG_RENDER;
}

/*
 * Enable a previously disabled plugin.
 */
void Plugin_Enable(Plugin_t *plugin) {

  //if plugin is already enabled, just return
  //if a plugin is enabled, but hasn't been scheduled, let it schedule itself
  if (Plugin_isEnabled(plugin) && Scheduler_isInitialized(&plugin->scheduler))
    return;

  PLUGIN_SET_ENABLED(plugin);
  //recreate the schedule for the plugin
  Plugin_SetSchedule(plugin);
  Plugin_StartSchedule(plugin);
}


/*
 * Disable a previously enabled plugin.
 */
void Plugin_Disable(Plugin_t *plugin) {

  if (!Plugin_isEnabled(plugin)) return;
  //free some disposable memory
  Plugin_ClientFreeResponse(plugin);
  SocketResponse_free(&plugin->externResponse);
  //remove plugin from scheduler
  Plugin_StopSchedule(plugin);
  PLUGIN_SET_DISABLED(plugin);
}


int Plugin_isFrontendLoaded(Plugin_t *plugin) {

  return plugin->flags & PLUGIN_FLAG_LOADED;
}

void Plugin_UnloadFrontEnd(Plugin_t *plugin) {

  plugin->flags &= ~PLUGIN_FLAG_LOADED;
}

void Plugin_StopBgScript(Plugin_t *plugin) {

  if (plugin->flags & PLUGIN_FLAG_INBG && plugin->bgScriptPID > 0) {
    Script_KillBG(plugin->bgScriptPID);
    //unset the inbg flag to indicate the script is not running
    plugin->flags &= ~PLUGIN_FLAG_INBG;
    plugin->bgScriptPID = -1;
  }

}

int Plugin_isConnected(Plugin_t *plugin) {

  if (!plugin)
    return 0;

  return (plugin->socketInstance != NULL);
}


static char *_pluginLoadHTML(Plugin_t *plugin) {

  char *filepath = PluginConf_GetHTML(plugin);
  if (!filepath)
    return NULL;


  FILE *file = fopen(filepath, "r");
  if (!file) {
    SYSLOG(LOG_ERR, "_pluginLoadHTML: Failed opening html file: %s", filepath);
    return NULL;
  }
  SYSLOG(LOG_INFO, "_pluginLoadHTML: Reading in html file");
  size_t dataSize = 0;
  char *data = NULL;

  //read 4kb of data at a time
  char buffer[4 * 1024];

  size_t dataRead = 0;
  int firstAlloc = 0;

  while (!feof(file)) {
    SYSLOG(LOG_INFO, "Doing read");
    memset(buffer, 0, sizeof(buffer));
    size_t curRead = fread(buffer, 1, sizeof(buffer), file);
    if (!curRead)
      break;

    dataRead += curRead;

    if (dataRead > dataSize) {
      SYSLOG(LOG_INFO, "Reallocating");
      if (!data) {
        dataSize = dataRead + 1;
        firstAlloc++;
      } else
        dataSize *= 2;

      char *temp = realloc(data, dataSize);
      if (!temp) {
        SYSLOG(LOG_ERR, "_pluginLoadHTML: Failed allocating memory for html file\n");
        fclose(file);
        return NULL;
      }
      SYSLOG(LOG_INFO, "Realloc'd");

      data = temp;
      //check if this is first allocation
      if (firstAlloc) {
        data[0] = '\0';
        firstAlloc = 0;
      }

    }

    SYSLOG(LOG_INFO, "StrCat");
    strcat(data, buffer);
  }

  SYSLOG(LOG_INFO, "Closing file");
  //all file in memory now
  fclose(file);
  return data;
}

/*
 * Tells the front end to load a plugin's
 * css and javascript file, as well as
 * sending any html data that needs to be loaded.
 */
void Plugin_LoadFrontend(Plugin_t *plugin) {    //load some data

  if (!Plugin_isEnabled(plugin))
    return;

  SYSLOG(LOG_INFO, "Plugin_LoadFrontend: preparing to send frontend data");

  char *mainClass = PluginConf_GetJSMain(plugin);

  int cssCount = 0;
  char **cssPaths = PluginConf_GetCSS(plugin, &cssCount);

  int jsCount = 0;
  char **jsPaths = PluginConf_GetJS(plugin, &jsCount);

  SYSLOG(LOG_INFO, "Plugin_LoadFrontend: %d js files", jsCount);
  SYSLOG(LOG_INFO, "Plugin_LoadFrontend: %d css files", cssCount);

  char *loadStr = NULL;
  size_t loadStrSize = 1;

  //calculate a preliminary size for the load string
  loadStrSize += sstrlen(mainClass);

  int i = 0;
  for (i = 0; i < cssCount; i++)
    loadStrSize += sstrlen(cssPaths[i]);



  //add the commas and quotes for each css file
  loadStrSize += sstrlen(",\"\"") * cssCount;

  for (i = 0; i < jsCount; i++)
    loadStrSize += sstrlen(jsPaths[i]);

  //add comma and quotes for each js file
  loadStrSize += sstrlen(",\"\"") * jsCount;

  //now fine tune the size with the extra symbols
  loadStrSize += 128;

  //now allocate the memory
  SYSLOG(LOG_INFO, "Plugin_LoadFrontend: allocating load string %zu", loadStrSize);
  loadStr = calloc(loadStrSize, sizeof(char));
  if (!loadStr) {
    SYSLOG(LOG_ERR, "Plugin_LoadFrontend: Error allocating load string space");
    goto _cleanup;
  }

  strcpy(loadStr, "{\"css\":[");
  for (i = 0; i < cssCount; i++) {
    strcat(loadStr, "\"");
    strcat(loadStr, cssPaths[i]);
    if (i < cssCount - 1)
      strcat(loadStr, "\",");
    else
      strcat(loadStr, "\"");
  }


  strcat(loadStr, "],\"js\":[");
  for (i = 0; i < jsCount; i++) {
    strcat(loadStr, "\"");
    strcat(loadStr, jsPaths[i]);
    if (i < jsCount - 1)
      strcat(loadStr, "\",");
    else
      strcat(loadStr, "\"");
  }


  strcat(loadStr, "]");

  if (mainClass) {
    strcat(loadStr, ",\"main\":\"");
    strcat(loadStr, mainClass);
    strcat(loadStr, "\"");
  }
  strcat(loadStr, "}");


  //send the js and css files to load
  //SYSLOG(LOG_INFO, "Sending %s to browser", loadStr);
  Plugin_SendMsg(plugin, "load", loadStr);


  char *html = _pluginLoadHTML(plugin);
  //send html first
  if (html) {
    //SYSLOG(LOG_INFO, "HTML TO SEND: %s", html);
    Plugin_SendMsg(plugin, "innerdiv", html);
    free(html);
    html = NULL;
  }

  _cleanup:
  if (loadStr) free(loadStr);
  if (jsPaths) free(jsPaths);
  if (cssPaths) free(cssPaths);

  return;
}

/*
 * Remove plugin data from the web browser.
 */
void Plugin_UnloadFrontend(Plugin_t *plugin) {

  Plugin_SendMsg(plugin, "unload", NULL);
  //unset plugin as loaded.
  plugin->flags &= ~PLUGIN_FLAG_LOADED;
}

/*=======================================================================================
 * Plugin CSS attribute storage
=======================================================================================*/
//Any css sent through the main API will get parsed through here and stored into
//a hash table for saving
void PluginCSS_store(Plugin_t *plugin, char *cssValString) {
  //cssValString will be in the form of a <cssattr>=<value>;...
  SYSLOG(LOG_INFO, "Storing CSS: %s", cssValString);
  char *cssSetting = strtok(cssValString, ";");
  do {

    if (cssSetting) {
      char *separator = strchr(cssSetting, '=');
      //skip badly formed attributes
      if (separator) {

        *separator = '\0';
        char *attr = cssSetting;
        char *value = separator + 1;

        if (attr && value && attr[0] != '\0' && value[0] != '\0') {
          HashData_t *data = HashData_create(attr, value);
          HashTable_add(plugin->cssAttr, data);
        }
      }
    }

    //update cssSetting to point to next setting
    cssSetting = strtok(NULL, ";");
  } while (cssSetting);

}

void PluginCSS_dump(Plugin_t *plugin) {


  char filepath[PATH_MAX];
  snprintf(filepath, PATH_MAX, PLUGIN_SAVED_CSS_LOCATION, Plugin_GetDirectory(plugin));

  FILE *dumpFile = fopen(filepath, "w");
  if (!dumpFile) {
    SYSLOG(LOG_ERR, "PluginCSS_dump: Error opening output file: %s", filepath);
    return;
  }

  HashTable_print(dumpFile, plugin->cssAttr);
  //close output file
  fclose(dumpFile);
}

void PluginCSS_load(Plugin_t *plugin) {

  //css is no longer loaded
  plugin->loadedSavedCSS = -1;

  //generate path for saved css settings
  char filepath[PATH_MAX];
  snprintf(filepath, PATH_MAX, PLUGIN_SAVED_CSS_LOCATION, Plugin_GetDirectory(plugin));

  FILE *dumpFile = fopen(filepath, "r");
  if (!dumpFile) {
    SYSLOG(LOG_ERR, "PluginCSS_load: Error opening input file: %s", filepath);
    return;
  }

  SYSLOG(LOG_INFO, "PluginCSS_load: Reading saved file");
  char *buffer = filepath;
  while (!feof(dumpFile)) {

    char *cssLine = fgets(buffer, PATH_MAX, dumpFile);
    //if nothing read, exit loop
    if (!cssLine)
      break;

    char *newline = strrchr(cssLine, '\n');
    if (newline)
      *newline = '\0';

    //remove windows return carriage
    newline = strrchr(cssLine, '\r');
    if (newline)
      *newline = '\0';

    char *separator = strchr(cssLine, ':');
    //skip badly formed attributes
    if (!separator)
      continue;

    *separator = '\0';
    char *attr = cssLine;
    char *value = separator + 1;

    if (attr && value) {
      HashData_t *data = HashData_create(attr, value);
      SYSLOG(LOG_INFO, "CSS loading: %s=%s", attr, value);
      HashTable_add(plugin->cssAttr, data);
    }

  }

  //close input file
  fclose(dumpFile);
}

int PluginCSS_sendSaved(Plugin_t *plugin) {

  SYSLOG(LOG_INFO, "PluginCSS_sendSaved: Sending saved CSS");
  if (!plugin->cssAttr) {
    SYSLOG(LOG_INFO, "PluginCSS_sendSaved: No CSS to send!");
    plugin->loadedSavedCSS = 1;
    return 0;
  }


  char buf[PATH_MAX];
  HashTable_t *table = plugin->cssAttr;

  //find next entry that needs to be sent
  size_t lastEntry = plugin->lastSentCSS + 1;

  //skip empty entries
  while (!table->entries[lastEntry] && lastEntry < table->size)
    lastEntry++;

  if (lastEntry >= table->size) {
    SYSLOG(LOG_INFO, "PluginCSS_sendSaved: No more css entries!");
    plugin->loadedSavedCSS = 1;
    return 0;
  }

  plugin->lastSentCSS = lastEntry;
  //skip entries where key may not be set (malformed entries)
  HashData_t *entry = table->entries[lastEntry];
  if (entry->key && entry->value) {

    snprintf(buf, PATH_MAX, "%s=%s;", entry->key, entry->value);
    SYSLOG(LOG_INFO, "PluginCSS_sendSaved: %s", buf);
    Plugin_SendMsg(plugin, "setcss", buf);
    plugin->loadedSavedCSS = 0;
    return 1;
  }

  plugin->loadedSavedCSS = 1;
  SYSLOG(LOG_INFO, "PluginCSS_sendSaved: blank entry?");
  return 0;
}

void PluginCSS_asyncSendSaved(Plugin_t *plugin) {

  if (!plugin->loadedSavedCSS)
    PluginCSS_sendSaved(plugin);
}

void PluginCSS_sendDisplaySettings(Plugin_t *plugin) {

  plugin->lastSentCSS = 0;
  plugin->loadedSavedCSS = 0;
}



