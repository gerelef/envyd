#ifndef NETWORK_H
#define NETWORK_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <unistd.h>
#include <nvml.h>
#include <json-c/json.h>
#include "helpers.h"

#define SO_INPUT_BUFFER_SIZE 8192

#define JSON_PARSING_FAILED "JSON_PARSING_FAILED"
#define INVALID_JSON_SCHEMA "INVALID_JSON_SCHEMA"
#define UNDEFINED_INVALID_ACTION "UNDEFINED_INVALID_ACTION"

typedef struct networkError_st {
    char* status_line;
    char* description;
} networkError_st;

typedef struct networkResponse_st {
    char* data;
    networkError_st error;
} networkResponse_st;

#define STRINGIFY_NULLABLE(s) s == NULL ? "null" : s

#define RESPOND(client_fd, datum, status, desc) do { \
    if (so_buffer == NULL) WTF("I/O buffer is null?!"); \
    unsigned long long bytes_to_send = snprintf(so_buffer, SO_INPUT_BUFFER_SIZE, "{ \"data\": %s, \"status\": \"%s\", \"description\": \"%s\"}", STRINGIFY_NULLABLE(datum), STRINGIFY_NULLABLE(status), STRINGIFY_NULLABLE(desc)); \
    ssize_t written = write(client_fd, so_buffer, bytes_to_send); \
    PRINTLN_SO("Writing to client_fd %d: %s", client_fd, so_buffer); \
    if (written < 0) PRINTLN_SO("Couldn't write to fd %d", client_fd); \
    } while (0)

typedef struct networkRequest_st {
    // TODO in the future add bearer field
    char* action;
    char* arguments[];
} networkRequest_st;

void process(const int client_fd, const struct timeval* tv_timeout);

int bind_socket_with_address(const char *address);

#endif