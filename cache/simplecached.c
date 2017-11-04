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
static int msqid;
static void _sig_handler(int signo){
	if (signo == SIGINT || signo == SIGTERM){
		/* Unlink IPC mechanisms here*/
        if (msqid != -1){
            destroy_msg(msqid);
        }
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

static int msqid=-1;

void* serve_cache(void*arg){
    req_msg msg;
    cache_p cblock;
    int fd;
    ssize_t filelen, readlen, datalen, transfered;
    int shmid;
    while(1){
        if (msgrcv(msqid, &msg, sizeof(msg.req), 1, 0) == -1) {
            perror("msgrcv");
            exit(1);
        }
        printf("Received request key %s with memory key %d\n", msg.req.path, msg.req.key);
        /* connect to (and possibly create) the segment: */
        if ((shmid = shmget(msg.req.key, msg.req.segsize, 0644 | IPC_CREAT)) == -1) {
            perror("shmget");
            exit(1);
        }
        cblock = (cache_p) shmat(shmid, (void *)0, 0);
        datalen = data_length(msg.req.segsize);
        transfered = 0;
        if((fd = simplecache_get(msg.req.path))<0){
            filelen=-1;
            printf("File does not exist in cache!\n");
        }
        else{
            filelen=lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
        }
        printf("Get fd of %s with size %zd\n", msg.req.path, filelen);
        pthread_mutex_lock(&cblock->meta.m);
        while(cblock->meta.status == READABLE){
            pthread_cond_wait(&cblock->meta.writable, &cblock->meta.m);
        }
        cblock->meta.filelen = filelen;
        if(filelen<0){
            cblock->meta.status=READABLE;
        }
        pthread_mutex_unlock(&cblock->meta.m);
        if(filelen<0){
            pthread_cond_signal(&cblock->meta.readable);
        }

        printf("Metadata set! Starting transfer...\n");
        while(transfered<filelen){
            pthread_mutex_lock(&cblock->meta.m);
            while(cblock->meta.status == READABLE){
                pthread_cond_wait(&cblock->meta.writable, &cblock->meta.m);
            }
            readlen = read(fd, cblock->data, (size_t)datalen);
//            printf("Read: %.40s...\n", cblock->data);
            if (readlen <= 0){
                fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", readlen, transfered, filelen );
                return NULL;
            }
            transfered += readlen;
//            printf("Read %zd bytes, transfered %zu, filelen %zu\n", readlen, transfered, filelen);
            cblock->meta.readlen = (size_t)readlen;
            cblock->meta.status = READABLE;
            pthread_mutex_unlock(&cblock->meta.m);
            pthread_cond_signal(&cblock->meta.readable);
        }
        printf(">>> Successfully sent cached file!\n");
        if(shmdt(cblock)<0){
            perror("shmdt");
        }
        printf(">>> Successfully detached shared memory block!\n");
    }
}

void spawn(int n){
    pthread_t tid[n];
    for (int i=0;i<n;i++){
        pthread_create(&tid[i], NULL, serve_cache, NULL);
    }
    printf("Threads spawned!\n");
    for (int i = 0; i < n; i++)
       pthread_join(tid[i], NULL);
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
    printf("Initialized Cache!\n");
    msqid = getmsqid();
    printf("Got messsage quene!\n");
    spawn(nthreads);
    /* this code probably won't execute */
    if (msgctl(msqid, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(1);
    }
    simplecache_destroy();
    return 0;
}
