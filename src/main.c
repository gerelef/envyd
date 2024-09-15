#include <stdio.h>
#include <stdlib.h>
#include <nvml.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <nvdialog.h>
#include "network.h"
#include "helpers.h"

#define SERVER_UNIX_PATH "/tmp/envyd.socket"

char *so_buffer = NULL;  // global; use for socket IO
int server_fd = -1;  // global; use to close fd gracefully upon death
nvmlReturn_t gl_nvml_result; // global; use for panics
logLevel_t current_log_level; // global; use for logging
char *log_buffer = NULL;  // global; use for logging

void die_gracefully(const int signal_code) {
    LOG_INFO("Received signal '%s', closing gracefully...", strsignal(signal_code));
    if (server_fd != -1) {
        LOG_WARNING("Closing server socket fd %d", server_fd);
        close(server_fd);
        unlink(SERVER_UNIX_PATH);
    }

    gl_nvml_result = nvmlShutdown();
    if (FATAL(gl_nvml_result)) WTF("Failed to shutdown NVML");
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    current_log_level = TRACE;

    sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);
    signal(SIGINT, die_gracefully);
    signal(SIGTERM, die_gracefully);

    gl_nvml_result = nvmlInit_v2();
    if (ERROR(gl_nvml_result) || FATAL(gl_nvml_result)) WTF("Failed to initialize NVML!");
    if (nvd_init() != 0) WTF("Failed to initialize NvDialog!");

    // timeout
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;

    server_fd = bind_socket_with_address(SERVER_UNIX_PATH);
    LOG_INFO("Server started on 'envyd'!");
    LOG_INFO("Stop via SIGTERM or SIGINT (CTRL+C)");
    // ReSharper disable once CppDFAEndlessLoop
    for (;;) {
        if (listen(server_fd, 15) < 0) WTF("Listen failed!");
        LOG_INFO("Waiting for connection...");

        const int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            LOG_ERROR("Accept failed!");
            continue;
        }

        LOG_INFO("Connection established on fd %d!", client_fd);
        process(client_fd, &tv);
        close(client_fd);
    }
}
