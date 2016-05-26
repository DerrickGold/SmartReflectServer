//
// Created by Derrick on 2016-05-26.
//

#ifndef SMARTREFLECT_PLUGINLOADER_H
#define SMARTREFLECT_PLUGINLOADER_H

#include "plugin.h"

extern int PluginLoader_InitSocketConnection(Plugin_t *plugin);

extern int PluginLoader_InitClient(Plugin_t *plugin);

extern int PluginLoader_UnloadClient(Plugin_t *plugin);

extern int PluginLoader_RescheduleDisconnectedPlugin(Plugin_t *plugin);

extern int PluginLoader_LoadPlugin(char *directory);

extern int PluginLoader_InstallPlugin(char *destDirectory, char *url);



#endif //SMARTREFLECT_PLUGINLOADER_H
