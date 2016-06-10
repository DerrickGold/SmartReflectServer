//
// Created by Derrick on 2016-05-22.
//

/*
 * Socket writes are buffered on a per-protocol basis. As each protocol is created,
 * it is given an ID number that is used to index to the correct write queue.
 */
#include <syslog.h>
#include <libwebsockets.h>

#include "misc.h"
#include "protocolWrite.h"

static void _clearQueue(WriteQueue_t *queue, int fd) {

  size_t i = 0;
  for (i = 0; i < NUM_BUFFERED_WRITES; i++) {

    BufferedWrite_t *buffers = &queue->writes[i];

    if (fd > -1 && buffers->descriptor != fd)
      continue;

    if (buffers->msg)
      free(buffers->msg);

    buffers->msg = NULL;
    buffers->len = 0;
  }

  queue->lastWritten = 0;
  queue->lastBuffered = 0;
}


static int getProtocolID(struct lws *socket) {

  struct lws_protocols *proto = (struct lws_protocols *) lws_get_protocol(socket);
  if (!proto || proto->id < 0) {
    SYSLOG(LOG_ERR, "ERROR: Socket has no protocol associated");
    return -1;
  }

  return proto->id;
}



void Protocol_addWriteToQueue(ProtocolWrites_t * protowrites, struct lws *socket, void *msg, size_t len) {

  if (!protowrites)
    return;

  int protoID = getProtocolID(socket);
  if (!protowrites->buffer || protoID < 0 || protoID > protowrites->bufferCount)
    return;

  //SYSLOG(LOG_INFO, "Protocol_addWriteToCueue: Buffering queue [%d]", proto->id);
  WriteQueue_t *curBuffer = &protowrites->buffer[protoID];
  BufferedWrite_t *curWrite = &curBuffer->writes[curBuffer->lastBuffered];

  if (curWrite->msg) {
    SYSLOG(LOG_ERR, "WRITING OVER BUFFERED MSG");
    free(curWrite->msg);
  }

  curWrite->descriptor = lws_get_socket_fd(socket);
  curWrite->msg = msg;
  curWrite->len = len;

  curBuffer->lastBuffered++;
  curBuffer->lastBuffered %= NUM_BUFFERED_WRITES;
}

void Protocol_processQueue(struct lws *socket, ProtocolWrites_t *protowrites) {

  if (!socket || !protowrites)
    return;

  int protoID = getProtocolID(socket);
  if (!protowrites->buffer || protoID < 0 || protoID > protowrites->bufferCount)
    return;

  WriteQueue_t *curBuffer = &protowrites->buffer[protoID];
  int fd = lws_get_socket_fd(socket);

  do {
    BufferedWrite_t *curWrite = &curBuffer->writes[curBuffer->lastWritten];

    if (!curWrite->msg || fd != curWrite->descriptor || lws_partial_buffered(socket))
      goto increment;

    lws_write(socket, curWrite->msg + LWS_SEND_BUFFER_PRE_PADDING, curWrite->len, LWS_WRITE_TEXT);
    free(curWrite->msg);
    curWrite->msg = NULL;

    increment:
    curBuffer->lastWritten++;
    curBuffer->lastWritten %= NUM_BUFFERED_WRITES;

  } while (curBuffer->lastWritten != curBuffer->lastBuffered);
}

//allocates memory for each protocol queue
int Protocol_setProtocolCount(ProtocolWrites_t *protowrites, size_t newCount) {

  WriteQueue_t *newBuffer = realloc(protowrites->buffer, sizeof(WriteQueue_t) * (newCount + 1));
  if (!newBuffer) {
    SYSLOG(LOG_INFO, "Protocol_addBuffer: Failed to resize old buffers");
    return -1;
  }
  newCount++;

  protowrites->buffer = newBuffer;
  protowrites->bufferCount = newCount;
  SYSLOG(LOG_INFO, "Protocol_addBuffer: new count: %d", newCount);

  return 0;
}


int Protocol_removeProtocol(ProtocolWrites_t *protowrites, unsigned int protocolID) {

  _clearQueue(&protowrites->buffer[protocolID], -1);

  unsigned int i = 0;
  for (i = protocolID; i < protowrites->bufferCount - 1; i++) {
    memcpy(&protowrites->buffer[i], &protowrites->buffer[i + 1], sizeof(WriteQueue_t));
  }

  return Protocol_setProtocolCount(protowrites, protowrites->bufferCount - 1);
}

void Protocol_clearQueue(struct lws *socket, ProtocolWrites_t *protowrites) {

  int protoID = getProtocolID(socket);
  if (!protowrites->buffer || protoID < 0 || protoID > protowrites->bufferCount)
    return;

  _clearQueue(&protowrites->buffer[protoID], lws_get_socket_fd(socket));
}


void Protocol_destroyQueues(ProtocolWrites_t *protowrites) {

  if (!protowrites->buffer)
    return;

  unsigned int i = 0;
  for (i = 0; i < protowrites->bufferCount; i++) {
    _clearQueue(&protowrites->buffer[i], -1);
  }

  free(protowrites->buffer);
  protowrites->buffer = NULL;
}


void Protocol_initQueue(ProtocolWrites_t *protowrites, unsigned int protocolID) {

  if (protocolID < 0 || protocolID >= protowrites->bufferCount) {
    SYSLOG(LOG_ERR, "Protocol_initQueue: initializing queue out of bounds");
    return;
  }

  memset(&protowrites->buffer[protocolID], 0, sizeof(WriteQueue_t));
}