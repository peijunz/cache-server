#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <getopt.h>
#include <pthread.h>

#include "steque.h"
#include "gfclient.h"
#include "workload.h"

//#include "gfclient-student.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webclient [options]\n"                                                     \
"options:\n"                                                                  \
"  -h                  Show this help message\n"                              \
"  -n [num_requests]   Requests download per thread (Default: 2)\n"           \
"  -p [server_port]    Server port (Default: 8803)\n"                         \
"  -s [server_addr]    Server address (Default: 0.0.0.0)\n"                   \
"  -t [nthreads]       Number of threads (Default 2)\n"                       \
"  -w [workload_path]  Path to workload file (Default: workload.txt)\n"       \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"help",          no_argument,            NULL,           'h'},
    {"nthreads",      required_argument,      NULL,           't'},
    {"nrequests",     required_argument,      NULL,           'n'},
    {"server",        required_argument,      NULL,           's'},
    {"port",          required_argument,      NULL,           'p'},
    {"workload-path", required_argument,      NULL,           'w'},
    {NULL,            0,                      NULL,             0}
};

static void Usage() {
    fprintf(stdout, "%s", USAGE);
}

static void localPath(char *req_path, char *local_path) {
    static int counter = 0;

    sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

static FILE* openFile(char *path) {
    char *cur, *prev;
    FILE *ans;

    /* Make the directory if it isn't there */
    prev = path;
    while(NULL != (cur = strchr(prev + 1, '/'))) {
        *cur = '\0';

        if(0 > mkdir(&path[0], S_IRWXU)) {
            if(errno != EEXIST) {
                perror("Unable to create directory");
                exit(EXIT_FAILURE);
            }
        }

        *cur = '/';
        prev = cur;
    }

    if(NULL == (ans = fopen(&path[0], "w"))) {
        perror("Unable to open file");
        exit(EXIT_FAILURE);
    }

    return ans;
}

/* Callbacks ========================================================= */
static void writecb(void* data, size_t data_len, void *arg) {
    FILE *file = (FILE*) arg;

    fwrite(data, 1, data_len, file);
}


typedef struct item {
    char* local_path;
    char* req_path;
    char* server;
    unsigned short port;
} item;

static steque_t Q;

static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t task = PTHREAD_COND_INITIALIZER;
static int busy = 0;
static pthread_cond_t done = PTHREAD_COND_INITIALIZER;

void* thread(void*args) {
    item *it;
    gfcrequest_t *gfr = NULL;
    FILE* file;
    int returncode, tmp;
    while(1) {
        // Task quene waiting
        pthread_mutex_lock(&m);
        while(steque_isempty(&Q)) {
            pthread_cond_wait(&task, &m);
        }
        it = steque_pop(&Q);
        pthread_mutex_unlock(&m);

        // Busy task number

        file = openFile(it->local_path);

        gfr = gfc_create();
        gfc_set_server(gfr, it->server);
        gfc_set_path(gfr, it->req_path);
        gfc_set_port(gfr, it->port);
        gfc_set_writefunc(gfr, writecb);
        gfc_set_writearg(gfr, file);

        fprintf(stdout, "Requesting %s%s\n", it->server, it->req_path);

        if(0 > (returncode = gfc_perform(gfr))) {
            fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
            fclose(file);
            if(0 > unlink(it->local_path))
                fprintf(stderr, "unlink failed on %s\n", it->local_path);
        } else {
            fclose(file);
        }

        if(gfc_get_status(gfr) != GF_OK) {
            if(0 > unlink(it->local_path))
                fprintf(stderr, "unlink failed on %s\n", it->local_path);
        }

        fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(gfr)));
        fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(gfr), gfc_get_filelen(gfr));

        gfc_cleanup(gfr);

        free(it->req_path);
        free(it->local_path);
        free(it);

        // Busy task number
        pthread_mutex_lock(&m);
        tmp = --busy;
        pthread_mutex_unlock(&m);
        if(tmp == 0) {
            printf("All threads terminated\n");
            pthread_cond_signal(&done);
        };
    }
}

void spawn(int n) {
    pthread_t tid;
    for(int i = 0; i < n; i++) {
        pthread_create(&tid, NULL, thread, NULL);
    }
    printf("Threads spawned!\n");
}

void gfclient_download(char *server, int port, char *req_path, char *local_path) {
    size_t l = strlen(req_path) + 1;
    //Pack data into quene
    item *it = (item*)malloc(sizeof(item));
    it->server = server;
    it->port = port;
    it->req_path = (char*)malloc(l);
    strncpy(it->req_path, req_path, l);
    l = strlen(local_path) + 1;
    it->local_path = (char*)malloc(l);
    strncpy(it->local_path, local_path, l);

    pthread_mutex_lock(&m);
    steque_enqueue(&Q, it);
    busy++;
    pthread_mutex_unlock(&m);

    pthread_cond_signal(&task);
}

// Wait until not busy
void gfclient_join() {
    pthread_mutex_lock(&m);
    while(busy) {
        pthread_cond_wait(&done, &m);
    }
    pthread_mutex_unlock(&m);

    gfc_global_cleanup();
    steque_destroy(&Q);
}

/* Main ========================================================= */
int main(int argc, char **argv) {
    /* COMMAND LINE OPTIONS ============================================= */
    char *server = "localhost";
    unsigned short port = 8803;
    char *workload_path = "workload.txt";

    int i = 0;
    int option_char = 0;
    int nrequests = 2;
    int nthreads = 2;
//  int returcode = 0;
    char *req_path = NULL;
    char local_path[512];

    // Parse and set command line arguments
    while((option_char = getopt_long(argc, argv, "hn:p:s:t:w:", gLongOptions, NULL)) != -1) {
        switch(option_char) {
            case 'h': // help
                Usage();
                exit(0);
                break;
            case 'n': // nrequests
                nrequests = atoi(optarg);
                break;
            case 'p': // port
                port = atoi(optarg);
                break;
            case 's': // server
                server = optarg;
                break;
            case 't': // nthreads
                nthreads = atoi(optarg);
                break;
            case 'w': // workload-path
                workload_path = optarg;
                break;
            default:
                Usage();
                exit(1);
        }
    }

    if(EXIT_SUCCESS != workload_init(workload_path)) {
        fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
        exit(EXIT_FAILURE);
    }

    gfc_global_init();
    steque_init(&Q);
    spawn(nthreads);
    /*Making the requests...*/
    for(i = 0; i < nrequests * nthreads; i++) {
        req_path = workload_get_path();
        size_t l;
        if((l = strlen(req_path)) > 256) {
            fprintf(stderr, "Request path exceeded maximum of 256 characters\n.");
            exit(EXIT_FAILURE);
        }
        localPath(req_path, local_path);
        gfclient_download(server, port, req_path, local_path);
    }
    printf("Quene Initialized\n");

    gfclient_join();

    exit(0);//Kill all threads
}
