#include <stdio.h>
#include <stdlib.h>
#include <nvml.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "network/network.h"
#include "helpers.h"

#define SERVER_UNIX_PATH "/tmp/envyd.socket"

// TODO eventually add security measure, whereby we connect to
//  the current session via ibus, and ask for permission for a client
//  'protected' action should be executed (bearer token scheme)

int server_fd = -1;
nvmlReturn_t gl_nvml_result;  // global; use for panics

void die_gracefully(int signal_code) {
    PRINTLN("Received signal %d, closing gracefully...", signal_code);
    if (server_fd != -1) {
        PRINTLN("Closing server socket fd %d", server_fd);
        close(server_fd);
        unlink(SERVER_UNIX_PATH);
    }
    exit(EXIT_SUCCESS);
}

int main() {
    signal(SIGINT, die_gracefully);
    signal(SIGTERM, die_gracefully);

    server_fd = bind_socket_with_address(SERVER_UNIX_PATH);
    PRINTLN("Server started on 'envyd'!");
    PRINTLN("Stop via SIGTERM or SIGINT (CTRL+C)");
    // ReSharper disable once CppDFAEndlessLoop
    for (;;) {
        if (listen(server_fd, 15) < 0) PANIC("Listen failed!");
        PRINTLN("Waiting for connection...");

        const int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            PRINTLN("Accept failed!");
            continue;
        }
        PRINTLN("Connection established on fd %d!", client_fd);
        process(client_fd);
        close(client_fd);
    }
    // gl_nvml_result = nvmlInit_v2();
    // if (gl_nvml_result != NVML_SUCCESS) PANIC("Failed to initialize NVML");
    //
    // unsigned int device_count = -1;
    // gl_nvml_result = nvmlDeviceGetCount_v2(&device_count);
    // PRINTLN("Device count: %d", device_count);
    //
    // for (int deviceIndex = 0; deviceIndex < device_count; ++deviceIndex) {
    //     nvmlDevice_t device;
    //     gl_nvml_result = nvmlDeviceGetHandleByIndex_v2(deviceIndex, &device);
    //     if (gl_nvml_result != NVML_SUCCESS) PANIC("Failed to get device handle for index %d", deviceIndex);
    //
    //     char *uuid = malloc(64);
    //     gl_nvml_result = nvmlDeviceGetUUID(device, uuid, 64);
    //     if (gl_nvml_result != NVML_SUCCESS) PANIC("Failed to get uuid for device %d", deviceIndex);
    //
    //     char *name = malloc(128);
    //     gl_nvml_result = nvmlDeviceGetName(device, name, 128);
    //     if (gl_nvml_result != NVML_SUCCESS) PANIC("Failed to get name for device %d", deviceIndex);
    //
    //     char *gsp_version = malloc(64);
    //     gl_nvml_result = nvmlDeviceGetGspFirmwareVersion(device, gsp_version);
    //     if (gl_nvml_result != NVML_SUCCESS) PANIC("Failed to get gsp version for device %d", deviceIndex);
    //
    //     unsigned int is_enabled = -1;
    //     unsigned int is_supported_by_default = -1;
    //     gl_nvml_result = nvmlDeviceGetGspFirmwareMode(device, &is_enabled, &is_supported_by_default);
    //     if (gl_nvml_result != NVML_SUCCESS) PANIC("Failed to get gsp details for device %d", deviceIndex);
    //
    //     nvmlMemory_v2_t memory;
    //     // jesus fucking christ https://github.com/NVIDIA/nvidia-settings/issues/78#issuecomment-1012837988
    //     memory.version = (unsigned int)(sizeof(nvmlMemory_v2_t) | 2 << 24U);
    //     gl_nvml_result = nvmlDeviceGetMemoryInfo_v2(device, &memory);
    //     if (gl_nvml_result != NVML_SUCCESS) PANIC("Failed to get memory info for device %d", deviceIndex);
    //
    //     PRINTLN("------- DEVICE (%.2f%% full)-------", ((float) memory.used / (float) memory.total) * 100);
    //     PRINTLN("Named: %s", name);
    //     PRINTLN("UUID: %s", uuid);
    //     PRINTLN("GSP version: %s", gsp_version);
    //     PRINTLN("GSP supported by default: %u", is_supported_by_default);
    //     PRINTLN("GSP enabled: %u", is_enabled);
    //     PRINTLN("Total memory: %ld", (long) bytes_to_denominator(MEGABYTES, memory.total));
    //     PRINTLN("Free memory: %f", bytes_to_denominator(MEGABYTES, memory.free));
    //     PRINTLN("Used memory: %f", bytes_to_denominator(MEGABYTES, memory.used));
    //     PRINTLN("Reserved memory: %f", bytes_to_denominator(MEGABYTES, memory.reserved));
    //
    //     free(gsp_version);
    //     free(name);
    //     free(uuid);
    // }
    //
    // gl_nvml_result = nvmlShutdown();
    // if (gl_nvml_result != NVML_SUCCESS) PANIC("Failed to shutdown NVML");
    // return 0;
}