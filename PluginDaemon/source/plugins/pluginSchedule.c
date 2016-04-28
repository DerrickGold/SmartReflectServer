//
// Created by Derrick on 2016-03-18.
//
#include <stdio.h>
#include <syslog.h>
#include "plugin.h"
#include "scripts.h"
#include "misc.h"

//This method is called every time a plugin timer completes a period
//return -1 will delete this event from the scheduler, prevents it from triggering again
//return 0 will keep the event in the scheduler and will execute again at the next interval
static int _scheduleHandler(void *data) {

  Plugin_t *plugin = (Plugin_t *) data;
  if (!PLUGIN_DO_SCHEDULE(plugin)) {
    SYSLOG(LOG_ERR, "_scheduleHandler: Plugin in schedulerer when it shouldn't be!");
    return -1;
  }

  //if plugin is disabled, remove this plugin from the scheduler
  if (!Plugin_isEnabled(plugin)) {
    return -1;
  }

  //wait for websocket connection....
  if (!Plugin_isConnected(plugin)) {
    return 0;
  }

  //launch script in background if its enabled to do so
  if (plugin->flags & PLUGIN_FLAG_SCRIPT_BACKGROUND) {
    plugin->flags |= PLUGIN_FLAG_INBG;
    plugin->bgScriptPID = Script_ExecInBg(PluginConf_GetEscapeScript(plugin),
      Plugin_GetDaemonProtocol(plugin), PluginSocket_GetPort());

    //remove this event from scheduler, the script or program will drive this plugin
    return -1;
  }

  //otherwise, execute the script, and pass stdout to the browser
  char *stdoutBuf = Script_ExecGetSTDIO(PluginConf_GetEscapeScript(plugin));
  if (!stdoutBuf) {
    SYSLOG(LOG_INFO, "_scheduleHandler: stdoutBuf is null");
    return 0;
  }

  if (stdoutBuf) {

    if (PLUGIN_CLEAR_FIRST(plugin))
      Plugin_SendMsg(plugin, "clear", NULL);

    Plugin_SendMsg(plugin, "write", stdoutBuf);

    //once this is done, lets free the buffer
    free(stdoutBuf);
    stdoutBuf = NULL;
  }

  return -(plugin->flags & PLUGIN_FLAG_SCRIPT_ONESHOT);
}


/*
 * If the script requires scheduling, create the schedule and
 * set the proper flags to indicate so.
 *
 */
int Plugin_SetSchedule(Plugin_t *plugin) {

  if (!PluginConf_GetScript(plugin)) return -1;

  //set flag to indicate plugin has a script to run
  plugin->flags |= PLUGIN_FLAG_ISSCRIPT;

  //if script is a periodic script, set the continuous flag and
  //schedule an update
  if (PluginConf_GetScriptPeriod(plugin) > 0) {
    plugin->flags |= PLUGIN_FLAG_SCRIPT_CONTINUOUS;
    Plugin_ScheduleUpdate(plugin);
  }

  return 0;
}


int Plugin_ScheduleUpdate(Plugin_t *plugin) {

  if (!PLUGIN_DO_SCHEDULE(plugin)) {
    SYSLOG(LOG_ERR, "Plugin_ScheduleUpdate: Plugin does not have a script associated with it.");
    return -1;
  }


  //set up the data for the callback
  if (Scheduler_setCallback(&plugin->scheduler, _scheduleHandler, plugin)) {
    return -1;
  }

  //create the timer for the plugin
  if (Scheduler_createTimer(&plugin->scheduler, PluginConf_GetScriptPeriod(plugin))) {
    return -1;
  }

  //only set periodic scripts to update immediately when enabled.
  if (!(plugin->flags & PLUGIN_FLAG_SCRIPT_ONESHOT || plugin->flags & PLUGIN_FLAG_SCRIPT_BACKGROUND))
    Scheduler_setImmediateUpdate(&plugin->scheduler);


  return 0;
}


int Plugin_StartSchedule(Plugin_t *plugin) {

  if (!Plugin_isEnabled(plugin)) {
    SYSLOG(LOG_ERR, "Plugin_StartSchedule: Plugin %s is not enabled", Plugin_GetName(plugin));
    return -1;
  }

  if (!PLUGIN_DO_SCHEDULE(plugin)) {
    SYSLOG(LOG_ERR, "Plugin_StartSchedule: Plugin %s has no script to schedule", Plugin_GetName(plugin));
    return 0;
  }

  if (Scheduler_start(&plugin->scheduler)) {
    return -1;
  }

  return 0;
}

/*
 * Re-adds a plugin back into the scheduler to be processed again.
 */
int Plugin_ResetSchedule(Plugin_t *plugin) {

  //schedule related flags should remain intact if a plugin gets removed
  //from the scheduler because it is a oneshot script or background script that exited.
  if (!PLUGIN_DO_SCHEDULE(plugin)) {
    SYSLOG(LOG_ERR, "Plugin_ResetSchedule: Plugin %s has no script to schedule", Plugin_GetName(plugin));
    return -1;
  }

  //if plugin is a oneshot script, we can say it was running anyway to get it back into the scheduler and display
  int wasRunning = Scheduler_isTicking(&plugin->scheduler) || plugin->flags & PLUGIN_FLAG_SCRIPT_ONESHOT ||
                   plugin->bgScriptPID > 0;


  Plugin_StopSchedule(plugin);

  //recreate the schedule for the plugin
  Plugin_SetSchedule(plugin);

  //start the schedule if it was previously running
  if (wasRunning) Plugin_StartSchedule(plugin);
  return 0;
}

void Plugin_StopSchedule(Plugin_t *plugin) {

  //delete plugin from scheduler if it is currently scheduled
  if (Scheduler_isInitialized(&plugin->scheduler))
    Scheduler_delete(&plugin->scheduler);

  //stop any subprocesses associated with this plugin
  Plugin_StopBgScript(plugin);

}

