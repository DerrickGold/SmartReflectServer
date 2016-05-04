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

#include "misc.h"
#include "pluginSocket.h"
#include "plugin.h"
#include "display.h"
#include "api.h"


//#define TEST_FIFO "test.fifo"
#define COMS_DIR "com"
#define MAIN_COM "main.fifo"
#define INDEX_FILE "index.html"
#define JSLIBS_DIR "jsLibs"
#define CSS_DIR "css"

#define WEBSOCKET_PORT 5000

#define BUF_LEN 1024

#define HELP_TEXT\
 "\n%s: [-D] -d WEBFOLDER PLUGINFOLDER\n" \
 "\tRuns the Magic Mirror process from the specified WEBFOLDER and PLUGINFOLDER\n" \
 "\tArguments:\n" \
 "\t\t-D: Runs the magic mirror application as a background process\n" \
 "\t\t-d: defines the webfolder where plugins are located\n"

static char *prgmName = NULL;

static void mainDaemon() {

  pid_t pid;

  /* Fork off the parent process */
  pid = fork();

  /* An error occurred */
  if (pid < 0)
    exit(EXIT_FAILURE);

  /* Success: Let the parent terminate */
  if (pid > 0)
    exit(EXIT_SUCCESS);

  /* On success: The child process becomes session leader */
  if (setsid() < 0)
    exit(EXIT_FAILURE);

  signal(SIGCHLD, SIG_IGN);
  signal(SIGHUP, SIG_IGN);

  /* Fork off for the second time*/
  pid = fork();

  /* An error occurred */
  if (pid < 0)
    exit(EXIT_FAILURE);

  /* Success: Let the parent terminate */
  if (pid > 0)
    exit(EXIT_SUCCESS);

  /* Set new file permissions */
  umask(0);

  /* Change the working directory to the root directory */
  /* or another appropriated directory */
  char cwd[PATH_MAX];
  chdir(getcwd(cwd, sizeof(cwd)));

  /* Close all open file descriptors */
  int x;
  for (x = sysconf(_SC_OPEN_MAX); x > 0; x--) {
    close(x);
  }

}

/*
 * (PluginList_forEach) callback function.
 *
 * Adds a plugin's protocol to the websocket server, and
 * starts the plugin's schedule if applicable.
 */
