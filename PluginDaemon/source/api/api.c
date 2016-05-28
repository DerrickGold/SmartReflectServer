#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <syslog.h>
#include <libgen.h>

//#include "api.h"
#include "plugin.h"
#include "apiPending.h"
#include "apiResponse.h"
#include "pluginSocket.h"
#include "display.h"
#include "socketResponse.h"
#include "misc.h"
#include "pluginLoader.h"


#define API_PROTO "STDIN"
#define API_PLUGLIST_DELIM "\n"
#define CLIENT_API_HEADER "[API]"

//defined in main.c, read for time it takes to boot
extern unsigned int MainProgram_BootSeconds;


const char *API_STATUS_STRING[] = {
     [API_STATUS_SUCCESS] = "success",
     [API_STATUS_FAIL] = "fail",
     [API_STATUS_PENDING] = "pending",
     [API_STATUS_BUSY] = "busy"
};

static int _shutdown = 0;

static int _reboot = 0;

static SocketResponse_t inputResponse;

static char *pluginsDirectory = NULL;

static void doAction(struct lws *wsi, char * identifier, APIAction_e action, Plugin_t *plugin, char *value);

APICommand_t allActions[API_ACTION_COUNT] = {
        [API_LIST_CMDS] = {"commands", NONE},
        /*
         * disable <pluginName>
         * Unloads a plugin from the frontend, unschedules the plugin
         * from updating, and stops any child processes of plugin.
         */
        [API_DISABLE] = {"disable", NEED_PLUGIN},

        /*
         * enable <pluginName>
         * Loads a plugin into the frontend and if applicable, enables
         * it in the main scheduler for providing display updates.
         */
        [API_ENABLE] = {"enable", NEED_PLUGIN},

        /*
         * reload <pluginName>
         * Reloads a plugins 'plugin.conf' file configuration,
         * unloads the plugin from the frontend and scheduler,
         * then reloads it back into the frontend and scheduler
         * with the newly loaded configuration setup.
         */
        [API_RELOAD] = {"reload", NEED_PLUGIN},

        /*
         * plugins
         * List all plugins currently loaded in the PluginDaemon.
         */
        [API_PLUGINS] = {"list", NONE},

        /*
         * getdir <pluginName>
         * Returns the filepath to the directory in which the
         * specified plugin's configuration file resides.
         */
        [API_PLUG_DIR] = {"getdir", NEED_PLUGIN},

        /*
         * rmplug <pluginName>
         * Completely unloads a plugin from the PluginDaemon.
         * After this is called, the PluginDaemon will be completely
         * unaware of this plugins existence.
         */
        [API_RM_PLUG] = {"rmplug",     NEED_PLUGIN},

        /*
         * mirrorsize
         * Returns the frontend display dimensions as a string
         * formatted as 'widthxheight'.
         */
        [API_MIR_SIZE] = {"mirrorsize", NONE},

        /*
         * stop
         * Stops the plugin daemon for a clean shutdown,
         */
        [API_STOP] = {"stop", NONE},

        /*
         * setcss <plugin> <cssAttr=value;attr=value....>
         * Set a CSS value for a plugin's frontend
         * display.
         */
        [API_SET_CSS] = {"setcss", NEED_PLUGIN | NEED_VALUES},

        /*
         * getcss <plugin> <attr1, attr2,....>
         * Query the plugin's css attribute from the frontend.
         * If plugin is not loaded to frontend, nothing will
         * be returned. Multiple CSS properties can be queried
         * for and will be delimited with a newline character.
         */
        [API_GET_CSS] = {"getcss", NEED_PLUGIN | NEED_VALUES},


        [API_DUMP_CSS] = {"savecss", NEED_PLUGIN},

        /*
         * getstate <plugin>
         * Returns whether a plugin is enabled and running
         * or not.
         */
        [API_GET_STATE] = {"getstate", NEED_PLUGIN},

        /*
         * jscmd <plugin> {\"fn\":\"<FUNCTION>\",\"args\":<ARGS>}
         * If plugin has a javascript class instantiated, this will
         * send a command to that plugin to execute a function. The
         * string sent should be formatted as JSON with the 'fn' property
         * being the name of the function to execute, and 'args' property
         * being the arguments to provide to that function call.
         */
        [API_JS_PLUG_CMD] = {"jscmd", NEED_PLUGIN | NEED_VALUES},

        /*
         * display
         * Returns whether or not a display is connected to the daemon.
         */
        [API_DISP_CONNECT] = {"display", NONE},

        /*
         * getcfg <plugin> <config_file_setting>
         * Returns a value in a plugins config file based
         * on the queried setting.
         */
        [API_GET_CONFIG] = {"getopt", NEED_PLUGIN | NEED_VALUES},

        /*
         * setcfg <plugin> <config_file_setting>=<new_value>
         * Write a new entry or overwrite an existing entry in a plugin's
         * config file.
         */
        [API_SET_CONFIG] = {"setopt", NEED_PLUGIN | NEED_VALUES},

        [API_INSTALL] = {"install", NEED_VALUES},

        [API_REBOOT] = {"reboot", NONE},

};


