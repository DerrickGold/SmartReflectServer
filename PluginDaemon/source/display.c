/*====================================================================
  index.c:
  generates index.html file with all plugin .js, .css, and html files
  included/embeded
====================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <libgen.h>
#include <syslog.h>

#include "misc.h"
#include "plugin.h"
#include "pluginSocket.h"
#include "display.h"
#include "socketResponse.h"

#define SIZE_CMD "{\"cmd\":\"getsize\"}"

#define READABLE


#define INCLUDE_JS_START "<script src=\""
#define INCLUDE_JS_END "\"></script>"
#define INCLUDE_JS(path) INCLUDE_JS_START path INCLUDE_JS_END


#define INCLUDE_CSS_START "<link rel=\"stylesheet\" type=\"text/css\" href=\""
#define INCLUDE_CSS_END "\">"
#define INCLUDE_CSS(path) INCLUDE_CSS_START path INCLUDE_CSS_END


#define WINDOW_ONLOAD_START "<script type=\"text/javascript\">function mirrorSysInit() {"
#define WINDOW_ONLOAD_END "}; window.addEventListener('load', mirrorSysInit);</script>"


#define INIT_COMMUNICATIONS() \
  "var %sCom = new PluginClient(\"%s\", \"%s\");"

#define INIT_FRONTEND_PROTO \
  "var displayStuff = new Display(\"%s\", \"%s\");"


#define PLUGIN_CLIENT_JS "plugin-client.js"
#define DISPLAY_JS "display.js"

/*
 * Set up daemon-display communication protocol.
 * This protocol will allow the daemon to dynamically
 * load and remove plugins
 */

static SocketResponse_t displayResponse;

static int _displayConnected = 0;

static struct lws *displaySocketInstance = NULL;

static int _displayCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

struct lws_protocols mirrorStart = {
        .name=PLUGIN_SERVER_PROTOCOL, .callback=_displayCallback
};

const char *indexHeader =
        "<html>"
                "<head></head>"
                "<body>";

const char *indexFooter =
        "</body>"
                "</html>";


static char *includeJSString(char *path, char *file) {

  size_t bufLen = strlen(INCLUDE_JS_START) + strlen(path) + strlen(file)
                  + strlen("/") + 1 + strlen(INCLUDE_JS_END);

  char *filepath = calloc(bufLen, sizeof(char));
  if (!filepath) {
    SYSLOG(LOG_ERR, "includeJSString: Error allocating path for %s/%s", path, file);
    return NULL;
  }

  strncpy(filepath, INCLUDE_JS_START, bufLen);
  strncat(filepath, path, bufLen - strlen(filepath));
  strncat(filepath, "/", bufLen - strlen(filepath));
  strncat(filepath, file, bufLen - strlen(filepath));
  strncat(filepath, INCLUDE_JS_END, bufLen - strlen(filepath));

  return filepath;
}


static int doWrite(int fd, const char *buf, size_t len) {

  write(fd, buf, len);
#ifdef READABLE
  write(fd, "\n", strlen("\n"));
#endif

  return 0;
}

static int loadJSLib(char *filepath, struct dirent *dirInfo, void *data) {


  struct stat st;
  lstat(filepath, &st);

  //ignore directories
  if (S_ISDIR(st.st_mode)) return 0;

  //check file extension to make sure we are loading javascript files
  char *dot = strrchr(filepath, '.');
  //no file extension, exit
  if (!dot)
    return 0;
  //otherwise
  dot++;

  //extension does not match js
  if (strncmp(dot, "js", strlen(dot)))
    return 0;

  int fd = *(int *) data;

  size_t len = strlen(INCLUDE_JS_START) + strlen(INCLUDE_JS_END) +
               strlen(filepath) + 1;
  char buf[len];
  memset(buf, 0, sizeof(buf));

  snprintf(buf, len, INCLUDE_JS_START"%s"INCLUDE_JS_END, filepath);
  doWrite(fd, buf, strlen(buf));

  return 0;
}