static int _startPluginConnection(void *plug, void *data) {

  //First add the plugins protocol to the socket server instance.
  Plugin_t *plugin = (Plugin_t *) plug;

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

static int _initPluginFrontendCom(void *plug, void *data) {

  Plugin_t *plugin = (Plugin_t *) plug;
  return Display_LoadPlugin(plugin);
}

static int _disconnectPluginFrontendCom(void *plug, void *data) {

  Plugin_t *plugin = (Plugin_t *) plug;
  return Display_UnloadPlugin(plugin);
}

/*
 * When daemon is disconnected, reschedule all running plugins
 */
static int _rescheduleDisconnected(void *plug, void *data) {

  Plugin_t *plugin = (Plugin_t *) plug;
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
static int LoadPlugin(char *directory) {

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
  if (_startPluginConnection(plugin, NULL)) return 0;


  //if there is no connection to the browser, end here
  if (!Display_IsDisplayConnected()) return 0;

  //otherwise, send the plugin details to the browser so it can be
  //instantiated
  if (_initPluginFrontendCom(plugin, NULL)) return -1;

  return 0;
}


/*
 * [scanDirectory] callback function.
 * Initialize a plugin from a folder.
 */
static int initPlugin(char *filepath, struct dirent *dirInfo, void *data) {

  SYSLOG(LOG_INFO, "initPlugin: filepath: %s", filepath);
  return LoadPlugin(filepath);
}


static int initializeDaemon(char *localDir, char *pluginDir, int portNum) {

  /*
   * Retrieve the directory of where this binary (magic-mirror.bin) resides so we can
   * find its dependent directories relative from the binary location.
   */
  if (!localDir) {
    char rootPath[PATH_MAX];
    readlink("/proc/self/exe", rootPath, PATH_MAX);

    //trim off the binary name from the binary path
    char *dirEnd = strrchr(rootPath, '/');
    if (dirEnd) *dirEnd = '\0';
    SYSLOG(LOG_INFO, "Main: Binary location: %s", rootPath);

    //change directory to where the binary is
    chdir(rootPath);
  } else {

    //otherwise, a user has specified a running directory
    chdir(localDir);
  }

  //first, initialize plugin list
  if (PluginList_Init()) {
    SYSLOG(LOG_ERR, "Main: Error initializing plugin list structure.");
    return -1;
  }

  //initialize all the plugins in the plugin directory
  if (DirectoryAction(pluginDir, &initPlugin, NULL)) {
    SYSLOG(LOG_ERR, "Main: Error initializing plugins.");
    return -1;
  }

  Display_Generate(portNum, COMS_DIR, CSS_DIR, JSLIBS_DIR, INDEX_FILE);
  API_Init();

  //set socket server to send index page if necessary
  PluginSocket_ServeHtmlFile(INDEX_FILE);
  SYSLOG(LOG_INFO, "Main: creating socket");
  //initialize the websocket interface
  if (PluginSocket_Start(portNum)) {
    SYSLOG(LOG_ERR, "Main: Error starting socket server.");
    return -1;
  }

  return 0;
}


/*
 * Process things like:
 * -keeping the websocket server alive
 * -any daemon inputs from fifo file
 * -connection status of plugins
 * --pause plugins when disconnected from frontend and resume when reconnected...
 */
static int daemonProcess(void) {

  int oldWebStatus = 0, curWebStatus = 0;

  int pluginSent = 0;
  while (!API_Shutdown()) {
    //update any api pending actions
    API_Update();

    //keep polling to see if the daemon has a connection to the browser
    curWebStatus = Display_IsDisplayConnected();
    if (curWebStatus != oldWebStatus) {
      oldWebStatus = curWebStatus;

      if (curWebStatus) {
        SYSLOG(LOG_INFO, "Main: Daemon successfully connected to web front end.");
        if (!pluginSent) {
          printf("Connecting plugins\n");
          if (PluginList_ForEach(_initPluginFrontendCom, NULL)) {
            //assumes plugin loaded successfully until otherwise
            //error initializing stuff, unload the last send stuff
            PluginList_ForEach(_disconnectPluginFrontendCom, NULL);
            //reset connection status oldWebStatus != curWebStatus
            oldWebStatus = -1;
          } else
            pluginSent = 1;
        }
      }
      else {
        SYSLOG(LOG_INFO, "Main: Daemon disconnected from web front end.");
        //reschedule plugins so all the prior one-shot scripts will be updated
        PluginList_ForEach(_rescheduleDisconnected, NULL);
        pluginSent = 0;
      }
    }

    PluginSocket_Update();
  }

  //return error status here
  return 0;
}

static void printHelp() {

  printf(HELP_TEXT, prgmName);
}

int main(int argc, char *argv[]) {

  prgmName = basename(argv[0]);
  int c = 0, port = WEBSOCKET_PORT;
  char *runDir = NULL;

  while ((c = getopt(argc, argv, "hDp:j:d:")) != -1) {
    switch (c) {
      case 'h':
        printHelp();
        return EXIT_SUCCESS;
      case 'D':
        //calls mainDaemon to daemonize the magic-mirror process
        mainDaemon();
        break;
      case 'p':
        port = atoi(optarg);
        break;
      case 'd':
        runDir = optarg;
        break;
      default:
        break;
    }
  }

  char *filepath = NULL;
  if (optind < argc && optind > 0) {
    filepath = argv[optind];
  } else {
    SYSLOG(LOG_ERR, "Main: No Plugin folder specified.");
    return EXIT_FAILURE;
  }

  //All code after this is in background!
  //openlog("magic mirror", LOG_PID | LOG_CONS, LOG_USER);


  char *absPluginPath = realpath(runDir, NULL);
  //initialize the daemon and all the plugins
  if (initializeDaemon(absPluginPath, filepath, port)) {
    free(absPluginPath);
    return EXIT_SUCCESS;
  }

  free(absPluginPath);
  int prgmStatus = daemonProcess();

  //clean up...
  API_ShutdownPlugins();
  PluginSocket_Cleanup();
  Display_Cleanup();
  PluginList_Free();
  closelog();


  if (prgmStatus) return EXIT_FAILURE;
  return EXIT_SUCCESS;
}
