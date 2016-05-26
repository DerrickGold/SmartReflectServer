//
// Created by Derrick on 2016-05-26.
//
#include <stdlib.h>
#include <syslog.h>
#include <libgen.h>
#include <sys/stat.h>
#include <string.h>

#include "pluginLoader.h"
#include "display.h"
#include "misc.h"

#define INSTALL_CMD "cd %s && git clone -q %s"


/*
 * (PluginList_forEach) callback function.
 *
 * Adds a plugin's protocol to the websocket server, and
 * starts the plugin's schedule if applicable.
 */
int PluginLoader_InitSocketConnection(Plugin_t *plugin) {

  //First add the plugins protocol to the socket server instance.
  //Create the interface between this daemon and the plugin's web interface
  struct lws_protocols proto = Plugin_MakeProtocol(plugin);
  PluginSocket_AddProtocol(&proto);

  //Add a handler for external applications to interact with plugins
  struct lws_protocols extProto = Plugin_MakeExternalProtocol(plugin);
  PluginSocket_AddProtocol(&extProto);

  //Next, start the plugin schedule so it can search/wait for a socket connection
  if (!Plugin_StartSchedule(plugin))
    SYSLOG(LOG_INFO, "_startPluginSchedule: starting timer for:%s", Plugin_GetName(plugin));
  else
    return -1;

  return 0;
}



int PluginLoader_InitClient(Plugin_t *plugin) {

  return Display_LoadPlugin(plugin);
}


int PluginLoader_UnloadClient(Plugin_t *plugin) {

  return Display_UnloadPlugin(plugin);
}

/*
 * When daemon is disconnected, reschedule all running plugins
 */
int PluginLoader_RescheduleDisconnectedPlugin(Plugin_t *plugin) {

  //plugin is unloaded from frontend on disconnect
  plugin->flags &= ~PLUGIN_FLAG_LOADED;
  Plugin_ResetSchedule(plugin);
  return 0;
}

/*
 * Loads a plugin from a directory to the mirror.
 * This should be called after a connection has been made to
 * the mirror, and handles all the initial setup required to load
 * a plugin on the fly
 */
int PluginLoader_LoadPlugin(char *directory) {
  SYSLOG(LOG_INFO, "Loading Directory: %s", directory);

  struct stat st;
  lstat(directory, &st);

  //must pass in a plugin directory
  if (!S_ISDIR(st.st_mode)) {
    SYSLOG(LOG_ERR, "LoadPlugin: Skipped regular file, requires directory");
    return 0;
  }

  char *dirName = basename(directory);

  Plugin_t *plugin = Plugin_Init(directory, dirName);

  if (!plugin) {
    SYSLOG(LOG_ERR, "LoadPlugin: Error initializing plugin.");
    return -1;
  }

  //Add plugin to the plugin list
  PluginList_Add(plugin);

  //start communications
  if (PluginLoader_InitSocketConnection(plugin)) return 0;


  //if there is no connection to the browser, end here
  if (!Display_IsDisplayConnected()) return 0;

  //otherwise, send the plugin details to the browser so it can be
  //instantiated
  if (PluginLoader_InitClient(plugin))
    return -1;

  return 0;
}

int PluginLoader_InstallPlugin(char *destDirectory, char *url) {


  SYSLOG(LOG_INFO, "Install Plugin Not Implemented Yet.");

  char command[PATH_MAX];
  snprintf(command, PATH_MAX, INSTALL_CMD, destDirectory, url);
  int status = system(command);

  if (status)
    return status;

  return 0;
}

