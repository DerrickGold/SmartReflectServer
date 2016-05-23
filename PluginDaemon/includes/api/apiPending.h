//
// Created by Derrick on 2016-03-06.
//

#ifndef MAGICMIRROR_APIPENDING_H
#define MAGICMIRROR_APIPENDING_H

#include "api.h"
#include "plugin.h"

typedef enum {
    APIPENDING_PLUGIN,
    APIPENDING_DISPLAY,
} APIPendingType_e;

void APIPending_init(void);

extern int APIPending_getFreeSlot(void);

extern int APIPending_addAction(APIPendingType_e type, char *id, APIAction_e action, Plugin_t *plugin,
                                struct lws *socket);

extern void APIPending_freeSlot(int slot);

extern void APIPending_update(void);

#endif //MAGICMIRROR_APIPENDING_H
