/*
 * API Pending:
 *
 * Some API calls will not return immediately. In these cases, typically
 * a query is made across a socket connection to retrieve the requested
 * information.
 *
 * This file creates a system for 'queuing' and processing pending API calls.
 */
#include <stdio.h>
#include <syslog.h>

#include "apiResponse.h"
#include "apiPending.h"
#include "display.h"
#include "misc.h"

#define MAX_PENDING_ACTIONS 32

//some actions may require multiple cycles to update
typedef struct PendingAction_s {
    APIPendingType_e type;
    APIAction_e action;
    Plugin_t *plugin;
    struct lws *socket;
    char *identifier;
    int started;
    APIResponse_t *response;
} PendingAction_t;

static size_t lastAction = 0, updateAction = 0;
static PendingAction_t pendingActions[MAX_PENDING_ACTIONS];


void APIPending_init(void) {

  memset(pendingActions, 0, sizeof(pendingActions));
  int i = 0;
  for (i = 0; i < MAX_PENDING_ACTIONS; i++)
    pendingActions[i].action = NO_ACTION;

}

int APIPending_getFreeSlot(void) {

  lastAction++;
  lastAction %= MAX_PENDING_ACTIONS;
  return lastAction;
}


int APIPending_addAction(APIPendingType_e type, char *id, APIAction_e action, Plugin_t *plugin, struct lws *socket) {

  int slot = APIPending_getFreeSlot();

  if (pendingActions[slot].action != NO_ACTION) {
    APIPending_freeSlot(slot);
  }

  char *idToken = NULL;
  if (id) {
    idToken = malloc(strlen(id) + 1);
    if (!idToken) {
      SYSLOG(LOG_INFO, "APIPending_addAction: Error allocating ID token.");
      return -1;
    }
    strcpy(idToken, id);
  }

  pendingActions[slot] = (PendingAction_t) {
          .type = type,
          .action = action,
          .plugin = plugin,
          .socket = socket,
          .response = APIResponse_new(),
          .identifier = idToken
  };

  return 0;
}

void APIPending_freeSlot(int slot) {

  if (slot < 0 || slot > MAX_PENDING_ACTIONS)
    return; //out of bounds

  APIResponse_free(pendingActions[slot].response);

  if (pendingActions[slot].identifier) {
    free(pendingActions[slot].identifier);
    pendingActions[slot].identifier = NULL;
  }

  memset(&pendingActions[slot], 0, sizeof(PendingAction_t));
  pendingActions[slot].action = NO_ACTION;
  SYSLOG(LOG_INFO, "APIPending: Clearing processed response");
}

void APIPending_update(void) {

  //do {

    char *resp = NULL;
    PendingAction_t *pending = &pendingActions[updateAction];

    if (pending->action == NO_ACTION)
      goto update;


    switch (pending->type) {

      case APIPENDING_DISPLAY:
        resp = Display_GetDisplayResponse();
        if (!resp)
          goto update;

        APIResponse_concat(pending->response, resp, -1);
        break;

      case APIPENDING_PLUGIN:
        //waiting for plugin response....
        resp = Plugin_ClientGetResponse(pending->plugin);
        if (!resp)
          goto update;

        APIResponse_concat(pending->response, resp, Plugin_ClientGetResponseSize(pending->plugin));
        Plugin_ClientFreeResponse(pending->plugin);
        break;

      default:
        break;
    }

    APIResponse_send(pending->response, pending->socket, pending->identifier, Plugin_GetName(pending->plugin),
                     pending->action, SUCCESS);
    //action was successful, clear it
    APIPending_freeSlot(updateAction);


    update:
    updateAction++;
    updateAction %= MAX_PENDING_ACTIONS;

  //} while (updateAction != lastAction);


}