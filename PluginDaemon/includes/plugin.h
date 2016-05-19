#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include <unistd.h>
#include <libwebsockets.h>
#include <uuid/uuid.h>
#include "scheduler.h"
#include "pluginSocket.h"
#include "hashtable.h"
#include "socketResponse.h"


#define PLUGIN_DO_RENDER(plugin) (plugin->flags & PLUGIN_FLAG_RENDER)
#define PLUGIN_DO_SCRIPT_UPDATE(plugin) (plugin->flags & PLUGIN_FLAG_ISSCRIPT && PluginConf_GetScript(plugin) != NULL)
#define PLUGIN_DO_SCHEDULE(plugin) (PLUGIN_DO_SCRIPT_UPDATE(plugin) && plugin->flags & PLUGIN_FLAG_SCRIPT_CONTINUOUS)
#define PLUGIN_CLEAR_FIRST(plugin) (plugin->flags & PLUGIN_FLAG_OUTPUT_CLEAR)

#define PLUGIN_SET_ENABLED(plugin) (plugin->flags |= PLUGIN_FLAG_RENDER)
#define PLUGIN_SET_DISABLED(plugin) (plugin->flags &= ~PLUGIN_FLAG_RENDER)


#define PLUGIN_CONF_OPT_TRUE "true"
#define PLUGIN_CONF_OPT_FALSE "false"

#define PLUGIN_CONF_FILENAME "plugin.conf"
#define PLUGIN_CONF_OUTFILE "plugin.new.conf"
#define PLUGIN_CONF_FILE "/" PLUGIN_CONF_FILENAME


#define PLUGIN_CONF_TAG_HTML "html-path"
#define PLUGIN_CONF_TAG_JS "js-path:"
#define PLUGIN_CONF_TAG_JS_CLASS "js-main-obj"
#define PLUGIN_CONF_TAG_CSS "css-path:"
#define PLUGIN_CONF_TAG_SCRIPT "script-path"
#define PLUGIN_CONF_TAG_SCRIPT_ESCAPED "script-path:escaped"
#define PLUGIN_CONF_TAG_SCRIPT_TIME "script-timer"
#define PLUGIN_CONF_TAG_SCRIPT_PROCESS "script-process"
#define PLUGIN_CONF_TAG_SCRIPT_BACKGROUND "script-background"
#define PLUGIN_CONF_TAG_SCRIPT_PROCESS_1 "append"
#define PLUGIN_CONF_TAG_SCRIPT_PROCESS_2 "clear"
#define PLUGIN_CONF_START_ON_LOAD "start-on-load"
#define PLUGIN_CONF_DESCRIPTION "description"
#define PLUGIN_CONF_WEBGUI "webgui-html"


#define PLUGIN_UUID_LEN 38
#define PLUGIN_UUID_SHORT_LEN 32


typedef struct PluginConf_s {

    //plugin config stored in hash table
    HashTable_t *table;

    //Materialized Plugin Conf attributes
    //eventually plan on deprecating the initialization
    //of these so that pointers point to their hash
    //table entry
    //integer values will remain materialized

    //update period length in seconds
    int periodLen;
} PluginConf_t;


typedef struct Plugin_s {
    //plugin name based on the plugin's directory name
    char *name;
    //full path of plugin directory
    char *basePath;

    int flags;

    char uuid[PLUGIN_UUID_LEN];
    //uuidShort is the longest length a websocket protocol can be (32 characters)
    char uuidShort[PLUGIN_UUID_SHORT_LEN];

    //scheduler data
    Schedule_t scheduler;

    //pid for bg script
    pid_t bgScriptPID;

    //size of last frontend response message
    SocketResponse_t clientResponse;
    SocketResponse_t externResponse;

    //socket instances
    struct lws *socketInstance;
    struct lws *externSocketInstance;

    //hash table for storing css data
    HashTable_t *cssAttr;

    PluginConf_t config;

    int writeable;
} Plugin_t;

typedef enum {
    PLUGIN_FLAG_RENDER = (1 << 0),
    PLUGIN_FLAG_ISSCRIPT = (1 << 1),
    PLUGIN_FLAG_OUTPUT_APPEND = (1 << 2),
    PLUGIN_FLAG_OUTPUT_CLEAR = (1 << 3),
    PLUGIN_FLAG_SCRIPT_CONTINUOUS = (1 << 4),
    PLUGIN_FLAG_SCRIPT_ONESHOT = (1 << 5),
    PLUGIN_FLAG_SCRIPT_BACKGROUND = (1 << 6),
    PLUGIN_FLAG_LOADED = (1 << 7),
    PLUGIN_FLAG_INBG = (1 << 8),
} PluginFlags_e;


