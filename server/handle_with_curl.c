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

/**
 * @brief send_chunk Callback function for CURLOPT_WRITEFUNCTION to send a chunk
 * @param ptr   pointer to data
 * @param size  size of each element
 * @param nmemb number of elements
 * @param data  gfcontext_t pointer
 * @return
 */
size_t send_chunk(void *ptr, size_t size, size_t nmemb, void *data) {
    size_t realsize = size * nmemb;
    gfcontext_t *ctx = (gfcontext_t *)data;
    ssize_t write_len = gfs_send(ctx, (char*)ptr, realsize);
    if(write_len != (ssize_t)realsize) {
        fprintf(stderr, "Send chunk error\n");
        return 0;
    }
    return realsize;
}

/**
 * @brief handle_with_curl Use curl to handle file request
 * @param ctx
 * @param path Path of requested file
 * @param arg Data directory
 * @return Transfered bytes
 *
 * Use libcurl to intercept received data of libcurl and send it to server
 */
ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg) {
    char buffer[BUFSIZE];
    curl_global_init(CURL_GLOBAL_ALL);
    CURL * myHandle = curl_easy_init();
    CURLcode result;
    double length;
    long status;

    strncpy(buffer, (char *)arg, BUFSIZE);
    strncat(buffer, path, BUFSIZE);
    curl_easy_setopt(myHandle, CURLOPT_NOBODY, 1);
    curl_easy_setopt(myHandle, CURLOPT_URL, buffer);
    result = curl_easy_perform(myHandle);
    if(result == CURLE_OK) {
        curl_easy_getinfo(myHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &length);
        curl_easy_getinfo(myHandle, CURLINFO_RESPONSE_CODE, &status);
    } else {
        fprintf(stderr, "Error happened during curl_easy_perform of header\n");
        return -1;
    }
    if(status >= 400) {
        gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
    } else {
        gfs_sendheader(ctx, GF_OK, (size_t)length);
    }

    curl_easy_setopt(myHandle, CURLOPT_NOBODY, 0);
    curl_easy_setopt(myHandle, CURLOPT_WRITEFUNCTION, send_chunk);
    curl_easy_setopt(myHandle, CURLOPT_WRITEDATA, (void *)ctx);
    result = curl_easy_perform(myHandle);
    if(result != CURLE_OK) {
        fprintf(stderr, "Error happened during curl_easy_perform\n");
        return -1;
    }
    curl_easy_cleanup(myHandle);
    curl_global_cleanup();
    return (ssize_t)length;
}

