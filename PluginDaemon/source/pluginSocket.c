//
// Created by Derrick on 2016-01-26.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <libwebsockets.h>

#include "protocolWrite.h"
#include "pluginSocket.h"
#include "misc.h"

#define INDEX_PATH "/"
#define GUI_PATH "/gui"


#define SOCKET_TIMEOUT 50
#define BASE_PROTO_POOL 2


/*
 * Main context for the socket server
 */
struct lws_context *_context = NULL;

static int portNumber = -1;


/*
 * Dynamically allocated list of protocols.
 * Grows as more protocols are added.
 */
static int _protocolCount = 0;
static int _lastProtocol = 0;
struct lws_protocols *_protocols = NULL;
static ProtocolWrites_t protocolWriteQueues = {0, 0};
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

static const struct lws_protocol_vhost_options bmp_mimetypes = {
        NULL,
        NULL,
        ".bmp",
        "image/bmp"
};

static const struct lws_protocol_vhost_options extra_mimetypes = {
        (struct lws_protocol_vhost_options *) &bmp_mimetypes,
        NULL,
        ".woff2",
        "application/font-woff"
};

static const struct lws_http_mount indexMount = {
        NULL,
        INDEX_PATH,
        "./",
        "index.html",
        NULL,
        &extra_mimetypes,
        0,
        0,
        0,
        0,
        0,
        LWSMPRO_FILE,
        1
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
      //start++;
    }


    int newCount = _protocolCount + 1;
    struct lws_protocols *protos = realloc(_protocols, sizeof(struct lws_protocols) * newCount);
    if (!protos) {
      SYSLOG(LOG_ERR, "PluginSocket_MakeProtocol: Error reallocating protocol list: count: %d", newCount);
      return -1;
    }
    _protocols = protos;
    _protocolCount = newCount;
    SYSLOG(LOG_INFO, "Addprotocol: resized pool %d", _protocolCount);
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
  _protocols[_lastProtocol].rx_buffer_size = proto->rx_buffer_size;
  _protocols[_lastProtocol].per_session_data_size = proto->per_session_data_size;
  _protocols[_lastProtocol].id = _lastProtocol;

  Protocol_setProtocolCount(&protocolWriteQueues, _lastProtocol);
  Protocol_initQueue(&protocolWriteQueues, _lastProtocol);

  _lastProtocol++;

  if (addEnd)
    PluginSocket_AddProtocol(&listTerminator);

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
      Protocol_removeProtocol(&protocolWriteQueues, _protocols[i].id);
      continue;
    } else if (pos > -1) {

      //i should be ahead of pos now, so we can shift the rest of the protocol
      //list overtop of the protocol we are removing
      memcpy(&_protocols[pos++], &_protocols[i], sizeof(struct lws_protocols));
      //make sure the id's point to the correct protocol buffer
      _protocols[pos - 1].id = _protocols[i].id;
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
}

/*
 * Write to a target socket.
 *
 * All messages must have pre-padding as defined by
 * LWS_SEND_BUFFER_PRE_PADDING. This function will
 * apply said padding to each message sent.
 */
int PluginSocket_writeToSocket(struct lws *wsi_in, char *str, int str_size_in, char noHeader) {

  if (str == NULL || wsi_in == NULL || !str_size_in)
    return -1;

  int len;
  unsigned char *out = NULL;

  if (str_size_in < 1) {
    char *ptr = str;

    ptr += (noHeader != 0) * LWS_SEND_BUFFER_PRE_PADDING;
    len = strlen(ptr);
  } else
    len = str_size_in;

  if (!noHeader) {
    out = malloc(sizeof(char) * (LWS_SEND_BUFFER_PRE_PADDING + len));
    if (!out) {
      SYSLOG(LOG_ERR, "PluginSocket_writeToSocket: message padding alloc failed");
      return -1;
    }

    //* setup the buffer*/
    memcpy(out + LWS_SEND_BUFFER_PRE_PADDING, str, len);
  }
  else
    out = str;

  //add this message to the write buffer
  Protocol_addWriteToQueue(&protocolWriteQueues, wsi_in, out, len);
  return 0;
}

void PluginSocket_writeBuffers(struct lws *wsi) {
  struct lws_protocols *proto = (struct lws_protocols *) lws_get_protocol(wsi);
  if (!proto) {
    SYSLOG(LOG_ERR, "ERROR: No protocol to write!");
    return;
  }
  Protocol_processQueue(wsi, &protocolWriteQueues);
}

void PluginSocket_clearWriteBuffers(struct lws *wsi, char onlyDead) {

  struct lws_protocols *proto = (struct lws_protocols *) lws_get_protocol(wsi);
  if (!proto) {
    SYSLOG(LOG_ERR, "ERROR: No protocol to write!");
    return;
  }

  Protocol_clearQueue(wsi, &protocolWriteQueues);
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
  info.ssl_cert_filepath = cert_path;
  info.ssl_private_key_filepath = key_path;
  info.gid = -1;
  info.uid = -1;
  info.options = opts;
  info.max_http_header_pool = 256;
  info.mounts = &indexMount;

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

  if (_context != NULL)
    return 0;

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
  Protocol_destroyQueues(&protocolWriteQueues);
  PluginSocket_FreeProtocolList();
  _context = NULL;

}