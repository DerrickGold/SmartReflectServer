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


#define API_PROTO "STDIN"
#define API_PLUGLIST_DELIM "\n"

const char *API_STATUS_STRING[] = {
        "success",
        "fail",
        "pending",
        "busy"
};

static int _shutdown = 0;

static SocketResponse_t inputResponse;


APICommand_t allActions[ACTION_COUNT] = {
        {LIST_CMDS,    "commands",   NONE},
        /*
         * disable <pluginName>
         * Unloads a plugin from the frontend, unschedules the plugin
         * from updating, and stops any child processes of plugin.
         */
        {DISABLE,      "disable",    NEED_PLUGIN},

        /*
         * enable <pluginName>
         * Loads a plugin into the frontend and if applicable, enables
         * it in the main scheduler for providing display updates.
         */
        {ENABLE,       "enable",     NEED_PLUGIN},

        /*
         * reload <pluginName>
         * Reloads a plugins 'plugin.conf' file configuration,
         * unloads the plugin from the frontend and scheduler,
         * then reloads it back into the frontend and scheduler
         * with the newly loaded configuration setup.
         */
        {RELOAD,       "reload",     NEED_PLUGIN},

        /*
         * plugins
         * List all plugins currently loaded in the PluginDaemon.
         */
        {PLUGINS,      "list",       NONE},

        /*
         * getdir <pluginName>
         * Returns the filepath to the directory in which the
         * specified plugin's configuration file resides.
         */
        {PLUG_DIR,     "getdir",     NEED_PLUGIN},

        /*
         * rmplug <pluginName>
         * Completely unloads a plugin from the PluginDaemon.
         * After this is called, the PluginDaemon will be completely
         * unaware of this plugins existence.
         */
        {RM_PLUG,      "rmplug",     NEED_PLUGIN},

        /*
         * mirrorsize
         * Returns the frontend display dimensions as a string
         * formatted as 'widthxheight'.
         */
        {MIR_SIZE,     "mirrorsize", NONE},

        /*
         * stop
         * Stops the plugin daemon for a clean shutdown,
         */
        {STOP,         "stop",       NONE},

        /*
         * setopt <plugin> <option=value>
         * Write a new, or replace an option to a plugin's
         * 'plugin.conf' file. This change will require a
         * plugin to be reloaded to take effect.
         */
        {SET_OPT,      "setopt",     NEED_PLUGIN | NEED_VALUES},

        /*
         * getopt <plugin> <option>
         * Get the value of an option found in a plugin's
         * 'plugin.conf' file. Some options may return multiple
         * values (js-path and css-path), which will be separated
         * with a newline character.
         */
        {GET_OPT,      "getopt",     NEED_PLUGIN | NEED_VALUES},

        /*
         * setcss <plugin> <cssAttr=value;attr=value....>
         * Set a CSS value for a plugin's frontend
         * display.
         */
        {SET_CSS,      "setcss",     NEED_PLUGIN | NEED_VALUES},

        /*
         * getcss <plugin> <attr1, attr2,....>
         * Query the plugin's css attribute from the frontend.
         * If plugin is not loaded to frontend, nothing will
         * be returned. Multiple CSS properties can be queried
         * for and will be delimited with a newline character.
         */
        {GET_CSS,      "getcss",     NEED_PLUGIN | NEED_VALUES},


        {DUMP_CSS,     "savecss",    NEED_PLUGIN},

        /*
         * getstate <plugin>
         * Returns whether a plugin is enabled and running
         * or not.
         */
        {GET_STATE,    "getstate",   NEED_PLUGIN},

        /*
         * jscmd <plugin> {\"fn\":\"<FUNCTION>\",\"args\":<ARGS>}
         * If plugin has a javascript class instantiated, this will
         * send a command to that plugin to execute a function. The
         * string sent should be formatted as JSON with the 'fn' property
         * being the name of the function to execute, and 'args' property
         * being the arguments to provide to that function call.
         */
        {JS_PLUG_CMD,  "jscmd",      NEED_PLUGIN | NEED_VALUES},

        /*
         * display
         * Returns whether or not a display is connected to the daemon.
         */
        {DISP_CONNECT, "display",    NONE},

        /*
         * getcfg <plugin> <config_file_setting>
         * Returns a value in a plugins config file based
         * on the queried setting.
         */
        {GET_CONFIG,   "getcfg",     NEED_PLUGIN | NEED_VALUES},

        /*
         * setcfg <plugin> <config_file_setting>=<new_value>
         * Write a new entry or overwrite an existing entry in a plugin's
         * config file.
         */
        {SET_CONFIG,   "setcfg",     NEED_PLUGIN | NEED_VALUES}

};

