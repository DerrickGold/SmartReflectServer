//
// Created by Derrick on 2016-01-26.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <libwebsockets.h>

#include "pluginSocket.h"
#include "misc.h"

#define INDEX_PATH "/"
#define GUI_PATH "/gui"


#define HTTP_SERVER_PROTOCOL "HTTP"
#define SOCKET_TIMEOUT 0
#define BASE_PROTO_POOL 3
#define NUM_BUFFERED_WRITES 256

/*
 * Main context for the socket server
 */
struct lws_context *_context = NULL;

typedef struct BufferedWrite_s {
    struct lws *socket;
    void *msg;
    size_t len;
} BufferedWrite_t;

typedef struct WriteBuffers_s {
    BufferedWrite_t writes[NUM_BUFFERED_WRITES];
    size_t lastBuffered, lastWritten;
} WriteBuffers_t;


static WriteBuffers_t WriteBuffer = {
        .lastBuffered = 0,
        .lastWritten = 0
};

static int portNumber = -1;


static void bufferSocketWrite(struct lws *socket, void *msg, size_t len) {

  BufferedWrite_t *curWrite = &WriteBuffer.writes[WriteBuffer.lastBuffered];
  if (curWrite->socket) {
    SYSLOG(LOG_ERR, "WRITING OVER BUFFERED MSG");
    free(curWrite->msg);
  }
  curWrite->socket = socket;
  curWrite->msg = msg;
  curWrite->len = len;

  WriteBuffer.lastBuffered++;
  WriteBuffer.lastBuffered %= NUM_BUFFERED_WRITES;
}

static void flushSocketWrites(void) {

  int slot = WriteBuffer.lastWritten;

  while (!WriteBuffer.writes[slot].socket || lws_partial_buffered(WriteBuffer.writes[slot].socket)) {
    slot++;
    slot %= NUM_BUFFERED_WRITES;

    WriteBuffer.lastWritten = slot;

    if (slot == WriteBuffer.lastBuffered)
      break;
  }

  BufferedWrite_t *curWrite = &WriteBuffer.writes[slot];
  if (curWrite->socket) {
    lws_write(curWrite->socket, curWrite->msg + LWS_SEND_BUFFER_PRE_PADDING, curWrite->len, LWS_WRITE_TEXT);
    free(curWrite->msg);
    curWrite->msg = NULL;
    curWrite->socket = NULL;
  }
}


/*
 * Dynamically allocated list of protocols.
 * Grows as more protocols are added.
 */
static int _protocolCount = 0;
static int _lastProtocol = 0;
struct lws_protocols *_protocols = NULL;

/*
 * protocol list must end with a protocol
 * that has a null callback
 */
struct lws_protocols listTerminator = {
        .name=NULL, .callback=NULL
};


/*
 * Set up html file serving.
 */
static char *_htmlPath = NULL;
static char *_basePath = NULL;

static int _httpServerCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

struct lws_protocols httpServer = {
        .name=HTTP_SERVER_PROTOCOL, .callback=_httpServerCallback
};


static char *allocStr(char *input) {

  char *newStr = calloc(strlen(input) + 1, sizeof(char));
  if (!newStr) {
    SYSLOG(LOG_ERR, "PluginSocket_allocStr: Error allocating file path: %s.", input);
    return NULL;
  }

  strncpy(newStr, input, strlen(input));
  return newStr;
}

/*
 * Creates a basic http server to serve resource files.
 * These files includes scripts, html, css, and image files
 * required by plugins. This allows the display client (browser)
 * to load via a url (127.0.0.1:<port number>) rather than
 * pointing it to the file location of the generated "index.html".
 */
static int _httpServerCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {

  switch (reason) {
    default:
      break;
    case LWS_CALLBACK_HTTP_FILE_COMPLETION:
      SYSLOG(LOG_INFO, "HTTP File completely sent\n");
      return lws_http_transaction_completed(wsi);


    case LWS_CALLBACK_HTTP:
      SYSLOG(LOG_INFO, "CALLBACK: HTTP");
      if (!_htmlPath) {
        SYSLOG(LOG_ERR, "_httpServerCallback: No html file specified to serve.");
        return -1;
      }

      return PluginSocket_serveHttp(wsi, (char *) in, len);
  }

  return 0;
}


/*
 * Sends a requested file if available with the proper mime
 * type.
 */
