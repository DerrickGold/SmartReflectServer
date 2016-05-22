#include <stdio.h>
#include <syslog.h>
#include <libwebsockets.h>
#include "apiResponse.h"
#include "misc.h"

#define API_RETURN_FMT "%s:%s:%s:%s"
#define API_RETURN_FMT_SIZE 5
#define EMPTY_STR "\0"


void APIResponse_free(APIResponse_t *response) {

  if (!response)
    return;

  if (response->payload)
    free(response->payload);

  response->payload = NULL;
  response->payloadSize = 0;

  free(response);
}

APIResponse_t *APIResponse_new(void) {

  APIResponse_t *response = calloc(1, sizeof(APIResponse_t));
  if (!response) {
    SYSLOG(LOG_ERR, "APIResponse_new: Error creating new response");
    return NULL;
  }

  return response;
}

/*
 * Append a string the payload response currently being
 * built.
 */
int APIResponse_concat(APIResponse_t *response, char *str, int len) {

  size_t newSize = response->payloadSize;
  if (len > 0)
    newSize += len;
  else
    newSize += strlen(str) + 1;

  int firstAlloc = (response->payload == NULL);

  //reallocate listing buffer to hold next name
  char *temp = realloc(response->payload, newSize + 1);
  if (!temp) {
    SYSLOG(LOG_ERR, "api_payloadCat: Error resizing response payload: %s:%zu", str, newSize + 1);
    APIResponse_free(response);
    return -1;
  }

  response->payload = temp;
  response->payloadSize = newSize;

  if (firstAlloc)
    response->payload[0] = '\0';

  //add string to payload response
  strcat(response->payload, str);
  return 0;
}

/*
 * Prepends api header response to a return value
 */
int APIResponse_send(APIResponse_t *response, struct lws *wsi, char *plugin, APIAction_e action, APIStatus_e status) {

  char *actionStr = EMPTY_STR, *statusStr = (char *) API_STATUS_STRING[status];

  if (action != NO_ACTION)
    actionStr = (char *) allActions[action].name;

  char *plugName = EMPTY_STR;

  if (plugin)
    plugName = plugin;

  size_t responseStrLen = strlen(statusStr) + strlen(actionStr) + API_RETURN_FMT_SIZE + strlen(plugName) + 1;

  if (response->payload)
    responseStrLen += strlen(response->payload);

  char *responseStr = calloc(responseStrLen + LWS_SEND_BUFFER_PRE_PADDING, sizeof(char));
  if (!responseStr) {
    SYSLOG(LOG_ERR, "APIResponse_send: Error allocating API Response");
    return -1;
  }

  char *resPtr = responseStr + LWS_SEND_BUFFER_PRE_PADDING;

  if (response->payload)
    snprintf(resPtr, responseStrLen, API_RETURN_FMT, actionStr, statusStr, plugName, response->payload);
  else
    snprintf(resPtr, responseStrLen, API_RETURN_FMT, actionStr, statusStr, plugName, EMPTY_STR);

  SYSLOG(LOG_INFO, "API Response: %s", resPtr);

  //responseStr is created with LWS header, set 'noHeader' flag and responseStr will be free'd after
  //it is sent
  return PluginSocket_writeToSocket(wsi, responseStr, -1, 1);
}
