#ifndef __GF_SERVER_H__
#define __GF_SERVER_H__


/*
 * gfserver is a server library for transferring files using the GETFILE
 * protocol.
 */
#include <stdio.h>
#include <pthread.h>
#include "steque.h"
#define MAX_REQUEST_LEN 128

typedef int gfstatus_t;

#define GF_OK 200
#define GF_FILE_NOT_FOUND 400
#define GF_ERROR 500
#define GF_INVALID 300


#if !defined(SERVER_FAILURE)
#define SERVER_FAILURE (-1)
#endif // SERVER_FAILURE

typedef struct gfcontext_t{
    int connfd;
} gfcontext_t;

typedef ssize_t (*handler_t)(gfcontext_t *, char *, void*);
typedef struct gfserver_t{
    handler_t handler;
    int maxnpending;
    int nthreads;
    unsigned short port;
    steque_t Q;
    pthread_mutex_t m;
    pthread_cond_t nonempty;
    void** args;
} gfserver_t;

typedef enum{
  GFS_PORT,
  GFS_MAXNPENDING,
  GFS_WORKER_FUNC,
  GFS_WORKER_ARG
} gfserver_option_t;

/* 
 * Initializes the input gfserver_t object to use nthreads.
 */
void gfserver_init(gfserver_t *gfh, int nthreads);

/* 
 * Sets options for the gfserver_t object. The table below
 * lists the values for option in the left column and the
 * additional arguments in the right.
 *
 * GFS_PORT 	  		unsigned short indicating the port on which 
 * 						the server should receive connections.
 *
 * GFS_MAXNPENDING 	 	int indicating the maximum number of pending
 * 						connections the receiving socket should permit.
 *
 * GFS_WORKER_FUNC 		a function pointer with the signature
 * 						ssize_t (*)(gfcontext_t *, char *, void*);
 *
 *						The first argument contains the needed context
 *						information for the server and should be passed
 *						into calls to gfs_sendheader and gfs_send.
 *						
 *						The second argument is the path of the requested
 *						resource.
 * 
 * 						The third argument is the argument registered for
 *						this particular thread with the GFS_WORKER_ARG
 *						option.
 *
 *						Returning a negative number will cause the 
 *						gfserver library to send an error message to the
 *						client.  Otherwise, gfserver will assume that
 *						this function has performed all the necessary 
 *						communication.
 *
 *
 * GFS_WORKER_ARG		This option is followed by two arguments, an int
 *						indicating the thread index [0,...,nthreads-1] and
 * 						a pointer which will be passed into the callback
 * 						registered via the GFS_WORKER_FUNC option on this 
 *						thread.
 *						
 */
void gfserver_setopt(gfserver_t *gfh, gfserver_option_t option, ...);
/*
 * Starts the server.  Does not return.
 */
void gfserver_serve(gfserver_t *gfs);

/*
 * Shuts down the server associated with the input gfserver_t object.  
 */
void gfserver_stop(gfserver_t *gfh);

/*
 * Sends to the client the Getfile header containing the appropriate 
 * status and file length for the given inputs.  This function should
 * only be called from within a callback registered gfserver_set_handler.
 */
ssize_t gfs_sendheader(gfcontext_t *ctx, gfstatus_t status, size_t file_len);

/*
 * Sends size bytes starting at the pointer data to the client 
 * This function should only be called from within a callback registered 
 * with gfserver_set_handler.  It returns once the data has been
 * sent.
 */
ssize_t gfs_send(gfcontext_t *ctx, void *data, size_t size);

/*
 * Aborts the connection to the client associated with the input
 * gfcontext_t.
 */
void gfs_abort(gfcontext_t *ctx);

#endif
