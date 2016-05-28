#ifndef MAGIC_MIRROR_INPUTREADER_H
#define MAGIC_MIRROR_INPUTREADER_H

#include "libwebsockets.h"

typedef enum {
    API_STATUS_SUCCESS = 0,
    API_STATUS_FAIL,
    API_STATUS_PENDING,
    API_STATUS_BUSY,
} APIStatus_e;

extern const char *API_STATUS_STRING[];


typedef enum {
    API_NO_ACTION = -1,
    API_LIST_CMDS,
    API_DISABLE,
    API_ENABLE,
    API_RELOAD,
    API_PLUGINS,
    API_PLUG_DIR,
    API_RM_PLUG,
    API_MIR_SIZE,
    API_STOP,
    API_SET_CSS,
    API_GET_CSS,
    API_DUMP_CSS,
    API_GET_STATE,
    API_JS_PLUG_CMD,
    API_DISP_CONNECT,
    API_GET_CONFIG,
    API_SET_CONFIG,
    API_INSTALL,
    API_REBOOT,
    API_ACTION_COUNT
} APIAction_e;


typedef struct API_Command_s {
    char *name;
    int flag;
} APICommand_t;

typedef enum {
    NONE = 0,
    NEED_PLUGIN = (1 << 0),
    NEED_VALUES = (1 << 1)
} APIActionFlags_e;

extern APICommand_t allActions[API_ACTION_COUNT];

extern void API_Init(char *pluginDir);

extern int API_Shutdown(void);

extern int API_Reboot(void);

extern void API_Update(void);

extern void API_ShutdownPlugins();

extern int API_Parse(struct lws *socket, char *in, size_t len);

#endif //MAGIC_MIRROR_INPUTREADER_H


