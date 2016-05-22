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
#include <libwebsockets.h>

#define DEFAULT_PORT 5000

#define HELP_TEXT \
 "\n%s: [-p PORT_NUM] -c COMMAND [[-x PLUGIN] [-v VALUES]]\n" \
 "\tSends a command to the running SmartReflect PluginDaemon process and returns the result.\n\n" \
 "\tArguments:\n" \
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


static char *COMMAND_BUFFER = NULL;

static char *prgmName = NULL;

static int destroy_flag = 0;

char *server = "localhost";

struct session_data {
    int fd;
};


int parseResponse(char *response, char **action, char **status, char **plugin, char **payload) {

    if (!response) {
        return -1;
    }

    char delim = ':';

    if (action)
        *action = response;

    //search for status
    while(*response != delim && *response != '\0')
        response++;

    if (*response == '\0') {
        //hit end of response, error!
        return -1;
    }

    //mark off the end of the action
    *response = '\0';
    response++;
    //get the status
    if (status)
        *status = response;

    //search for plugin that action operates on
    while(*response != delim && *response != '\0')
        response++;

    if (*response == '\0') {
        *payload = NULL;
        return 0;
    }

    //otherwise, : was found, make it end of string
    *response = '\0';
    response++;
    if (*response == delim) {
        //no plugin was provided for this action
        response++;
        if (payload)
            *payload++ = response;
        //no more information to be found after payload
        return 0;

    } else {
        //plugin was provided for this action
        if (plugin)
            *plugin = response;
    }

    //now scan for payload, which is optional
    while(*response != delim && *response != '\0')
        response++;

    if (*response == '\0')
        //hit end of string before finding the next : delimited parameter
        return 0;

    response++;
    //otherwise, we can set the payload
    if (payload)
        *payload = response;

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


static int websocket_write_back(struct lws *wsi_in, char *str, int str_size_in)
{
    if (str == NULL || wsi_in == NULL)
        return -1;

    int n;
    int len;
    char *out = NULL;

    if (str_size_in < 1)
        len = strlen(str);
    else
        len = str_size_in;

    out = (char *)malloc(sizeof(char)*(LWS_SEND_BUFFER_PRE_PADDING + len ));
    //* setup the buffer*/
    memcpy (out + LWS_SEND_BUFFER_PRE_PADDING, str, len );
    //* write out*/
    n = lws_write(wsi_in, (unsigned char*)out + LWS_SEND_BUFFER_PRE_PADDING, len, LWS_WRITE_TEXT);
    //* free the buffer*/
    free(out);

    return n;
}


static int ws_service_callback(
                         struct lws *wsi,
                         enum lws_callback_reasons reason, void *user,
                         void *in, size_t len)
{

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
        break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        destroy_flag = 1;
        break;

    case LWS_CALLBACK_CLOSED:
        destroy_flag = 1;
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        //print back the received info from the mirror
        printf("%s", (char *)in);
        if (lws_remaining_packet_payload(wsi) == 0) {
          printf("\n");
        }
        //then exit
        char *status = NULL;

        parseResponse((char *)in, NULL, &status, NULL, NULL);
        //if there is no status replied, or the message is not
        //pending, we can exit the program
        destroy_flag =  (!status || strcmp(status, "pending"));
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

    char *port = NULL, *cmd = NULL, *plugin = NULL, *values = NULL;

    //loop through arguments and collect options
    int c;
    while ((c = getopt(argc, argv, "hp:c:x:v:")) != -1) {

        switch (c) {
        case 'h':
            printHelp();
            return EXIT_SUCCESS;
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

    protocol.name  = protoName;
    protocol.callback = ws_service_callback;
    protocol.per_session_data_size = sizeof(struct session_data);
    protocol.rx_buffer_size = 128;

    context = lws_create_context(&info);

    if (context == NULL)
        return -1;

    const char *prot;
    if (lws_parse_uri(server, &prot, &i.address, &i.port, &i.path)) {
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

    size_t len = strlens(cmd) + strlens(plugin) + strlens(values) + 4;
    COMMAND_BUFFER = calloc(len + 1, sizeof(char));
    if (!COMMAND_BUFFER) {
        fprintf(stderr, "Generated command string too long!\n");
        return EXIT_FAILURE;
    }

    //build command
    if (cmd != NULL) {
      strcpy(COMMAND_BUFFER, cmd);
      strcat(COMMAND_BUFFER, "\n");
    }
    if (plugin != NULL) {
      strcat(COMMAND_BUFFER, plugin);
      strcat(COMMAND_BUFFER, "\n");
    }
    if (values != NULL)
      strcat(COMMAND_BUFFER, values);



    wsi = lws_client_connect_via_info(&i);
    while(!destroy_flag) {
        //wait for connection
        lws_service(context, 50);
    }

    lws_context_destroy(context);
    return 0;
}
