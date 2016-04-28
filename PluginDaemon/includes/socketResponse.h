//
// Created by Derrick on 2016-04-02.
//

#ifndef MAGICMIRROR_SOCKETRESPONSE_H
#define MAGICMIRROR_SOCKETRESPONSE_H


typedef struct SocketResponse_s {
    char *data;
    size_t len;
    char complete;
} SocketResponse_t;

extern void SocketResponse_free(SocketResponse_t *sockr);

extern int SocketResponse_build(SocketResponse_t *sockr, struct lws *wsi, char *response, size_t len);

extern char SocketResponse_done(SocketResponse_t *sockr);

extern char *SocketResponse_get(SocketResponse_t *sockr);

extern size_t SocketResponse_size(SocketResponse_t *sockr);



#endif //MAGICMIRROR_SOCKETRESPONSE_H
