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
#include <curl/curl.h>
#include <sys/msg.h>

#include "gfserver.h"
#include "proxy-student.h"
#include "shm_channel.h"
#include "simplecache.h"

#if !defined(CACHE_FAILURE)
#define CACHE_FAILURE (-1)
#endif // CACHE_FAILURE

#define MAX_CACHE_REQUEST_LEN 8803

static void _sig_handler(int signo){
	if (signo == SIGINT || signo == SIGTERM){
		/* Unlink IPC mechanisms here*/
		exit(signo);
	}
}

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -t [thread_count]   Thread count for work queue (Default: 2, Range: 1-8803)\n"      \
"  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"cachedir",           required_argument,      NULL,           'c'},
  {"nthreads",           required_argument,      NULL,           't'},
  {"help",               no_argument,            NULL,           'h'},
  {"hidden",			 no_argument,			 NULL,			 'i'}, /* server side */
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}

static int msqid;

void serve_cache(){
    req_msg msg;
    cache_p cblock;
    int fd;
    ssize_t filelen, readlen, datalen, transfered=0;
    while(1){
        if (msgrcv(msqid, &msg, sizeof(msg.req), 0, 0) == -1) {
            perror("msgrcv");
            exit(1);
        }
        printf("Received request key: %s", msg.req.path);
        cblock = (cache_p) shmat(msg.req.shmd, (void *)0, 0);
        fd = simplecache_get(msg.req.path);
        datalen = data_length(msg.req.segsize);
        filelen=lseek(fd, 0, SEEK_END);

        pthread_mutex_lock(&cblock->meta.m);
        while(cblock->meta.status == READABLE){
            pthread_cond_wait(&cblock->meta.writable, &cblock->meta.m);
        }
        cblock->meta.filelen = (size_t)filelen;
        cblock->meta.status = READABLE;
        pthread_mutex_unlock(&cblock->meta.m);
        pthread_cond_signal(&cblock->meta.readable);

        while(transfered<filelen){
            pthread_mutex_lock(&cblock->meta.m);
            while(cblock->meta.status == READABLE){
                pthread_cond_wait(&cblock->meta.writable, &cblock->meta.m);
            }
            readlen = read(fd, cblock->data, (size_t)datalen);
            if (readlen <= 0){
                fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", readlen, transfered, filelen );
                return;
            }
            transfered += readlen;
            cblock->meta.readlen = (size_t)readlen;
            cblock->meta.status = READABLE;
            pthread_mutex_unlock(&cblock->meta.m);
            pthread_cond_signal(&cblock->meta.readable);
        }
    }
}

void spawn(int n){
    pthread_t tid;
    for (int i=0;i<n;i++){
        pthread_create(&tid, NULL, serve_cache, NULL);
    }
    printf("Threads spawned!\n");
}

int main(int argc, char **argv) {
	int nthreads = 2;
	char *cachedir = "locals.txt";
	char option_char;

	/* disable buffering to stdout */
	setbuf(stdout, NULL);

	while ((option_char = getopt_long(argc, argv, "ic:ht:", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			case 'c': //cache directory
				cachedir = optarg;
				break;
			case 'h': // help
				Usage();
				exit(0);
				break;    
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;   
			case 'i': // server side usage
				break;
			default:
				Usage();
				exit(1);
		}
	}

	if ((nthreads>8803) || (nthreads < 1)) {
		fprintf(stderr, "Invalid number of threads\n");
		exit(__LINE__);
	}

	if (SIG_ERR == signal(SIGINT, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGINT...exiting.\n");
		exit(CACHE_FAILURE);
	}

	if (SIG_ERR == signal(SIGTERM, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGTERM...exiting.\n");
		exit(CACHE_FAILURE);
	}

	/* Cache initialization */
	simplecache_init(cachedir);

    msqid = getmsqid();
    spawn(nthreads);
    /* this code probably won't execute */
    if (msgctl(msqid, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(1);
    }
    simplecache_destroy();
    return 0;
}