static int loadCSSLib(char *filepath, struct dirent *dirInfo, void *data) {


  struct stat st;
  lstat(filepath, &st);

  //ignore directories
  if (S_ISDIR(st.st_mode)) return 0;

  //check file extension to make sure we are loading javascript files
  char *dot = strrchr(filepath, '.');
  //no file extension, exit
  if (!dot)
    return 0;
  //otherwise
  dot++;

  //extension does not match js
  if (strncmp(dot, "css", strlen(dot)))
    return 0;

  int fd = *(int *) data;

  size_t len = strlen(INCLUDE_CSS_START) + strlen(INCLUDE_CSS_END) +
               strlen(filepath) + 1;
  char buf[len];
  memset(buf, 0, sizeof(buf));

  snprintf(buf, len, INCLUDE_CSS_START"%s"INCLUDE_CSS_END, filepath);
  doWrite(fd, buf, strlen(buf));

  return 0;
}

void Display_Cleanup(void) {

  Display_ClearDisplayResponse();
  _displayConnected = 0;
  displaySocketInstance = NULL;
}


static int _displayCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {

  switch (reason) {

    case LWS_CALLBACK_SERVER_WRITEABLE: {

      if (!displaySocketInstance)
        return 0;

      PluginSocket_writeBuffers(displaySocketInstance);
      lws_callback_on_writable(displaySocketInstance);
    } break;


    case LWS_CALLBACK_ESTABLISHED:

      //_displayConnected = 1;
      displaySocketInstance = wsi;
      lws_callback_on_writable(displaySocketInstance);
      break;

    case LWS_CALLBACK_RECEIVE:

      SocketResponse_build(&displayResponse, wsi, (char *) in, len);
      if (SocketResponse_done(&displayResponse)) {
        char *socketResponse = SocketResponse_get(&displayResponse);
        size_t socketSize = SocketResponse_size(&displayResponse);

        if (!strncmp(socketResponse, "ready", socketSize)) {
          _displayConnected = 1;
          SYSLOG(LOG_INFO, "_displayCallback: Successfully connected to browser.");
          return 0;
        }
      }
      break;

    case LWS_CALLBACK_CLOSED:
      Display_ClearDisplayResponse();
      PluginSocket_clearWriteBuffers(displaySocketInstance, 0);
      displaySocketInstance = NULL;
      _displayConnected = 0;
      break;

    default:
      break;
  }

  return 0;
}

/*
 * Tell the index page to initialize a new PluginClient object
 * for use with a plugin.
 */
static int Display_BootstrapSocket(char *cmd, char *protocol, char *data) {

  //if no connection to the index page has been made, do nothing
  if (!Display_IsDisplayConnected()) return -1;

  //otherwise, tell the display to 'load' a plugin by initializing a new
  //PluginClient object for a plugin. The PluginClient requires a protocol name, and target
  //div to modify; these values are passed as pName and pDiv respectively.
  char msg[1024];
  if (data)
    snprintf(msg, sizeof(msg) - 1, "{\"cmd\":\"%s\",\"pName\":\"%s\",%s}", cmd, protocol, data);
  else
    snprintf(msg, sizeof(msg) - 1, "{\"cmd\":\"%s\",\"pName\":\"%s\"}", cmd, protocol);


  return Display_SendFrontendMsg(msg, -1);
}

/*
 * Initializes a plugin in the web frontend
 */
int Display_LoadPlugin(Plugin_t *plugin) {

  if (!plugin) {
    SYSLOG(LOG_ERR, "Display_LoadPlugin: Cannot load null plugin.");
    return -1;
  }

  if (!Plugin_isEnabled(plugin))
    return 0;

  if (!Display_IsDisplayConnected()) {
    SYSLOG(LOG_ERR, "Display_LoadPlugin: No browser connection established.");
    return -1;
  }

  char *protocol = Plugin_GetWebProtocol(plugin);
  char *name = Plugin_GetName(plugin);

  SYSLOG(LOG_INFO, "Display_LoadPlugin: Sending plugin %s to browser.", name);
  //the protocol for a given plugin between the daemon and the browser is just the plugins name
  char data[1024];
  snprintf(data, 1024, "\"pDiv\":\"%s\"", name);

  if (Display_BootstrapSocket("load", protocol, data) < 0) {
    SYSLOG(LOG_ERR, "Display_LoadPlugin: Error sending plugin to browser: %s", name);
    return -1;
  }

  //won't know if it loaded until it works at this point
  return 0;
}

