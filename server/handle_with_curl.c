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

#include "gfserver.h"
#include "proxy-student.h"

#define BUFSIZE (8803)

/*
 * Replace with an implementation of handle_with_curl and any other
 * functions you may need.
 */
ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg){
	(void) ctx;
	(void) path;
	(void) arg;

	/* not implemented */
	errno = ENOSYS;
	return -1;
}


/*
 * We provide a dummy version of handle_with_file that invokes handle_with_curl
 * as a convenience for linking.  We recommend you simply modify the proxy to
 * call handle_with_curl directly.
 */
ssize_t handle_with_file(gfcontext_t *ctx, char *path, void* arg){
	return handle_with_curl(ctx, path, arg);
}	