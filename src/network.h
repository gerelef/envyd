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
#define AUTHORIZATION_FAILED "AUTHORIZATION_FAILED"
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
    size_t datum_len = datum != NULL ? strlen(datum) : 4; \
    size_t status_len = status != NULL ? strlen(status) : 4; \
    size_t desc_len = desc != NULL ? strlen(desc) : 4; \
    size_t len = datum_len + status_len + desc_len + 96; \
    char* send_buffer = calloc(sizeof(char), len); \
    unsigned long long bytes_to_send = snprintf(send_buffer, len - 1, "{ \"data\": %s, \"status\": %s%s%s, \"description\": %s%s%s}", \
        STRINGIFY_NULLABLE(datum), \
        status == NULL ? "" : "\"", STRINGIFY_NULLABLE(status), status == NULL ? "" : "\"", \
        desc == NULL   ? "" : "\"", STRINGIFY_NULLABLE(desc), desc == NULL     ? "" : "\""  \
    ); \
    ssize_t written = write(client_fd, send_buffer, bytes_to_send); \
    LOG_INFO("Writing to client_fd %d: %s", client_fd, send_buffer); \
    if (written < 0) LOG_ERROR("Couldn't write to fd %d", client_fd); \
    free(send_buffer); \
    } while (0)

#ifdef INSECURE
#define CHECK_AUTHORIZATION(api, jobj) do {} while(0)  // no-op
#else
#define CHECK_AUTHORIZATION(api, jobj) do { \
        json_object *bearer_field = json_object_object_get(jobj, "bearer"); \
        if (bearer_field == NULL) { \
            PRINTLN_SO("Invalid JSON schema: 'bearer' field does not exist in $ (root) jobj"); \
            RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'bearer' field does not exist in $ (root) jobj"); \
            return; \
        } \
        \
        const char *bearer = json_object_get_string(bearer_field); \
        if (bearer == NULL) { \
            PRINTLN_SO("Invalid JSON schema: 'bearer' field does have a valid value"); \
            RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'bearer' field does have a valid value"); \
            return; \
        } \
        if (is_authorized(bearer)) break; \
        PRINTLN_SO(AUTHORIZATION_FAILED, api); \
        RESPOND(client_fd, NULL, AUTHORIZATION_FAILED, AUTHORIZATION_FAILED); \
    } while (0)
#endif

typedef struct networkRequest_st {
    // TODO in the future add bearer field
    char* action;
    char* arguments[];
} networkRequest_st;

void process(const int client_fd, const struct timeval* tv_timeout);

int bind_socket_with_address(const char *address);

#endif