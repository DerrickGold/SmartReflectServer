/*
 * MMCOM
 *  Sends a command to the mirror, and receives info back.
 * Arguments:
 *  -p
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h> //include for basename
#include <signal.h>
#include <time.h>
#include <libwebsockets.h>

#include "socketResponse.h"


#define DEFAULT_PORT 5000
#define DEFAULT_TIMOUT_LEN 1

#define HELP_TEXT \
 "\n%s: [-a server address] [-p PORT_NUM] -c COMMAND [[-x PLUGIN] [-v VALUES]]\n" \
 "\tSends a command to the running SmartReflect PluginDaemon process and returns the result.\n\n" \
 "\tArguments:\n" \
 "\t\t-a: Specify an server address. Default is 'localhost'.\n" \
 "\t\t-p: Specify a port number to use. Default is %d.\n" \
 "\t\t-c: The command action to perform:\n" \
 "\t\t\tdisable: Disable a currently running plugin.\n" \
 "\t\t\tenable: Start a currently disabled plugin.\n" \
 "\t\t\treload: Disables, then enables a plugin with one command.\n" \
 "\t\t\tplugins: Get a list of all the installed plugins.\n" \
 "\t\t\tgetdir: Get the folder path of an installed plugin.\n" \
 "\t\t\trmplug: Returns the dimensions of the mirror (widthxheight).\n" \
 "\t\t\tmirrorsize: indicate whether the mirror successfully connected to front end.\n" \
 "\t\t\tstop: Stop the SmartReflect PluginDaemon.\n" \
 "\t\t\tsetcss: Modify the CSS of a specified plugin's display client.\n" \
 "\t\t\tgetcss: Query the plugin's css attribute from the display client.\n" \
 "\t\t\tsavecss: Dumps any API sent css attributes to 'position.txt' in plugin folder.\n" \
 "\t\t\tjscmd: Tell a plugin to execute a JavaScript function.\n" \
 "\t\t\tdisplay: Returns if a browser display is connected to the PluginDaemon.\n" \
 "\t\t\tgetopt: Returns a specified value from a plugin's config file.\n" \
 "\t\t\tsetopt: Store a key-value option in a plugin's config file.\n" \
 "\t\t-x: Name of plugin to perform action on.\n" \
 "\t\t-v: Any associated values to perform action with.\n" \
 "\t\t\tCSS values: '<css_attribute>:<new value>'\n" \
 "\t\t\tPlugin Attribute: '<attribute>:<new value>'\n"


static char API_IDENTIFIER[256];
static char *COMMAND_BUFFER = NULL;

static char *prgmName = NULL;

static int destroy_flag = 0;

static unsigned int sentTime = 0;

char *server = "localhost";

static SocketResponse_t serverResponse;

struct session_data {
    int fd;
};


static int parseResponseSection(char *input, char *inputEnd, char **output, char **nextLine) {

  char parsed = 0;
  char *pos = input, *outStart = NULL;

  //otherwise, leading whitespace removed
  outStart = pos;

  //find the end of the line now
  while (pos < inputEnd && *pos != ':' && *pos != '\0')
    pos++;

  //end of input, no further lines given
  if (pos >= inputEnd - 1)
    parsed++;
  else {
    //otherwise, we have hit the end of the given command and more
    //input exists, so remove the deliminating the command, and
    //advance the pos ptr to the next input
    (*pos) = '\0';
    pos++;
  }

  if (output && strcmp(outStart, ":"))
    *output = outStart;

  *nextLine = pos;
  return parsed;
}

int parseResponse(char *response, size_t responseLen, char **identifier, char **action, char **status, char **plugin,
                  char **payload) {

  if (!response)
    return -1;

  char *input = response,
          *inputEnd = input + responseLen;


  //optional identifier
  if (parseResponseSection(input, inputEnd, identifier, &input))
    return -1;

  //required action
  if (parseResponseSection(input, inputEnd, action, &input))
    return -1;

  //status is always provided
  if (parseResponseSection(input, inputEnd, status, &input))
    return -1;

  //plugin depends on the action
  if (parseResponseSection(input, inputEnd, plugin, &input))
    return -1;

  //payload depends on the action
  if (payload)
    *payload = input;

  return 0;
}

//A safe version of strlen
static size_t strlens(char *string) {

  if (!string)
    return 0;

  return strlen(string);
}


static void printHelp() {
  printf(HELP_TEXT, prgmName, DEFAULT_PORT);
}


static int websocket_write_back(struct lws *wsi_in, char *str, int str_size_in) {

  if (str == NULL || wsi_in == NULL)
    return -1;

  int n;
  int len;
  char *out = NULL;

  if (str_size_in < 1)
    len = strlen(str);
  else
    len = str_size_in;

  out = (char *) malloc(sizeof(char) * (LWS_SEND_BUFFER_PRE_PADDING + len));
  //* setup the buffer*/
  memcpy(out + LWS_SEND_BUFFER_PRE_PADDING, str, len);
  //* write out*/
  n = lws_write(wsi_in, (unsigned char *) out + LWS_SEND_BUFFER_PRE_PADDING, len, LWS_WRITE_TEXT);
  //* free the buffer*/
  free(out);

  return n;
}


