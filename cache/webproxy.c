#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <sys/signal.h>
#include <printf.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include "shm_channel.h"
#include "gfserver.h"
#include "proxy-student.h"

/* note that the -n and -z parameters are NOT used for Part 1 */
/* they are only used for Part 2 */
#define USAGE                                                                         \
"usage:\n"                                                                            \
"  webproxy [options]\n"                                                              \
"options:\n"                                                                          \
"  -n [segment_count]  Number of segments to use (Default: 8)\n"                      \
"  -p [listen_port]    Listen port (Default: 8803)\n"                                 \
"  -t [thread_count]   Num worker threads (Default: 2, Range: 1-8803)\n"              \
"  -s [server]         The server to connect to (Default: Udacity S3 instance)\n"     \
"  -z [segment_size]   The segment size (in bytes, Default: 8803).\n"                  \
"  -h                  Show this help message\n"


/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"segment-count", required_argument,      NULL,           'n'},
    {"port",          required_argument,      NULL,           'p'},
    {"thread-count",  required_argument,      NULL,           't'},
    {"server",        required_argument,      NULL,           's'},
    {"segment-size",  required_argument,      NULL,           'z'},
    {"help",          no_argument,            NULL,           'h'},
    {"hidden",        no_argument,            NULL,           'i'}, /* server side */
    {NULL,            0,                      NULL,            0}
};

extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);
extern void init_cache_handlers(int, int);
extern void stop_cache_handlers(void);

static gfserver_t gfs;

static void _sig_handler(int signo) {
    if(signo == SIGINT || signo == SIGTERM) {
        gfserver_stop(&gfs);
        stop_handlers();
        printf("Proxy killed by signal %d\n", signo);
        exit(signo);
    }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
    int i;
    int option_char = 0;
    unsigned short port = 8803;
    unsigned short nworkerthreads = 2;
    unsigned int nsegments = 8;
    size_t segsize = 8803;
    char *server = "s3.amazonaws.com/content.udacity-data.com";

    /* disable buffering on stdout so it prints immediately */
    setbuf(stdout, NULL);

    if(signal(SIGINT, _sig_handler) == SIG_ERR) {
        fprintf(stderr, "Can't catch SIGINT...exiting.\n");
        exit(SERVER_FAILURE);
    }

    if(signal(SIGTERM, _sig_handler) == SIG_ERR) {
        fprintf(stderr, "Can't catch SIGTERM...exiting.\n");
        exit(SERVER_FAILURE);
    }

    /* Parse and set command line arguments */
    while((option_char = getopt_long(argc, argv, "in:p:s:t:z:h", gLongOptions, NULL)) != -1) {
        switch(option_char) {
            case 'n': // segment count
                nsegments = atoi(optarg);
                break;
            case 'p': // listen-port
                port = atoi(optarg);
                break;
            case 's': // file-path
                server = optarg;
                break;
            case 't': // thread-count
                nworkerthreads = atoi(optarg);
                break;
            case 'z': // segment size
                segsize = atoi(optarg);
                break;
            case 'i':
                /* bonnie side only */
                break;
            case 'h': // help
                fprintf(stdout, "%s", USAGE);
                exit(0);
                break;
            default:
                fprintf(stderr, "%s", USAGE);
                exit(__LINE__);
        }
    }

    if(!server) {
        fprintf(stderr, "Invalid (null) server name\n");
        exit(__LINE__);
    }

    if(segsize < 128) {
        fprintf(stderr, "Invalid segment size\n");
        exit(__LINE__);
    }

    if(nsegments < 1) {
        fprintf(stderr, "Must have a positive number of segments\n");
        exit(__LINE__);
    }

    if(port < 1024) {
        fprintf(stderr, "Invalid port number\n");
        exit(__LINE__);
    }

    if((nworkerthreads < 1) || (nworkerthreads > 8803)) {
        fprintf(stderr, "Invalid number of worker threads\n");
        exit(__LINE__);
    }


    /* This is where you initialize your shared memory 初始化内存*/
    if(data_length(segsize) < 128) {
        fprintf(stderr, "Segment size %lu smaller than metadata size %lu+128!\n", segsize, sizeof(metadata));
        exit(__LINE__);
    }
    init_cache_handlers((int)segsize, (int)nsegments);
    /* This is where you initialize the server struct */
    gfserver_init(&gfs, nworkerthreads);

    /* This is where you set the options for the server */
    gfserver_setopt(&gfs, GFS_PORT, port);
    gfserver_setopt(&gfs, GFS_MAXNPENDING, 42);
    gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);
    for(i = 0; i < nworkerthreads; i++) {
        gfserver_setopt(&gfs, GFS_WORKER_ARG, i, server);
    }

    /* This is where you invoke the framework to run the server */
    /* Note that it loops forever */
    gfserver_serve(&gfs);
}
