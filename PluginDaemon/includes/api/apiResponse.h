//
// Created by Derrick on 2016-03-06.
//

#ifndef MAGICMIRROR_APIRESPONSE_H
#define MAGICMIRROR_APIRESPONSE_H

#include "api.h"
#include "pluginSocket.h"

typedef struct APIResponse_s {
    size_t payloadSize;
    char *payload;
} APIResponse_t;

extern void APIResponse_free(APIResponse_t *response);

extern APIResponse_t *APIResponse_new(void);

extern int APIResponse_concat(APIResponse_t *response, char *str, int len);

extern int APIResponse_send(APIResponse_t *response, struct lws *wsi, char *plugin, APIAction_e action, APIStatus_e status);

#endif //MAGICMIRROR_APIRESPONSE_H