int Display_UnloadPlugin(Plugin_t *plugin) {

  if (!plugin) {
    SYSLOG(LOG_ERR, "Display_UnloadPlugin: Cannot unload null plugin.");
    return -1;
  }

  if (!Plugin_isEnabled(plugin))
    return 0;

  if (Display_BootstrapSocket("unload", Plugin_GetWebProtocol(plugin), NULL) < 0) {
    SYSLOG(LOG_ERR, "Display_UnloadPlugin: Failed to unload plugin in browser.");
    return -1;
  }

  return 0;
}

int Display_GetDisplaySize(void) {

  if (!Display_IsDisplayConnected()) {
    SYSLOG(LOG_ERR, "Display_LoadPlugin: No browser connection established.");
    return -1;
  }


  Display_SendFrontendMsg(SIZE_CMD, -1);
  return 0;
}

int Display_Generate(int portNum, const char *comFolder, const char *cssFolder, const char *jsLibsFolder,
                     const char *output) {
  //delete old index
  unlink(output);

  PluginSocket_AddProtocol(&mirrorStart);

  //open html file to write to
  int fd = open(output, O_RDWR | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH);
  if (fd < 0) {
    SYSLOG(LOG_ERR, "Display_Generate: Error opening file: %s", output);
    return -1;
  }

  //write html header
  doWrite(fd, indexHeader, strlen(indexHeader));

  //include all global css files
  DirectoryAction((char *) cssFolder, loadCSSLib, &fd);

  //include the required plugin-client files
  char *comsJs = includeJSString((char *) comFolder, PLUGIN_CLIENT_JS);
  if (comsJs) {
    doWrite(fd, comsJs, strlen(comsJs));
    free(comsJs);
  }


  char *displayJs = includeJSString((char *) comFolder, DISPLAY_JS);
  if (displayJs) {
    doWrite(fd, displayJs, strlen(displayJs));
    free(displayJs);
  }


  //include any extra javascript libraries
  DirectoryAction((char *) jsLibsFolder, loadJSLib, &fd);

  //write communications initialization
  doWrite(fd, WINDOW_ONLOAD_START, strlen(WINDOW_ONLOAD_START));
  //doWrite(fd, INIT_FRONTEND_PROTO(), strlen(INIT_FRONTEND_PROTO()));

  char portStr[32];
  snprintf(portStr, sizeof(portStr) - 1, "%d", portNum);

  size_t dispBufLen = strlen(INIT_FRONTEND_PROTO) + strlen(PLUGIN_SERVER_PROTOCOL) +
                      strlen(portStr) + 1;

  char *dispBuf = calloc(dispBufLen, sizeof(char));
  if (!dispBuf) {
    SYSLOG(LOG_ERR, "Display_Generate: Error generating frontend protocol string.");
    close(fd);
    return -1;
  }
  snprintf(dispBuf, dispBufLen - 1, INIT_FRONTEND_PROTO, PLUGIN_SERVER_PROTOCOL, portStr);
  doWrite(fd, dispBuf, strlen(dispBuf));
  free(dispBuf);


  doWrite(fd, WINDOW_ONLOAD_END, strlen(WINDOW_ONLOAD_END));

  //write out html footer
  doWrite(fd, indexFooter, strlen(indexFooter));

  //close html file
  close(fd);
  return 0;
}

/*
 * Returns true/false indicator if the
 * daemon is connected to a display.
 */
int Display_IsDisplayConnected(void) {

  return _displayConnected && displaySocketInstance != NULL;
}

/*
* Send a message to the display manager itself (main.js)
*/
int Display_SendFrontendMsg(char *msg, size_t size) {

  if (!Display_IsDisplayConnected())
    return -1;

  return PluginSocket_writeToSocket(displaySocketInstance, msg, size, 0);
}

/*
 * Clear the last message received from the display.
 */
void Display_ClearDisplayResponse(void) {

  SocketResponse_free(&displayResponse);
}

/*
 * Retrieve the last message sent from the display.
 * Can be used to poll for a new message.
 */
char *Display_GetDisplayResponse(void) {

  return SocketResponse_get(&displayResponse);
}

