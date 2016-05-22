//
// Created by Derrick on 2016-01-26.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <libwebsockets.h>

#include "pluginComLib.h"
#include "misc.h"

#define FIFO_TIMEOUT 60 //in seconds

#define BATCH_START "["
#define BATCH_NEXT ","
#define BATCH_END "]"

#define COMMAND_START "{\"command\":\""
#define COMMAND_END "\","
#define DATA_START "\"data\":\""
#define DATA_END "\"}"


#define FIFO_OPEN(path) \
socketFd = open(path, O_WRONLY | O_NONBLOCK); \
errsv = errno


static char JSON_Escape[][8] = {
        "\\b",
        "\\f",
        "\\n",
        "\\r",
        "\\t",
        "\\\\",
        "\\\"",
        "\0"
};


static char *escapeCharacter(char input) {

  switch (input) {
    case '\b':
      return (char *) &JSON_Escape[0];
    case '\f':
      return (char *) &JSON_Escape[1];
    case '\n':
      return (char *) &JSON_Escape[2];
    case '\r':
      return (char *) &JSON_Escape[3];
    case '\t':
      return (char *) &JSON_Escape[4];
    case '\\':
      return (char *) &JSON_Escape[5];
    case '\"':
      return (char *) &JSON_Escape[6];
    default:
      return (char *) &JSON_Escape[7];
  }

  return (char *) &JSON_Escape[7];
}

char *PluginComLib_makeMsg(char *command, char *data) {
  //make sure a command is given
  if (!command) return NULL;

  size_t bufLen =  strlen(COMMAND_START) + strlen(command) + strlen(COMMAND_END) + strlen(DATA_START) +
                  +strlen(DATA_END);

  if (data) bufLen += strlen(data) * 2;


  char *buffer = calloc(bufLen + LWS_SEND_BUFFER_PRE_PADDING, sizeof(char));
  if (!buffer) {
    SYSLOG(LOG_ERR, "PluginComLib_makeMsg: error allocating command");
    return NULL;
  }

  char *pos = buffer + LWS_SEND_BUFFER_PRE_PADDING;

  pos += snprintf(pos, bufLen, "%s%s%s%s", COMMAND_START, command, COMMAND_END, DATA_START);

  if (data != NULL) {
    size_t byte = 0;
    //as we are writing data out, escape and special characters that would
    //throw off json parsing.
    do {
      char letter = data[byte];
      if (letter == '\0') break;

      char *escaped = escapeCharacter(letter);
      if (*escaped == '\0') {
        *pos++ = letter;
      } else {
        char *n = escaped;
        while (*n != '\0') {
          *pos++ = *n++;
        }
      }

    } while (byte++ < strlen(data) - 1);
    *pos = '\0';
  }

  memcpy(pos, DATA_END, strlen(DATA_END));
  return buffer;
}