//Returns an APIAction_e value for a given search string
static APIAction_e findAPICall(char *search) {

  APIAction_e action = API_NO_ACTION;
  for (int i = 0; i < API_ACTION_COUNT; i++) {
    if (strncmp(allActions[i].name, search, strlen(allActions[i].name)) == 0) {
      action = i;
      break;
    }
  }

  return action;
}


static int response(APIResponse_t *response, struct lws *wsi, char *identifier, Plugin_t *plugin, APIAction_e action,
                        APIStatus_e status)
{

  char *plugName = NULL;
  if (plugin)
    plugName = Plugin_GetName(plugin);

  return APIResponse_send(response, wsi, identifier, plugName, action, status);
}


/*
 * Sets an action to wait for a response from the daemon plugin communicator.
 */
static APIStatus_e waitForDaemonResponse(APIResponse_t *response, char *identifier, APIAction_e action,
                                             Plugin_t *plugin, struct lws *socket)
{

  if (!Display_IsDisplayConnected()) {
    APIResponse_concat(response, "No display connected.", -1);
    return API_STATUS_FAIL;
  }

  APIStatus_e rtrn = API_STATUS_PENDING;
  if (!APIPending_addAction(APIPENDING_DISPLAY, identifier, action, plugin, socket)) {
    Display_ClearDisplayResponse();
  } else {
    rtrn = API_STATUS_FAIL;
    APIResponse_concat(response, "Failed to add pending request.", -1);
  }

  return rtrn;
}

/*
 * Sets an action to wait for a response for a value obtained from
 * a plugin's frontend.
 */
static APIStatus_e waitForPluginResponse(APIResponse_t *response, char *identifier, APIAction_e action,
                                             Plugin_t *plugin, struct lws *socket)
{

  if (!Plugin_isConnected(plugin)) {
    APIResponse_concat(response, "Plugin not connected to frontend", -1);
    return API_STATUS_FAIL;
  }

  APIStatus_e rtrn = API_STATUS_PENDING;
  if (!APIPending_addAction(APIPENDING_PLUGIN, identifier, action, plugin, socket)) {
    //Plugin_FreeFrontEndResponse(plugin);
    Plugin_ClientFreeResponse(plugin);
  } else {
    rtrn = API_STATUS_FAIL;
    APIResponse_concat(response, "Failed to add pending request.", -1);
  }

  return rtrn;
}



/*
 * API action implementations
 */

static int actionPluginList(void *plugin, void *data) {

  APIResponse_t *response = (APIResponse_t *) data;

  char *name = Plugin_GetName((Plugin_t *) plugin);
  APIResponse_concat(response, name, -1);
  APIResponse_concat(response, API_PLUGLIST_DELIM, 1);
  return 0;
}

static int actionPluginEnable(Plugin_t *plugin) {

  Plugin_Enable(plugin);
  Display_LoadPlugin(plugin);
  //set plugin configuration to load on next boot
  PluginConf_setValue(plugin, PLUGIN_CONF_START_ON_LOAD, PLUGIN_CONF_OPT_TRUE);
  SYSLOG(LOG_INFO, "API: Enabled plugin");
  return 0;
}

static int actionPluginDisable(void *plug, void *shutdown) {

  Plugin_t *plugin = (Plugin_t *) plug;

  Display_UnloadPlugin(plugin);

  //set plugin to not load next time server is started
  //if the mirror is shutting down, then the plugin isn't being disabled by the user
  //and is likely intended to startup again on next boot.
  if (!shutdown)
    PluginConf_setValue(plugin, PLUGIN_CONF_START_ON_LOAD, PLUGIN_CONF_OPT_FALSE);
  else
    PluginSocket_Update();

  SYSLOG(LOG_INFO, "API: Disabled plugin");
  Plugin_Disable(plugin);

  return 0;
}