int PluginSocket_serveHttp(struct lws *wsi, char *file, size_t len) {

  char *requested_uri = (char *) file;
  char resource_path[1024];

  snprintf(resource_path, 1024, ".%s", requested_uri);
  SYSLOG(LOG_INFO, "HTTP Requested: %s", resource_path);

  //redirect "/" to "/index.html"
  if (!strncmp(requested_uri, INDEX_PATH, len)) {
    SYSLOG(LOG_INFO, "Sending html file\n");
    SYSLOG(LOG_INFO, "HTML PATH: %s", _htmlPath);
    return lws_serve_http_file(wsi, _htmlPath, "text/html", NULL, 0);
  }
  //add a redirect for /gui to /webui/gui.html
  //else if (!strncmp(requested_uri, GUI_PATH, len)) {
  //snprintf(resource_path, 1024, "./webgui/gui.html");
  //  return lws_serve_http_file (wsi, resource_path, "text/html", NULL, 0);
  // }

  char *extension = strrchr(resource_path, '.');
  char *mime;

  SYSLOG(LOG_INFO, "HTTP Requested: %s", resource_path);

  // choose mime type based on the file extension
  if (extension == NULL) {
    mime = "text/plain";
  } else if (strcmp(extension, ".png") == 0) {
    mime = "image/png";
    //return 1;
  } else if (strcmp(extension, ".jpg") == 0) {
    mime = "image/jpg";
  } else if (strcmp(extension, ".gif") == 0) {
    mime = "image/gif";
  } else if (strcmp(extension, ".html") == 0) {
    mime = "text/html";
  } else if (strcmp(extension, ".css") == 0) {
    mime = "text/css";
  } else if (strcmp(extension, ".js") == 0) {
    mime = "text/javascript";
  } else {
    mime = "text/plain";
  }

  return lws_serve_http_file(wsi, resource_path, mime, NULL, 0);
}

/*============================================================================================
 * Protocol List
 ===========================================================================================*/

/*
 * Clears all memory used by the protocol list
 */
void PluginSocket_FreeProtocolList(void) {

  if (!_protocols) return;

  free(_protocols);
  _protocols = NULL;
  _protocolCount = 0;
  _lastProtocol = 0;
}

static void printProtocols(void) {

  for (int i = 0; i < _protocolCount; i++) {
    SYSLOG(LOG_INFO, "Protocol: %s", _protocols[i].name);
  }
}

/*
 * Returns a protocol instance via the protocols name.
 */
struct lws_protocols *PluginSocket_getProtocol(char *name) {

  int i = 0;
  for (i = 0; i < _protocolCount; i++) {
    const char *target = _protocols[i].name;

    if (!strncmp(name, target, strlen(target)))
      return &_protocols[i];
  }

  return NULL;
}

/*
 * Add a protocol to the list.
 *
 * By default, the httpServer, display, and listTerminator
 * protocols are added to the list.
 *
 * The first protocol must be the httpServer protocol, as
 * per libwebsocket documentation--all http requests are
 * directed to the first protocol in the list. This function
 * ensures the proper order is maintained.
 *
 * The last protocol must always have a null callback set.
 * This is how libwebsockets determines that it has reached
 * the end of the protocol list array since no size can be
 * specified.
 *
 * When adding a protocol, this function will always ensure
 * that the listTerminator protocol will be at the end of
 * the list. New entries will be placed after all previous
 * entries, but before the listTerminator.
 */
int PluginSocket_AddProtocol(struct lws_protocols *proto) {

  if (_lastProtocol >= _protocolCount) {
    int start = 0;
    if (!_protocolCount) {
      _protocolCount = BASE_PROTO_POOL;
      start = 1;
    }

    struct lws_protocols *protos = realloc(_protocols, sizeof(struct lws_protocols) * (_protocolCount + 1));
    if (!protos) {
      SYSLOG(LOG_ERR, "PluginSocket_MakeProtocol: Error reallocating protocol list: count: %d", _protocolCount);
      return -1;
    }

    _protocols = protos;
    _protocolCount++;
    SYSLOG(LOG_INFO, "Addprotocol: resized pool %d", _protocolCount);

    //add starting protocol
    if (start) {
      PluginSocket_AddProtocol(&httpServer);
    }
  }

  //replace the null entry if we are adding onto the list after the context has been
  //created.
  int addEnd = 0;
  if (_lastProtocol > 0 && _protocols[_lastProtocol - 1].callback == NULL) {
    addEnd++;
    _lastProtocol--;
  }


  //replace the old _arrayTerminate protocol with an actual protocol

  _protocols[_lastProtocol].name = proto->name;
  _protocols[_lastProtocol].callback = proto->callback;
  _protocols[_lastProtocol].user = proto->user;
  _protocols[_lastProtocol].rx_buffer_size = 0;
  _protocols[_lastProtocol].per_session_data_size = proto->per_session_data_size;

  _lastProtocol++;

  if (addEnd) PluginSocket_AddProtocol(&listTerminator);

  //return success
  return 0;
}