extern Plugin_t *Plugin_Create(void);

extern Plugin_t *Plugin_Init(const char *path, const char *pluginName);

extern void Plugin_Reload(Plugin_t *plugin);

extern char Plugin_Exists(Plugin_t *plugin);

extern void Plugin_Free(Plugin_t *plugin, int freeContainer);

extern void Plugin_Print(Plugin_t *plugin);

extern char *Plugin_GetName(Plugin_t *plugin);

extern int Plugin_SetName(Plugin_t *plugin, const char *name);

extern char *Plugin_GetID(Plugin_t *plugin);


extern char *Plugin_GetDirectory(Plugin_t *plugin);

extern char *PluginConf_GetScript(Plugin_t *plugin);

extern char *Plugin_GetWebProtocol(Plugin_t *plugin);

extern char *Plugin_GetDaemonProtocol(Plugin_t *plugin);


extern int Plugin_SendMsg(Plugin_t *plugin, char *command, char *data);


extern void Plugin_ClientFreeResponse(Plugin_t *plugin);

extern int Plugin_ClientSaveResponse(Plugin_t *plugin, char *response, size_t len);

extern void Plugin_ClientSetResponseDone(Plugin_t *plugin);

extern char Plugin_ClientResponseDone(Plugin_t *plugin);

extern char *Plugin_ClientGetResponse(Plugin_t *plugin);

extern size_t Plugin_ClientGetResponseSize(Plugin_t *plugin);


extern int Plugin_ResetSchedule(Plugin_t *plugin);

extern int Plugin_SetSchedule(Plugin_t *plugin);

extern int Plugin_ScheduleUpdate(Plugin_t *plugin);

extern int Plugin_StartSchedule(Plugin_t *plugin);

extern void Plugin_StopSchedule(Plugin_t *plugin);

extern int Plugin_isEnabled(Plugin_t *plugin);

extern int Plugin_isConnected(Plugin_t *plugin);

extern void Plugin_Enable(Plugin_t *plugin);

extern void Plugin_Disable(Plugin_t *plugin);

extern int Plugin_confApply(void *data, char *property, char *value);

extern int Plugin_loadConfig(Plugin_t *plugin);

extern int Plugin_isFrontendLoaded(Plugin_t *plugin);

extern void Plugin_UnloadFrontEnd(Plugin_t *plugin);

extern void Plugin_LoadFrontend(Plugin_t *plugin);

extern void Plugin_StopBgScript(Plugin_t *plugin);


extern struct lws_protocols Plugin_MakeProtocol(Plugin_t *plugin);

extern struct lws_protocols Plugin_MakeExternalProtocol(Plugin_t *plugin);


extern int PluginConf_setValue(Plugin_t *plugin, char *property, char *value);

extern char *PluginConf_GetHTML(Plugin_t *plugin);

extern char **PluginConf_GetCSS(Plugin_t *plugin, int *count);

extern char **PluginConf_GetJS(Plugin_t *plugin, int *count);

extern char *PluginConf_GetEscapeScript(Plugin_t *plugin);

extern int PluginConf_SetJSMain(Plugin_t *plugin, char *mainObj);

extern char *PluginConf_GetJSMain(Plugin_t *plugin);

extern int PluginConf_GetScriptPeriod(Plugin_t *plugin);

extern char **PluginConf_GetConfigValue(Plugin_t *plugin, char *property, int *count);


extern void PluginCSS_store(Plugin_t *plugin, char *cssValString);

extern void PluginCSS_dump(Plugin_t *plugin);

extern void PluginCSS_load(Plugin_t *plugin);

extern int PluginCSS_sendAll(Plugin_t *plugin);


//extern int Plugin_SocketCallback(struct lws *wsi, websocket_callback_type reason, void *user,  void *in, size_t len);

//function ptr for operating on each node
extern int PluginList_Init(void);

extern void PluginList_Add(Plugin_t *plugin);

extern Plugin_t *PluginList_Find(const char *pluginName);

extern void PluginList_Delete(const char *pluginName);

extern void PluginList_Free(void);

extern int PluginList_ForEach(int (*operation)(void *, void *), void *data);

extern int PluginList_GetCount(void);


#endif