//Write or replace a new value to a plugin's config file
static int actionWritePluginSetting(APIResponse_t *response, Plugin_t *plugin, char *setting) {

  SYSLOG(LOG_INFO, "Set config: %s", setting);
  char *attr = strrchr(setting, '=');


  if (!attr) {
    APIResponse_concat(response, "Improperly formatted config string. Missing '='", -1);
    return -1;
  }

  *attr = '\0';
  attr++;

  if (strlen(setting) < 1) {
    APIResponse_concat(response, "Improperly formatted config string. Blank configuration setting.", -1);
    return -1;
  }

  SYSLOG(LOG_INFO, "Setting plugin config %s = %s", setting, attr);
  if (PluginConf_setValue(plugin, setting, attr))
    return -1;

  return 0;
}

//get a setting from the config file
static int actionGetPluginSetting(APIResponse_t *response, Plugin_t *plugin, char *setting) {

  int count = 0;
  char **config = PluginConf_GetConfigValue(plugin, setting, &count);

  APIResponse_concat(response, setting, -1);
  APIResponse_concat(response, ":", 1);
  if (!count)
    return -1;


  int i = 0;
  for (i = 0; i < count; i++) {
    APIResponse_concat(response, config[i], -1);
    APIResponse_concat(response, "\n", 1);
  }

  free(config);
  return 0;
}

static int actionInstallPlugin(struct lws *socket, char *identifier, APIResponse_t *response, char *data) {

  PluginLoader_InstallPlugin(pluginsDirectory, (char *)data);
  doAction(socket, identifier, API_REBOOT, NULL, NULL);
  return 0;
}

static void doAction(struct lws *wsi, char * identifier, APIAction_e action, Plugin_t *plugin, char *value) {

  APIStatus_e status = API_STATUS_SUCCESS;

  APIResponse_t *immResponse = APIResponse_new();
  if (!immResponse)
    return;

  if (allActions[action].flag & NEED_PLUGIN && plugin == NULL) {
    SYSLOG(LOG_INFO, "Action requires a plugin to be specified");
    status = API_STATUS_FAIL;
    //PluginSocket_writeToSocket(wsi, API_API_STATUS_FAIL"Action requires a plugin to be specified", -1);
    //return;
    APIResponse_concat(immResponse, "Action requires a plugin to be specified.", -1);
  }

  if (allActions[action].flag & NEED_VALUES && value == NULL) {
    SYSLOG(LOG_INFO, "Action requires values to be provided for plugin");
    status = API_STATUS_FAIL;
    APIResponse_concat(immResponse, "Action requires values to be provided for plugin.", -1);
  }

  if (status != API_STATUS_SUCCESS)
    goto _response;

  switch (action) {
    case API_NO_ACTION:
      //PluginSocket_writeToSocket(wsi, "Specified action does not exist", -1);
      //return;
      status = API_STATUS_FAIL;
      APIResponse_concat(immResponse, "Specified action does not exist", -1);
      //syslog(LOG_INFO, "Specified action does not exist.");
      break;
    case API_LIST_CMDS: {

      int i = 0;
      for (i = 0; i < API_ACTION_COUNT; i++) {
        APIResponse_concat(immResponse, allActions[i].name, -1);
        APIResponse_concat(immResponse, API_PLUGLIST_DELIM, 1);
      }
    }
      break;
    case API_DISABLE:
      if (actionPluginDisable(plugin, NULL))
        status = API_STATUS_FAIL;
      break;
    case API_ENABLE:
      if (actionPluginEnable(plugin))
        status = API_STATUS_FAIL;
      break;
    case API_RELOAD: {
      //reload plugin and reload plugin
      //syslog(LOG_INFO, "Reloaded plugin");
      int status = Plugin_isEnabled(plugin);
      //only disable and re-enable plugin on the frontend
      //if it was previously running
      if (status) actionPluginDisable(plugin, NULL);
      Plugin_Reload(plugin);
      if (status) actionPluginEnable(plugin);

    }
      break;
    case API_PLUGINS:
      //list plugins to fifo file
      //builds up the payload to send back
      PluginList_ForEach(actionPluginList, immResponse);
      break;
    case API_PLUG_DIR:
      //get plugin's data directory
      APIResponse_concat(immResponse, Plugin_GetDirectory(plugin), -1);
      SYSLOG(LOG_INFO, "Got plugin directory");
      break;
    case API_RM_PLUG: {
      //if plugin is currently running, unload it first
      if (Plugin_isEnabled(plugin))
        actionPluginDisable(plugin, NULL);

      //set plugin name as payload since the name
      //will not be available after removing the plugin.
      APIResponse_concat(immResponse, Plugin_GetName(plugin), -1);
      //then delete it
      PluginList_Delete(Plugin_GetName(plugin));
      plugin = NULL;
      SYSLOG(LOG_INFO, "Unloaded plugin");
    }
      break;
    case API_MIR_SIZE: {
      if (Display_GetDisplaySize())
        status = API_STATUS_FAIL;

      status = waitForDaemonResponse(immResponse, identifier, action, plugin, wsi);
    }
      break;

    case API_REBOOT: {
      _reboot = 1;

      char numString[PATH_MAX];
      sprintf(numString, "%d", MainProgram_BootSeconds);
      //return number of seconds to the web gui to reboot on
      APIResponse_concat(immResponse, numString, -1);
      Display_Reload(MainProgram_BootSeconds);
    }
    case API_STOP:
      _shutdown = 1;
      if (!API_Reboot())
        APIResponse_concat(immResponse, "Shutting down daemon...", -1);
      SYSLOG(LOG_INFO, "Stopped mirror");
      break;
    case API_SET_CSS:
      SYSLOG(LOG_INFO, "Set plugin CSS");
      Plugin_SendMsg(plugin, "setcss", value);
      PluginCSS_store(plugin, value);
      break;
    case API_GET_CSS:
      //modify the css of the specified plugin and regenerate index
      //modify this css file with the values
      SYSLOG(LOG_INFO, "Modified plugin css");
      Plugin_SendMsg(plugin, "getcss", value);
      status = waitForPluginResponse(immResponse, identifier, action, plugin, wsi);
      break;

    case API_DUMP_CSS:
      SYSLOG(LOG_INFO, "Saving plugin's css");
      PluginCSS_dump(plugin);

      break;
    case API_GET_STATE: {
      SYSLOG(LOG_INFO, "Getting plugin status.");
      if (!Plugin_isEnabled(plugin))
        status = API_STATUS_FAIL;
    }
      break;

    case API_JS_PLUG_CMD: {
      Plugin_SendMsg(plugin, "jsPluginCmd", value);
      status = waitForPluginResponse(immResponse, identifier, action, plugin, wsi);
      //WAIT_FOR_RESPONSE;
    }
      break;

      //check if there is a display connected
    case API_DISP_CONNECT:
      if (!Display_IsDisplayConnected())
        status = API_STATUS_FAIL;
      break;

    case API_GET_CONFIG:
      if (actionGetPluginSetting(immResponse, plugin, value))
        status = API_STATUS_FAIL;
      break;

    case API_SET_CONFIG:
      if (actionWritePluginSetting(immResponse, plugin, value))
        status = API_STATUS_FAIL;

      break;

    case API_INSTALL:
      SYSLOG(LOG_INFO, "Installing: %s", value);
      actionInstallPlugin(wsi, identifier, immResponse, value);
      break;
    default:
      break;
  }

  _response:
  //respond with the built payload for the given action
  response(immResponse, wsi, identifier, plugin, action, status);
  APIResponse_free(immResponse);
}



