//
// Created by Derrick on 2016-05-22.
//

#ifndef SMARTREFLECT_PROTOCOLWRITE_H
#define SMARTREFLECT_PROTOCOLWRITE_H

#include <libwebsockets.h>

#define NUM_BUFFERED_WRITES 256

typedef struct BufferedWrite_s {
    struct lws *socket;
    void *msg;
    size_t len;
} BufferedWrite_t;

typedef struct WriteQueue_s {
    BufferedWrite_t writes[NUM_BUFFERED_WRITES];
    size_t lastBuffered, lastWritten;
} WriteQueue_t;

typedef struct ProtocolWrites_s {
    WriteQueue_t *buffer;
    size_t bufferCount;
} ProtocolWrites_t;

extern void Protocol_addWriteToQueue(ProtocolWrites_t * protowrites, struct lws *socket, void *msg, size_t len);

extern void Protocol_processQueue(ProtocolWrites_t *protowrites, unsigned int protocolID);

extern int Protocol_setProtocolCount(ProtocolWrites_t *protowrites, size_t newCount);

extern int Protocol_removeProtocol(ProtocolWrites_t *protowrites, unsigned int protocolID);

extern void Protocol_clearQueue(ProtocolWrites_t *protowrites, unsigned int protocolID);

extern void Protocol_destroyQueues(ProtocolWrites_t *protowrites);

extern void Protocol_initQueue(ProtocolWrites_t *protowrites, unsigned int protocolID);

#endif //SMARTREFLECT_PROTOCOLWRITE_H
