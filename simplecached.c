#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>

#include "shm_channel.h"
#include "simplecache.h"

#if !defined(CACHE_FAILURE)
#define CACHE_FAILURE (-1)
#endif // CACHE_FAILURE

#define MAX_CACHE_REQUEST_LEN 512

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
"  -h                  Show this help message\n"                              \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"cachedir",           required_argument,      NULL,           'c'},
  {"help",               no_argument,            NULL,           'h'},
  {"nthreads",           required_argument,      NULL,           't'},
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}

int main(int argc, char **argv) {
	int nthreads = 1;
	char *cachedir = "locals.txt";
	char option_char;


	while ((option_char = getopt_long(argc, argv, "t:c:h", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;   
			case 'c': //cache directory
				cachedir = optarg;
				break;
			case 'h': // help
				Usage();
				exit(0);
				break;    
			default:
				Usage();
				exit(1);
		}
	}

	if ((nthreads < 1) || (nthreads>1024)) {
		nthreads = 1;
	}

	if (signal(SIGINT, _sig_handler) == SIG_ERR){
		fprintf(stderr,"Can't catch SIGINT...exiting.\n");
		exit(CACHE_FAILURE);
	}

	if (signal(SIGTERM, _sig_handler) == SIG_ERR){
		fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
		exit(CACHE_FAILURE);
	}

	/* Initializing the cache */
	simplecache_init(cachedir);

	//Your code here...
}