static int parseInputLine(char *input, char *inputEnd, char **output, char **nextLine) {

  char parsed = 0;
  char *pos = input, *outStart = NULL;

  //eliminate leading whitespace for the command
  while (pos < inputEnd && (*pos == ' ' || *pos == '\t'))
    pos++;

  //exhausted input, nothing read
  if (pos >= inputEnd)
    return -1;

  //otherwise, leading whitespace removed
  outStart = pos;

  //find the end of the line now
  while (pos < inputEnd && *pos != '\n' && *pos != '\0')
    pos++;

  //end of input, no further lines given
  if (pos >= inputEnd - 1)
    parsed++;
  else {
    //otherwise, we have hit the end of the given command and more
    //input exists, so remove the deliminating the command, and
    //advance the pos ptr to the next input
    (*pos) = '\0';
    pos++;
  }

  if (pos - 1 != outStart)
    *output = outStart;

  *nextLine = pos;
  return parsed;
}
/*
 * Parses input given to standard input protocol and applies an action
 *
 * Inputs follow the form of:
 * <requestID>\n<action>\n<plugin>\n<values>
 *
 * Multiple clients can be using the API at the same time, requestID is an identifying
 * token that is provided by the client, and then embedded back into the response as to provide
 * a way for the client to recognize responses intended for it to receive.
 */
