#include <getopt.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>

#include "gfserver.h"
#include "content.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  gfserver_main [options]\n"                                                 \
"options:\n"                                                                  \
"  -t [nthreads]       Number of threads (Default: 3)\n"                      \
"  -p [listen_port]    Listen port (Default: 8803)\n"                         \
"  -m [content_file]   Content file mapping keys to content files\n"          \
"  -h                  Show this help message.\n"                             \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"port",          required_argument,      NULL,           'p'},
    {"nthreads",      required_argument,      NULL,           't'},
    {"content",       required_argument,      NULL,           'm'},
    {"help",          no_argument,            NULL,           'h'},
    {NULL,            0,                      NULL,             0}
};


extern ssize_t handler_get(gfcontext_t *ctx, char *path, void* arg);

static void _sig_handler(int signo) {
    if(signo == SIGINT || signo == SIGTERM) {
        exit(signo);
    }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
    int option_char = 0;
    unsigned short port = 8803;
    char *content_map = "content.txt";
    gfserver_t gfs;
    int nthreads = 3;

    if(signal(SIGINT, _sig_handler) == SIG_ERR) {
        fprintf(stderr, "Can't catch SIGINT...exiting.\n");
        exit(EXIT_FAILURE);
    }

    if(signal(SIGTERM, _sig_handler) == SIG_ERR) {
        fprintf(stderr, "Can't catch SIGTERM...exiting.\n");
        exit(EXIT_FAILURE);
    }

    // Parse and set command line arguments
    while((option_char = getopt_long(argc, argv, "m:p:t:h", gLongOptions, NULL)) != -1) {
        switch(option_char) {
            default:
                fprintf(stderr, "%s", USAGE);
                exit(1);
            case 'p': // listen-port
                port = atoi(optarg);
                break;
            case 't': // nthreads
                nthreads = atoi(optarg);
                break;
            case 'h': // help
                fprintf(stdout, "%s", USAGE);
                exit(0);
                break;
            case 'm': // file-path
                content_map = optarg;
                break;
        }
    }

    /* not useful, but it ensures the initial code builds without warnings */
    if(nthreads < 1) {
        nthreads = 1;
    }

    content_init(content_map);

    /*Initializing server*/
    gfserver_init(&gfs, nthreads);

    /*Setting options*/
    gfserver_setopt(&gfs, GFS_PORT, port);
    gfserver_setopt(&gfs, GFS_MAXNPENDING, 12);
    gfserver_setopt(&gfs, GFS_WORKER_FUNC, handler_get);
    for(int i = 0; i < nthreads; i++) {
        gfserver_setopt(&gfs, GFS_WORKER_ARG, i, NULL);
    }
    /*Loops forever*/
    gfserver_serve(&gfs);
}