static int ws_service_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {

  switch (reason) {

    case LWS_CALLBACK_CLIENT_ESTABLISHED:
      lws_callback_on_writable(wsi);
      break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:

      if (lws_partial_buffered(wsi) || COMMAND_BUFFER == NULL)
        return 0;

      websocket_write_back(wsi, COMMAND_BUFFER, strlen(COMMAND_BUFFER));
      //lws_callback_on_writable(wsi);
      free(COMMAND_BUFFER);
      COMMAND_BUFFER = NULL;
      sentTime = time(NULL);
      break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
      destroy_flag = 1;
      break;

    case LWS_CALLBACK_CLOSED:
      destroy_flag = 1;
      break;

    case LWS_CALLBACK_CLIENT_RECEIVE: {

      SocketResponse_build(&serverResponse, wsi, (char *) in, len);
      if (SocketResponse_done(&serverResponse)) {

        char *identifier = NULL;
        char *status = NULL;
        char *response = malloc(SocketResponse_size(&serverResponse) + 1);
        if (!response) {
          fprintf(stderr, "Error allocating response string\n");
          return -1;
        }

        strcpy(response, SocketResponse_get(&serverResponse));
        parseResponse(response, SocketResponse_size(&serverResponse), &identifier, NULL, &status, NULL, NULL);

        if (!strcmp(identifier, API_IDENTIFIER)) {
          //only process responses specific to this program
          if (!strcmp(status, "pending"))
            return 0;

          printf("%s\n", SocketResponse_get(&serverResponse));

          //if there is no status replied, or the message is not
          //pending, we can exit the program
          destroy_flag = 1;
          return -1;
        }

        if (response)
          free(response);
        SocketResponse_free(&serverResponse);
      }
    }
      break;
    default:
      break;
  }

  return 0;
}


int main(int argc, char *argv[]) {

  prgmName = basename(argv[0]);

  //No user given arguments provided, print help
  if (argc < 2) {
    printHelp();
    return EXIT_FAILURE;
  }

  //generate Identifier for filtering responses
  srand(time(NULL));
  sprintf(API_IDENTIFIER, "mmcom%d", rand());

  char *address = server,
          *port = NULL,
          *cmd = NULL,
          *plugin = NULL,
          *values = NULL;

  unsigned int timeoutLen = DEFAULT_TIMOUT_LEN;

  //loop through arguments and collect options
  int c;
  while ((c = getopt(argc, argv, "ha:p:c:x:v:t:")) != -1) {

    switch (c) {
      case 'h':
        printHelp();
        return EXIT_SUCCESS;
      case 'a':
        address = optarg;
        break;
      case 'p':
        port = optarg;
        break;
      case 'c':
        cmd = optarg;
        break;
      case 'x':
        plugin = optarg;
        break;
      case 'v':
        values = optarg;
        break;
      case 't':
        timeoutLen = strtol(optarg, NULL, 10);
        break;
      default:
        //unsupported arguments
        fprintf(stderr, "Invalide argument: %c\n", c);
        return EXIT_FAILURE;
    }
  }

  if (cmd == NULL) {
    fprintf(stderr, "Missing command argument.\n");
    return EXIT_FAILURE;
  }

  lws_set_log_level(0, NULL);

  char protoName[] = "STDIN";

  struct lws_context *context = NULL;
  struct lws_context_creation_info info;
  struct lws *wsi = NULL;
  struct lws_protocols protocol;
  struct lws_client_connect_info i;

  memset(&info, 0, sizeof info);
  memset(&i, 0, sizeof i);

  info.port = CONTEXT_PORT_NO_LISTEN;
  info.protocols = &protocol;
  info.gid = -1;
  info.uid = -1;

  protocol.name = protoName;
  protocol.callback = ws_service_callback;
  protocol.per_session_data_size = sizeof(struct session_data);
  protocol.rx_buffer_size = 0;

  context = lws_create_context(&info);

  if (context == NULL)
    return -1;

  const char *prot;
  if (lws_parse_uri(address, &prot, &i.address, &i.port, &i.path)) {
    printf("Client.c: Error parsing uri\n");
    exit(1);
  }

  if (port != NULL)
    i.port = atoi(port);
  else
    i.port = DEFAULT_PORT;

  i.context = context;
  i.ssl_connection = 0;
  i.host = i.address;
  i.origin = i.address;
  i.ietf_version_or_minus_one = -1;
  i.protocol = protoName;

  size_t len = strlen(API_IDENTIFIER) + strlens(cmd) + strlens(plugin) + strlens(values) + 5;
  COMMAND_BUFFER = calloc(len + 1, sizeof(char));
  if (!COMMAND_BUFFER) {
    fprintf(stderr, "Generated command string too long!\n");
    return EXIT_FAILURE;
  }


  sprintf(COMMAND_BUFFER, "%s\n", API_IDENTIFIER);


  //build command
  if (cmd != NULL) {
    strcat(COMMAND_BUFFER, cmd);
    strcat(COMMAND_BUFFER, "\n");
  }
  if (plugin != NULL) {
    strcat(COMMAND_BUFFER, plugin);
  }

  if (plugin || values)
    strcat(COMMAND_BUFFER, "\n");

  if (values != NULL)
    strcat(COMMAND_BUFFER, values);

  memset(&serverResponse, 0, sizeof(SocketResponse_t));

  wsi = lws_client_connect_via_info(&i);
  while (!destroy_flag) {
    //wait for connection
    lws_service(context, 0);

    //check for timeout after command is sent
    if(sentTime && time(NULL) - sentTime >= timeoutLen) {
      destroy_flag = 1;
      printf("%s:%s:%s::", API_IDENTIFIER, cmd, "fail");
    }
  }

  lws_context_destroy(context);
  return 0;
}
