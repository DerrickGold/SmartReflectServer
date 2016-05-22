//
// Created by Derrick on 2016-04-02.
//
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#include <libwebsockets.h>
#include "socketResponse.h"
#include "misc.h"


void SocketResponse_free(SocketResponse_t *sockr) {

  if (sockr->data)
    free(sockr->data);

  sockr->data = NULL;
  sockr->len = 0;
  sockr->complete = 0;
}


int SocketResponse_build(SocketResponse_t *sockr, struct lws *wsi, char *response, size_t len) {

  //if a message has been previously completed, clear it and restart
  //for new message
  if (sockr->complete)
    SocketResponse_free(sockr);


  size_t oldSize = sockr->len;
  if (oldSize > 0) oldSize--;

  sockr->len += len;
  sockr->len += (sockr->len == len);

  char *temp = realloc(sockr->data, sockr->len);
  if (!temp) {
    SYSLOG(LOG_ERR, "SocketResponse_build: Error allocating socket response size");
    return -1;
  }
  sockr->data = temp;
  memcpy(&sockr->data[oldSize], response, len);
  sockr->data[sockr->len - 1] = 0;

  //no more expected data from this response, set it as complete
  if (lws_remaining_packet_payload(wsi) == 0 && lws_is_final_fragment(wsi))
    sockr->complete = 1;

  return 0;
}


char SocketResponse_done(SocketResponse_t *sockr) {

  return sockr->complete;
}

char *SocketResponse_get(SocketResponse_t *sockr) {

  if (sockr->complete)
    return sockr->data;

  return NULL;
}

size_t SocketResponse_size(SocketResponse_t *sockr) {

  return sockr->len;
}