//Returns an APIAction_e value for a given search string
static APIAction_e findAPICall(char *search) {

  APIAction_e action = NO_ACTION;
  for (int i = 0; i < ACTION_COUNT; i++) {
    if (strncmp(search, allActions[i].name, strlen(allActions[i].name)) == 0) {
      action = i;
      break;
    }
  }

  return action;
}


static int api_PluginEnable(Plugin_t *plugin) {

  Plugin_Enable(plugin);
  Display_LoadPlugin(plugin);
  SYSLOG(LOG_INFO, "API: Enabled plugin");
  return 0;
}

static int api_PluginDisable(void *plug, void *data) {

  Plugin_t *plugin = (Plugin_t *) plug;
  Display_UnloadPlugin(plugin);
  Plugin_Disable(plugin);
  SYSLOG(LOG_INFO, "API: Disabled plugin");
  return 0;
}


static int api_response(APIResponse_t *response, struct lws *wsi, Plugin_t *plugin, APIAction_e action, APIStatus_e status) {

  char *plugName = NULL;
  if (plugin)
    plugName = Plugin_GetName(plugin);

  return APIResponse_send(response, wsi, plugName, action, status);
}


static int _pluginList(void *plugin, void *data) {
  APIResponse_t *response = (APIResponse_t *)data;

  char *name = Plugin_GetName((Plugin_t *) plugin);
  APIResponse_concat(response, name, -1);
  APIResponse_concat(response, API_PLUGLIST_DELIM, 1);
  return 0;
}


/*
 * Sets an action to wait for a response from the daemon plugin communicator.
 */
static APIStatus_e api_waitForDaemonResponse(APIResponse_t *response, APIAction_e action, Plugin_t *plugin, struct lws *socket) {

  if (!Display_IsDisplayConnected()) {
    APIResponse_concat(response, "No display connected.", -1);
    return FAIL;
  }

  APIStatus_e rtrn = PENDING;
  if (!APIPending_addAction(APIPENDING_DISPLAY, action, plugin, socket)) {
    Display_ClearDisplayResponse();
  } else {
    rtrn = FAIL;
    APIResponse_concat(response, "Failed to add pending request.", -1);
  }

  return rtrn;
}

/*
 * Sets an action to wait for a response for a value obtained from
 * a plugin's frontend.
 */
static APIStatus_e api_waitForPluginResponse(APIResponse_t *response, APIAction_e action, Plugin_t *plugin, struct lws *socket) {

  if (!Plugin_isConnected(plugin)) {
    APIResponse_concat(response, "Plugin not connected to frontend", -1);
    return FAIL;
  }

  APIStatus_e rtrn = PENDING;
  if (!APIPending_addAction(APIPENDING_PLUGIN, action, plugin, socket)) {
    //Plugin_FreeFrontEndResponse(plugin);
    Plugin_ClientFreeResponse(plugin);
  } else {
    rtrn = FAIL;
    APIResponse_concat(response, "Failed to add pending request.", -1);
  }

  return rtrn;
}

