//
// Created by Derrick on 2016-01-26.
//

#ifndef MAGICMIRROR_PLUGINSOCKET_H
#define MAGICMIRROR_PLUGINSOCKET_H

#include <libwebsockets.h>

#define PLUGIN_RX_BUFFER_SIZE 4096

#define PLUGIN_SERVER_PROTOCOL "PluginServer"


typedef enum lws_callback_reasons websocket_callback_type;

extern void PluginSocket_FreeProtocolList(void);

extern struct lws_protocols *PluginSocket_getProtocol(char *name);

extern int PluginSocket_AddProtocol(struct lws_protocols *proto);

extern void PluginSocket_RemoveProtocol(char *protocolName);

extern int PluginSocket_Start(int port);

extern int PluginSocket_GetPort(void);

extern void PluginSocket_Update(void);

extern void PluginSocket_Cleanup(void);

extern int PluginSocket_writeToSocket(struct lws *wsi_in, char *str, int str_size_in, char noHeader);

extern void PluginSocket_writeBuffers(struct lws *wsi);

extern void PluginSocket_clearWriteBuffers(struct lws *wsi);

extern char PluginSocket_ServeHtmlFile(char *htmlPath);

extern char PluginSocket_SetComDir(char *comDir);


#endif //MAGICMIRROR_PLUGINSOCKET_H
