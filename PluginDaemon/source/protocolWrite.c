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

void _clearQueue(WriteQueue_t *queue) {

  BufferedWrite_t *buffers = queue->writes;
  size_t i = 0;

  for (i = 0; i < NUM_BUFFERED_WRITES; i++) {
    if (buffers->msg)
      free(buffers->msg);

    buffers->msg = NULL;
    buffers->socket = NULL;
    buffers->len = 0;
  }

  queue->lastWritten = 0;
  queue->lastBuffered = 0;
}

void Protocol_addWriteToQueue(ProtocolWrites_t * protowrites, struct lws *socket, void *msg, size_t len) {

  if (!protowrites)
    return;

  struct lws_protocols *proto = (struct lws_protocols *) lws_get_protocol(socket);
  if (!proto) {
    SYSLOG(LOG_ERR, "ERROR: Socket has no protocol associated");
    return;
  }

  //SYSLOG(LOG_INFO, "Protocol_addWriteToCueue: Buffering queue [%d]", proto->id);
  WriteQueue_t *curBuffer = &protowrites->buffer[proto->id];
  BufferedWrite_t *curWrite = &curBuffer->writes[curBuffer->lastBuffered];

  if (curWrite->socket) {
    SYSLOG(LOG_ERR, "WRITING OVER BUFFERED MSG");
    free(curWrite->msg);
  }
  curWrite->socket = socket;
  curWrite->msg = msg;
  curWrite->len = len;

  curBuffer->lastBuffered++;
  curBuffer->lastBuffered %= NUM_BUFFERED_WRITES;
}

void Protocol_processQueue(ProtocolWrites_t *protowrites, unsigned int protocolID) {

  if (!protowrites || protocolID < 0 || protocolID > protowrites->bufferCount)
    return;

  WriteQueue_t *curBuffer = &protowrites->buffer[protocolID];
  //SYSLOG(LOG_INFO, "Protocol_processQueue: Processing queue [%d]", protocolID);
  do {
    BufferedWrite_t *curWrite = &curBuffer->writes[curBuffer->lastWritten];
    if (!curWrite->socket || !curWrite->msg || lws_partial_buffered(curWrite->socket))
      goto increment;

    lws_write(curWrite->socket, curWrite->msg + LWS_SEND_BUFFER_PRE_PADDING, curWrite->len, LWS_WRITE_TEXT);
    free(curWrite->msg);
    curWrite->msg = NULL;
    curWrite->socket = NULL;

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

  protowrites->buffer = newBuffer;
  protowrites->bufferCount = newCount;
  SYSLOG(LOG_INFO, "Protocol_addBuffer: new count: %d", newCount);

  return 0;
}


int Protocol_removeProtocol(ProtocolWrites_t *protowrites, unsigned int protocolID) {

  _clearQueue(&protowrites->buffer[protocolID]);

  unsigned int i = 0;
  for (i = protocolID; i < protowrites->bufferCount - 1; i++) {
    memcpy(&protowrites->buffer[i], &protowrites->buffer[i + 1], sizeof(WriteQueue_t));
  }

  return Protocol_setProtocolCount(protowrites, protowrites->bufferCount - 1);
}

void Protocol_clearQueue(ProtocolWrites_t *protowrites, unsigned int protocolID) {

  if (!protowrites->buffer || protocolID >= protowrites->bufferCount)
    return;

  _clearQueue(&protowrites->buffer[protocolID]);
}

void Protocol_destroyQueues(ProtocolWrites_t *protowrites) {

  if (!protowrites->buffer)
    return;

  unsigned int i = 0;
  for (i = 0; i < protowrites->bufferCount; i++) {
    _clearQueue(&protowrites->buffer[i]);
  }

  free(protowrites->buffer);
  protowrites->buffer = NULL;
}


void Protocol_initQueue(ProtocolWrites_t *protowrites, unsigned int protocolID) {

  if (protocolID < 0 || protocolID > protowrites->bufferCount)
    return;

  memset(&protowrites->buffer[protocolID], 0, sizeof(WriteQueue_t));
}