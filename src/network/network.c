#include "network.h"

#include <assert.h>

#include "../helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <nvml.h>

extern nvmlReturn_t gl_nvml_result;  // global; use for panics

// void read_request(const int client_fd, cJSON* request /* out */);
void send_response(const int client_fd, const networkResponse_st* response);

int bind_socket_with_address(const char* address) {
    assert(address != NULL);  // sanity

    const int fd_server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd_server_socket < 0) PANIC("Could not create socket!");
    struct sockaddr_un sock_addr;
    sock_addr.sun_family = AF_UNIX;
    strcpy(sock_addr.sun_path, address);  // strcpy blows, memcpy should be used here
    if (access(address, F_OK) == 0) unlink(address);  // remove the file if it exists from a zombie process or whatever the fuck
    if (bind(fd_server_socket, (struct sockaddr *) &sock_addr, sizeof(struct sockaddr_un)) == -1) PANIC("Could not bind socket!");
    return fd_server_socket;
}

void process(const int client_fd) {
    assert(client_fd >= 0);  // sanity
    // TODO assign according to 'action' element

    // TODO free
}

void assign_task(const char* action) {
    assert(action != NULL);  // sanity
    if (strcmp(action, "nvmlDeviceGetTemperature")) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1g92d1c5182a14dd4be7090e3c1480b121
        // TODO impl
    } else if (strcmp(action, "nvmlDeviceGetTemperatureThreshold")) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1g271ba78911494f33fc079b204a929405
        // Note: This API is no longer the preferred interface for retrieving the following temperature thresholds on Ada and later architectures: NVML_TEMPERATURE_THRESHOLD_SHUTDOWN, NVML_TEMPERATURE_THRESHOLD_SLOWDOWN, NVML_TEMPERATURE_THRESHOLD_MEM_MAX and NVML_TEMPERATURE_THRESHOLD_GPU_MAX.
        //  Support for reading these temperature thresholds for
        //  Ada and later architectures would be removed from this API in future releases.Please
        //  use nvmlDeviceGetFieldValues with NVML_FI_DEV_TEMPERATURE_* fields to retrieve temperature thresholds on these architectures
        // TODO impl
    } else if (strcmp(action, "nvmlDeviceGetThermalSettings")) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1gf0c51f78525ea6fbc1a83bd75db098c7
        // TODO impl
    } else {
        // TODO add default handler
    }
}

void get_thermal_settings_handler(const int client_fd, const networkRequest_st* request) {
    networkResponse_st response;
    const char* uuid = request->arguments[0];  // FIXME
    const unsigned int sensor_index = strtol(request->arguments[1], NULL, 10);  // FIXME

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result)) {
        RESPONSE(response, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID to any device");
        send_response(client_fd, &response);
        return;
    }
    if (FATAL(gl_nvml_result)) PANIC("Couldn't get device handle!");

    nvmlGpuThermalSettings_t gpu_thermal_settings;
    gl_nvml_result = nvmlDeviceGetThermalSettings(device, sensor_index, &gpu_thermal_settings);
    if (ERROR(gl_nvml_result)) {
        RESPONSE(response, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't match sensor to any thermal settings");
        send_response(client_fd, &response);
        return;
    }
    if (FATAL(gl_nvml_result)) PANIC("Couldn't get GPU thermal settings for uuid %s sensor %d!", uuid, sensor_index);

    // TODO construct response

}

// void read_request(const int client_fd, cJSON* request /* out */) {
    // TODO impl
// }

void send_response(const int client_fd, const networkResponse_st* response) {
    // TODO impl
}

// int main() {
//     // OUT
//     cJSON* root = cJSON_CreateObject();
//     cJSON_AddStringToObject(root, "message", "Hello, World!");
//
//     char *out = cJSON_Print(root);
//     free(out);
//     cJSON_Delete(root);
//
//     // IN
//     char buffer[1024];
//     // write to buffer ...
//     cJSON* read_root = cJSON_Parse(buffer);
//     const char *message = cJSON_GetStringValue(cJSON_GetObjectItem(read_root, "message"));
//     printf("Read message: %s\n", message);
//     cJSON_Delete(read_root);
// }