/*
 * Remove a protocol from the list.
 */
void PluginSocket_RemoveProtocol(char *protocolName) {

  int pos = -1;
  for (int i = 0; i < _lastProtocol; i++) {
    //once we've found the protocol to replace, mark its position, then continue
    if (pos == -1 && !strcmp(_protocols[i].name, protocolName)) {
      pos = i;
      continue;
    } else if (pos > -1) {

      //i should be ahead of pos now, so we can shift the rest of the protocol
      //list overtop of the protocol we are removing
      memcpy(&_protocols[pos++], &_protocols[i], sizeof(struct lws_protocols));
    }
  }

  //update _lastProtocol count
  _lastProtocol -= (pos > -1);
}

/*
 * Updates the socket service. Processes
 * all sockets.
 */
void PluginSocket_Update(void) {

  lws_service(_context, SOCKET_TIMEOUT);
  flushSocketWrites();
}

/*
 * Write to a target socket.
 *
 * All messages must have pre-padding as defined by
 * LWS_SEND_BUFFER_PRE_PADDING. This function will
 * apply said padding to each message sent.
 */
int PluginSocket_writeToSocket(struct lws *wsi_in, char *str, int str_size_in) {

  if (str == NULL || wsi_in == NULL)
    return -1;

  int n = 0;
  int len;
  unsigned char *out = NULL;

  if (str_size_in < 1)
    len = strlen(str);
  else
    len = str_size_in;

  out = malloc(sizeof(char) * (LWS_SEND_BUFFER_PRE_PADDING + len));
  if (!out) {
    SYSLOG(LOG_ERR, "PluginSocket_writeToSocket: message padding alloc failed");
    return -1;
  }

  //* setup the buffer*/
  memcpy(out + LWS_SEND_BUFFER_PRE_PADDING, str, len);

  //add this message to the write buffer
  bufferSocketWrite(wsi_in, out, len);
  //process the next message in the buffer
  flushSocketWrites();

  return n;
}

/*
 * Creates a context instance for a socket server
 * on a specified port number.
 */
static struct lws_context *_makeContext(int port) {

  portNumber = port;
  PluginSocket_AddProtocol(&listTerminator);
  printProtocols();

  const char *interface = NULL;
  struct lws_context_creation_info info;
  struct lws_context *context;
  // Not using ssl
  const char *cert_path = NULL;
  const char *key_path = NULL;
  // no special options
  int opts = 0;


  //* setup websocket context info*/
  memset(&info, 0, sizeof info);

  info.port = port;
  info.iface = interface;
  info.protocols = _protocols;
  //info.extensions = lws_get_internal_extensions();
  info.ssl_cert_filepath = cert_path;
  info.ssl_private_key_filepath = key_path;
  info.gid = -1;
  info.uid = -1;
  info.options = opts;
  info.max_http_header_pool = 256;
  /*info.timeout_secs = 10;
  info.ka_probes = 3;
  info.ka_time = 5;
  info.ka_interval = 2;*/

  //* create libwebsocket context. */
  context = lws_create_context(&info);
  if (context == NULL) {
    SYSLOG(LOG_ERR, " PluginSocket_InitSocket: Error creating websocket context");
    return NULL;
  }

  return context;
}

/*
 * Initializes the daemons socket server for a specified
 * port number. Ensures that only one socket server
 * is running/instantiated.
 */
int PluginSocket_Start(int port) {

  if (!_context)
    _context = _makeContext(port);

  if (_context != NULL) return 0;
  return -1;
}


int PluginSocket_GetPort(void) {

  return portNumber;
}

/*
 * Destroys and frees all memory associated
 * with the daemon's socket server instance.
 */
void PluginSocket_Cleanup(void) {

  if (_htmlPath)
    free(_htmlPath);
  _htmlPath = NULL;

  if (_basePath)
    free(_basePath);
  _basePath = NULL;

  lws_context_destroy(_context);
  PluginSocket_FreeProtocolList();
  _context = NULL;

}

/*
 * Set the location of index.html file to serve
 * via the http server when accessing '/' url
 */
char PluginSocket_ServeHtmlFile(char *htmlPath) {

  if (_htmlPath)
    free(_htmlPath);

  _htmlPath = allocStr(htmlPath);
  if (!_htmlPath)
    return -1;

  char *base = strrchr(_htmlPath, '/');
  if (!base)
    return 0;

  //temporarily set the '/' to end of string for copying to basepath
  char old = *base;
  *base = '\0';


  _basePath = allocStr(_htmlPath);
  if (!_basePath) {
    *base = old;
    return -1;
  }

  *base = old;
  return 0;
}