//Write or replace a new value to a plugin's config file
static int api_writePluginSetting(APIResponse_t *response, Plugin_t *plugin, char *setting) {

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
static int api_getPluginSetting(APIResponse_t *response, Plugin_t *plugin, char *setting) {

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


static void _applyAction(struct lws *wsi, APIAction_e action, Plugin_t *plugin, char *value) {

  APIStatus_e status = SUCCESS;

  APIResponse_t *immResponse = APIResponse_new();
  if (!immResponse)
    return;

  if (allActions[action].flag & NEED_PLUGIN && plugin == NULL) {
    SYSLOG(LOG_INFO, "Action requires a plugin to be specified");
    status = FAIL;
    //PluginSocket_writeToSocket(wsi, API_FAIL"Action requires a plugin to be specified", -1);
    //return;
    APIResponse_concat(immResponse, "Action requires a plugin to be specified.", -1);
  }

  if (allActions[action].flag & NEED_VALUES && value == NULL) {
    SYSLOG(LOG_INFO, "Action requires values to be provided for plugin");
    status = FAIL;
    APIResponse_concat(immResponse, "Action requires values to be provided for plugin.", -1);
  }

  if (status != SUCCESS)
    goto _response;

  switch (action) {
    case NO_ACTION:
      PluginSocket_writeToSocket(wsi, "Specified action does not exist.", -1);
      //syslog(LOG_INFO, "Specified action does not exist.");
      break;
    case LIST_CMDS: {

      int i = 0;
      for (i = 0; i < ACTION_COUNT; i++) {
        APIResponse_concat(immResponse, allActions[i].name, -1);
        APIResponse_concat(immResponse, API_PLUGLIST_DELIM, 1);
      }
    }
      break;
    case DISABLE:
      if (api_PluginDisable(plugin, NULL))
        status = FAIL;
      break;
    case ENABLE:
      if (api_PluginEnable(plugin))
        status = FAIL;
      break;
    case RELOAD: {
      //reload plugin and reload plugin
      //syslog(LOG_INFO, "Reloaded plugin");
      int status = Plugin_isEnabled(plugin);
      //only disable and re-enable plugin on the frontend
      //if it was previously running
      if (status) api_PluginDisable(plugin, NULL);
      Plugin_Reload(plugin);
      if (status) api_PluginEnable(plugin);

    }
      break;
    case PLUGINS:
      //list plugins to fifo file
      //builds up the payload to send back
      PluginList_ForEach(_pluginList, immResponse);
      break;
    case PLUG_DIR:
      //get plugin's data directory
      APIResponse_concat(immResponse, Plugin_GetDirectory(plugin), -1);
      SYSLOG(LOG_INFO, "Got plugin directory");
      break;
    case RM_PLUG: {
      //if plugin is currently running, unload it first
      if (Plugin_isEnabled(plugin))
        api_PluginDisable(plugin, NULL);

      //set plugin name as payload since the name
      //will not be available after removing the plugin.
      APIResponse_concat(immResponse, Plugin_GetName(plugin), -1);
      //then delete it
      PluginList_Delete(Plugin_GetName(plugin));
      plugin = NULL;
      SYSLOG(LOG_INFO, "Unloaded plugin");
    }
      break;
    case MIR_SIZE: {
      if (Display_GetDisplaySize())
        status = FAIL;

      status = api_waitForDaemonResponse(immResponse, action, plugin, wsi);
    }
      break;
    case STOP:
      _shutdown = 1;
      APIResponse_concat(immResponse, "Shutting down daemon...", -1);
      SYSLOG(LOG_INFO, "Stopped mirror");
      break;
    case SET_OPT:
      //modify either the CSS path, the script path, execution timer, and renegerate index
      SYSLOG(LOG_INFO, "Set plugin attribute");
      if (api_writePluginSetting(immResponse, plugin, value))
        status = FAIL;
      break;
    case GET_OPT:
      //modify either the CSS path, the script path, execution timer, and renegerate index
      SYSLOG(LOG_INFO, "Get plugin attribute");
      if (api_getPluginSetting(immResponse, plugin, value))
        status = FAIL;

      break;
    case SET_CSS:
      SYSLOG(LOG_INFO, "Set plugin CSS");
      Plugin_SendMsg(plugin, "setcss", value);
      PluginCSS_store(plugin, value);
      break;
    case GET_CSS:
      //modify the css of the specified plugin and regenerate index
      //modify this css file with the values
      SYSLOG(LOG_INFO, "Modified plugin css");
      Plugin_SendMsg(plugin, "getcss", value);
      status = api_waitForPluginResponse(immResponse, action, plugin, wsi);
      break;

    case DUMP_CSS:
      SYSLOG(LOG_INFO, "Saving plugin's css");
      PluginCSS_dump(plugin);

      break;
    case GET_STATE: {
      SYSLOG(LOG_INFO, "Getting plugin status.");
      if (!Plugin_isEnabled(plugin))
        status = FAIL;
    }
      break;

    case JS_PLUG_CMD: {
      Plugin_SendMsg(plugin, "jsPluginCmd", value);
      status = api_waitForPluginResponse(immResponse, action, plugin, wsi);
      //WAIT_FOR_RESPONSE;
    }
      break;

      //check if there is a display connected
    case DISP_CONNECT:
      if (!Display_IsDisplayConnected())
        status = FAIL;
      break;

    case GET_CONFIG:
      if (api_getPluginSetting(immResponse, plugin, value))
        status = FAIL;
      break;

    case SET_CONFIG:
      if (api_writePluginSetting(immResponse, plugin, value))
        status = FAIL;

      break;

    default:
      break;
  }

  _response:
  //respond with the built payload for the given action
  api_response(immResponse, wsi, plugin, action, status);
  APIResponse_free(immResponse);
}


/*
 * Parses input given to standard input protocol and applys an action
 *
 * Inputs follow the form of:
 * <action>\n<plugin>\n<values>
 */
static void parseInput(char *input, size_t inputLen, struct lws *wsi) {

  int parsed = 0;
  char *inputEnd = input + inputLen;


  Plugin_t *plugin = NULL;
  char *cmd = input, *pluginName = NULL, *value = NULL;
  //first get the command string
  char *pos = cmd;
  //eliminate leading whitespace for the command
  while (pos < inputEnd && (*pos == ' ' || *pos == '\t'))
    pos++;

  //if we have hit the end of the input string, then no action was given
  if (pos >= inputEnd) return;

  cmd = pos;
  //find end of the commmand
  while (pos < inputEnd && *pos != '\n' && *pos != '\0')
    pos++;

  //hit end of input, there is nothing after the given command
  if (pos >= inputEnd - 1) parsed++;
  else {
    //otherwise, we have hit the end of the given command and more
    //input exists, so remove the deliminating the command, and
    //advance the pos ptr to the next input
    (*pos) = '\0';
    pos++;
  }
  SYSLOG(LOG_INFO, "Entered Command: %s", cmd);

  //check to see if the entered command is a valid API call
  APIAction_e action = findAPICall(cmd);

  //if all input has been consumed, try the action
  if (parsed)
    goto _doAction;

  SYSLOG(LOG_INFO, "Action Num: %d", action);
  //Otherwise, there is more to parse
  //string leading whitespace
  while (pos < inputEnd && (*pos == ' ' || *pos == '\t'))
    pos++;

  //incomplete plugin name...
  if (pos >= inputEnd)
    goto _doAction;

  //get plugin name
  pluginName = pos;
  //now remove the input delimiter
  while (pos < inputEnd && *pos != '\n')
    pos++;

  if (pos >= inputEnd) parsed++;
  else {
    *pos = '\0';
    pos++;
  }
  //attempt to find the plugin
  plugin = PluginList_Find(pluginName);

  //if all input has been consumed, try the action
  if (parsed)
    goto _doAction;

  //next, grab the input values, which can be anything
  value = pos;

  _doAction:
  //now check if plugin exists
  _applyAction(wsi, action, plugin, value);
}

/*
 * This callback handler is for handling external applications -> this daemon information. This daemon will
 * then forward messages to the proper frontend interface.
 */
static int API_Callback(struct lws *wsi, websocket_callback_type reason, void *user, void *in, size_t len) {


  struct lws_protocols *proto = NULL;
  if (wsi) proto = (struct lws_protocols *) lws_get_protocol(wsi);

  switch (reason) {
    case LWS_CALLBACK_ESTABLISHED:
      SYSLOG(LOG_INFO, "InputReader connection established established[%s]", proto->name);
      break;

    case LWS_CALLBACK_RECEIVE: {

      if (!len)
        return 0;

      SYSLOG(LOG_INFO, "InputReader received command");

      SocketResponse_build(&inputResponse, wsi, (char *) in, len);
      if (SocketResponse_done(&inputResponse)) {
        //wait for full message before parsing input
        if (proto) {
          parseInput(SocketResponse_get(&inputResponse),
                     SocketResponse_size(&inputResponse), wsi);

        }
        SocketResponse_free(&inputResponse);
      }
    } break;

    case LWS_CALLBACK_CLOSED:
      SYSLOG(LOG_INFO, "InputReader disconnect[%s]", proto->name);
      SocketResponse_free(&inputResponse);
      break;

    default:
      break;
  }

  return 0;
}

void API_ShutdownPlugins() {

  PluginList_ForEach(api_PluginDisable, NULL);
  PluginSocket_Update();
  SocketResponse_free(&inputResponse);
}

void API_Init(void) {

  //create websocket protocol for standard input
  struct lws_protocols proto = {};
  //replace the old _arrayTerminate protocol with an actual protocol
  proto.name = API_PROTO;
  proto.callback = &API_Callback;
  proto.rx_buffer_size = PLUGIN_RX_BUFFER_SIZE;
  PluginSocket_AddProtocol(&proto);
  SocketResponse_free(&inputResponse);
  APIPending_init();
}

//API Update loop for processing pending actions
//Pending actions will block other api calls
void API_Update(void) {

  APIPending_update();
}


int API_Shutdown(void) {

  return _shutdown;
}
