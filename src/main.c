#include <stdio.h>
#include <stdlib.h>
#include <nvml.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "network.h"
#include "helpers.h"

#define SERVER_UNIX_PATH "/tmp/envyd.socket"

// TODO eventually add security measure, whereby we connect to
//  the current session via ibus, and ask for permission for a client
//  'protected' action should be executed (bearer token scheme)

char *so_buffer = NULL;
int server_fd = -1;  // global; use to close fd gracefully upon death
nvmlReturn_t gl_nvml_result;  // global; use for panics

void die_gracefully(const int signal_code) {
    PRINTLN("Received signal '%s', closing gracefully...", strsignal(signal_code));
    if (server_fd != -1) {
        PRINTLN("Closing server socket fd %d", server_fd);
        close(server_fd);
        unlink(SERVER_UNIX_PATH);
    }

    gl_nvml_result = nvmlShutdown();
    if (FATAL(gl_nvml_result)) WTF("Failed to shutdown NVML");
    exit(EXIT_SUCCESS);
}

int main() {
    sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);
    signal(SIGINT, die_gracefully);
    signal(SIGTERM, die_gracefully);

    gl_nvml_result = nvmlInit_v2();
    if (ERROR(gl_nvml_result) || FATAL(gl_nvml_result)) WTF("Failed to initialize NVML!");

    // timeout
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;

    server_fd = bind_socket_with_address(SERVER_UNIX_PATH);
    PRINTLN("Server started on 'envyd'!");
    PRINTLN("Stop via SIGTERM or SIGINT (CTRL+C)");
    for (;;) {
        if (listen(server_fd, 15) < 0) WTF("Listen failed!");
        PRINTLN("Waiting for connection...");

        const int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            PRINTLN("Accept failed!");
            continue;
        }

        PRINTLN("Connection established on fd %d!", client_fd);
        process(client_fd, &tv);
        close(client_fd);
    }
}