static void parseInput(char *input, size_t inputLen, struct lws *wsi) {

  int parsed = 0, parseStatus = 0;
  char *inputEnd = input + inputLen;


  Plugin_t *plugin = NULL;
  char *identifier = NULL, *cmd = NULL, *pluginName = NULL, *value = NULL;
  //first get the command string

  char *pos = input;

  //first read identifier if given
  if ((parseStatus = parseInputLine(pos, inputEnd, &identifier, &pos)) < 0) {
    //end of string
    return;
  }
  parsed += (parseStatus > 0);
  SYSLOG(LOG_INFO, "Given Identifier Token: %s", identifier);

  //next get the command that is to be executed
  if ((parseStatus = parseInputLine(pos, inputEnd, &cmd, &pos)) < 0) {
    //end of string
    return;
  }
  parsed += (parseStatus > 0);
  SYSLOG(LOG_INFO, "Entered Command: %s", cmd);

  //check to see if the entered command is a valid API call
  APIAction_e action = findAPICall(cmd);

  //if all input has been consumed, try the action
  if (parsed || action == API_NO_ACTION)
    goto _doAction;

  SYSLOG(LOG_INFO, "Action Num: %d", action);

  //Next get the plugin name if required
  if ((parseStatus = parseInputLine(pos, inputEnd, &pluginName, &pos)) < 0) {
    //end of string
    return;
  }
  parsed += (parseStatus > 0);

  if (pluginName && strlen(pluginName) > 0) {
    //attempt to find the plugin
    plugin = PluginList_Find(pluginName);
  }

  //if all input has been consumed, try the action
  if (parsed)
    goto _doAction;

  //next, grab the input values, which can be anything
  value = pos;

  _doAction:
  //now check if plugin exists
  doAction(wsi, identifier, action, plugin, value);
}

/*
 * This callback handler is for handling external applications -> this daemon information. This daemon will
 * then forward messages to the proper frontend interface.
 */
static int apiCallback(struct lws *wsi, websocket_callback_type reason, void *user, void *in, size_t len) {


  struct lws_protocols *proto = NULL;
  if (wsi) proto = (struct lws_protocols *) lws_get_protocol(wsi);

  switch (reason) {
    case LWS_CALLBACK_SERVER_WRITEABLE: {
      PluginSocket_writeBuffers(wsi);
      lws_callback_on_writable(wsi);
    } break;

    case LWS_CALLBACK_ESTABLISHED:
      SYSLOG(LOG_INFO, "InputReader connection established established[%s]", proto->name);
      lws_callback_on_writable(wsi);
      break;

    case LWS_CALLBACK_RECEIVE: {

      if (!len)
        return 0;

      SocketResponse_build(&inputResponse, wsi, (char *) in, len);
      if (SocketResponse_done(&inputResponse)) {

        SYSLOG(LOG_INFO, "Command->%s", SocketResponse_get(&inputResponse));
        //wait for full message before parsing input
        if (proto) {
          parseInput(SocketResponse_get(&inputResponse),
                     SocketResponse_size(&inputResponse), wsi);

        }
        SocketResponse_free(&inputResponse);
      }
    }
      break;

    case LWS_CALLBACK_CLOSED:
      SYSLOG(LOG_INFO, "InputReader disconnect[%s]", proto->name);
      SocketResponse_free(&inputResponse);
      return -1;

    default:
      break;
  }

  return 0;
}

/*
 * public facing functions
 */


void API_ShutdownPlugins() {

  //pass non-null value into actionPluginDisable so each plugin isn't disabled
  //on next boot
  char dontSave = 1;
  PluginList_ForEach(actionPluginDisable, (void*)&dontSave);
  PluginSocket_Update();
  SocketResponse_free(&inputResponse);
}

void API_Init(char *pluginDir) {

  pluginsDirectory = pluginDir;

  //create websocket protocol for standard input
  struct lws_protocols proto = {};
  //replace the old _arrayTerminate protocol with an actual protocol
  proto.name = API_PROTO;
  proto.callback = &apiCallback;
  proto.rx_buffer_size = PLUGIN_RX_BUFFER_SIZE;
  PluginSocket_AddProtocol(&proto);
  SocketResponse_free(&inputResponse);
  APIPending_init();

  _shutdown = 0;
  _reboot = 0;
}

//API Update loop for processing pending actions
//Pending actions will block other api calls
void API_Update(void) {

  APIPending_update();
}


int API_Shutdown(void) {

  return _shutdown;
}

int API_Reboot(void) {

  return _reboot;
}

int API_Parse(struct lws *socket, char *in, size_t len) {

  size_t headerLen = strlen(CLIENT_API_HEADER);
  //if plugin client wants to make an api call, it needs the API header
  if (strncmp(in, CLIENT_API_HEADER, headerLen))
    return 0;

  size_t newLen = len - headerLen;
  char *temp = malloc(newLen);
  if (!temp) return -1;

  memcpy(temp, in + headerLen, newLen);
  parseInput(temp, newLen, socket);

  free(temp);
  return 1;
}
