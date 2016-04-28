#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <libwebsockets.h>

#define SCREEN_WIDTH 1080

#define NANOSECOND 1000000000
#define MILLISECOND 1000

#define FPS 12

#define SPEED 1


#define KGRN "\033[0;32;32m"
#define KCYN "\033[0;36m"
#define KRED "\033[0;32;31m"
#define KYEL "\033[1;33m"
#define KBLU "\033[0;32;34m"
#define KCYN_L "\033[1;36m"
#define KBRN "\033[0;33m"
#define RESET "\033[0m"

static int destroy_flag = 0;
static int connection_flag = 0;
static int writeable_flag = 0;

char *server = "localhost";

struct session_data {
    int fd;
};

struct pthread_routine_tool {
    struct lws_context *context;
    struct lws *wsi;
};

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

    out = (char *)malloc(sizeof(char)*(LWS_SEND_BUFFER_PRE_PADDING + len));
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
            printf(KYEL"[Main Service] Connect with server success.\n"RESET);
            connection_flag = 1;
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            printf(KRED"[Main Service] Connect with server error.\n"RESET);
            destroy_flag = 1;
            connection_flag = 0;
            break;

        case LWS_CALLBACK_CLOSED:
            printf(KYEL"[Main Service] LWS_CALLBACK_CLOSED\n"RESET);
            destroy_flag = 1;
            connection_flag = 0;
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            printf(KCYN_L"[Main Service] Client recvived:%s\n"RESET, (char *)in);

            if (writeable_flag)
                destroy_flag = 1;

            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE :
            printf(KYEL"[Main Service] On writeable is called. send byebye message\n"RESET);
            //websocket_write_back(wsi, "Byebye! See you later", -1);
            writeable_flag = 1;
            break;

        default:
            break;
    }

    return 0;
}

//When sigterm signal is caught, cleanly exit the plugin
static int terminate(int signum) {
    destroy_flag = 1;
}

int main(int argc, char *argv[])
{
    //setup signal handler to detect if the mirror
    //wants to terminate this program.
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));

    action.sa_handler = terminate;
    sigaction(SIGTERM, &action, NULL);

    //start of program here
    lws_set_log_level(0, NULL);

    char protoName[1024];
    strcpy(protoName, argv[1]);

    struct lws_context *context = NULL;
    struct lws_context_creation_info info;
    struct lws *wsi = NULL;
    struct lws_protocols protocol;
    struct lws_client_connect_info i;

    memset(&info, 0, sizeof info);
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = &protocol;
    info.max_http_header_pool = 128;
    info.gid = -1;
    info.uid = -1;

    protocol.name  = protoName;
    protocol.callback = ws_service_callback;
    protocol.per_session_data_size = sizeof(struct session_data);
    protocol.rx_buffer_size = 125;

    printf("Creating context\n");
    context = lws_create_context(&info);
    printf(KRED"[Main] context created.\n"RESET);

    if (context == NULL) {
        printf(KRED"[Main] context is NULL.\n"RESET);
        return -1;
    }

    printf("Connecting...\n");

    const char *prot;
    if (lws_parse_uri(server, &prot, &i.address, &i.port, &i.path)) {
        printf("Client.c: Error parsing uri\n");
        exit(1);
    }


    i.port = 5000;
    i.context = context;
    i.ssl_connection = 0;
    i.host = i.address;
    i.origin = i.address;
    i.ietf_version_or_minus_one = -1;
    i.protocol = protoName;


    //struct lws *wsi = NULL;
    const struct timespec sleepTime = {
            0, NANOSECOND/FPS
    };

    char command[256];
    int left = 0, dir = 1;

    if (!connection_flag || !wsi) {
        wsi = lws_client_connect_via_info(&i);
    }

    while(!destroy_flag) {

        lws_service(context, 0);

        if (!connection_flag || !wsi) continue;
        left += (dir * SPEED);

        if (left > SCREEN_WIDTH) dir = -dir;
        else if (left < 0) dir = -dir;

        sprintf(command, "{\"command\":\"setcss\",\"data\":\"left=%dpx\"}", left);
        websocket_write_back(wsi, command, -1);
        nanosleep(&sleepTime, NULL);
    }

    lws_context_destroy(context);
    return 0;
}
