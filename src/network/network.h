#ifndef NETWORK_H
#define NETWORK_H

typedef struct networkError_st {
    char* status_line;
    char* description;
} networkError_st;

typedef struct networkResponse_st {
    char* data;
    networkError_st error;
} networkResponse_st;

#define RESPONSE(response_st, datum, status, desc) do { \
    if (datum != NULL) response_st.data = datum; \
    if (status != NULL) response_st.error.status_line = status; \
    if (desc != NULL) response_st.error.description = desc; \
    } while (0)

typedef struct networkRequest_st {
    // TODO in the future add bearer field
    char* action;
    char* arguments[];
} networkRequest_st;

void process(int fd);

// ------------------- REQUEST -------------------
// {
//     "bearer" (optional): "token",
//     "action" (required): "functionName",
//     "args" (required): [
//         arg1,
//         arg2,
//         arg3,
//         ...
//     ]
// }
// ------------------- RESPONSE -------------------
// {
//     "data" : {
//         // filled with query-specific data; usually 1:1 if returning structs
//         // - this will be null in case of errors
//         // - this will also be null in cases where return type is 'void' (of the appropriate NVML API)
//     },
//     "status": "NVML_SUCCESS"  // or any other value from nvmlReturn_t
// }

int bind_socket_with_address(const char *address);

#endif