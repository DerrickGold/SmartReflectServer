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
#include <time.h>


#include "misc.h"
#include "pluginSocket.h"
#include "plugin.h"
#include "display.h"
#include "api.h"
#include "pluginLoader.h"

//one second in nanoseconds
#define SECOND 1000000000
#define DEFAULT_SLEEP_DIV 10000

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
 "\t\t-d: defines the webfolder where plugins are located\n" \
 "\t\t-p: Set what port to use for the server. Default is 5000\n" \
 "\t\t-s: Set number of cycles per second to run the server at. Default is 100.\n"

/*
 * Time in seconds it took the system to load all the plugins
 * and instantiate the socket server. This is used as an estimate
 * for determining how long the display and web gui should wait
 * before reloading their pages.
 */
unsigned int MainProgram_BootSeconds = 0;

/*
 * Store name of the binary here for use in the help text.
 */
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
 * [scanDirectory] callback function.
 * Initialize a plugin from a folder.
 */
static int initPlugin(char *filepath, struct dirent *dirInfo, void *data) {

  SYSLOG(LOG_INFO, "initPlugin: filepath: %s", filepath);
  return PluginLoader_LoadPlugin(filepath);
}

static int _initPluginFrontendCom(void *plug, void *data) {

  return PluginLoader_InitClient((Plugin_t *)plug);
}

static int _disconnectPluginFrontendCom(void *plug, void *data) {

  return PluginLoader_UnloadClient((Plugin_t *)plug);
}

static int _rescheduleDisconnected(void *plug, void *data) {

  return PluginLoader_RescheduleDisconnectedPlugin((Plugin_t *)plug);
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
  API_Init(pluginDir);

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
static int daemonProcess(unsigned int sleep) {

  int oldWebStatus = 0, curWebStatus = 0;
  int pluginSent = 0;

  //prevent division by 0
  sleep = (sleep == 0) ? DEFAULT_SLEEP_DIV : sleep;

  struct timespec sleepTime = {
      .tv_nsec=SECOND/sleep
  };

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
          SYSLOG(LOG_INFO, "Main: Connecting plugins\n");
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
    //sched_yield();
    nanosleep(&sleepTime, NULL);
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
  unsigned int sleepDivisor = DEFAULT_SLEEP_DIV;

  while ((c = getopt(argc, argv, "hDp:j:d:s:")) != -1) {
    switch (c) {
      case 'h':
        printHelp();
        return EXIT_SUCCESS;
      case 'D':
        //calls mainDaemon to daemonize the magic-mirror process
        mainDaemon();
        break;
      case 'p':
        port = strtol(optarg, NULL, 10);
        break;
      case 'd':
        runDir = optarg;
        break;
      case 's':
        sleepDivisor = strtol(optarg, NULL, 10);
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
  openlog("magic mirror", LOG_PID | LOG_CONS, LOG_USER);

  int prgmStatus = 0;
  char *runDirectory = realpath(runDir, NULL);
  do {
    unsigned int startTime = time(NULL);

    //initialize the daemon and all the plugins
    if (initializeDaemon(runDirectory, filepath, port)) {
      free(runDirectory);
      return EXIT_SUCCESS;
    }

    MainProgram_BootSeconds = (time(NULL) - startTime) + 1;
    SYSLOG(LOG_INFO, "Boot Time: %d", MainProgram_BootSeconds);

    prgmStatus = daemonProcess(sleepDivisor);

    //clean up...
    API_ShutdownPlugins();

    PluginSocket_Cleanup();
    Display_Cleanup();
    PluginList_Free();

  } while (API_Reboot());

  SYSLOG(LOG_INFO, "Main: hard shutdown");
  free(runDirectory);
  closelog();


  if (prgmStatus) return EXIT_FAILURE;
  return EXIT_SUCCESS;
}
