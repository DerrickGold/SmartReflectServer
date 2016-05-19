#ifndef MAGIC_MIRROR_INPUTREADER_H
#define MAGIC_MIRROR_INPUTREADER_H

#include "libwebsockets.h"

typedef enum {
    SUCCESS = 0,
    FAIL,
    PENDING,
    BUSY,
} APIStatus_e;

extern const char *API_STATUS_STRING[];


typedef enum {
    NO_ACTION = -1,
    LIST_CMDS,
    DISABLE,
    ENABLE,
    RELOAD,
    PLUGINS,
    PLUG_DIR,
    RM_PLUG,
    MIR_SIZE,
    STOP,
    SET_CSS,
    GET_CSS,
    DUMP_CSS,
    GET_STATE,
    JS_PLUG_CMD,
    DISP_CONNECT,
    GET_CONFIG,
    SET_CONFIG,
    ACTION_COUNT
} APIAction_e;


typedef struct API_Command_s {
    APIAction_e actionID;
    char *name;
    int flag;
} APICommand_t;

typedef enum {
    NONE = 0,
    NEED_PLUGIN = (1 << 0),
    NEED_VALUES = (1 << 1)
} APIActionFlags_e;

extern APICommand_t allActions[ACTION_COUNT];

extern void API_Init(void);

extern int API_Shutdown(void);

extern void API_Update(void);

extern void API_ShutdownPlugins();

extern int API_Parse(struct lws *socket, char *in, size_t len);

#endif //MAGIC_MIRROR_INPUTREADER_H


