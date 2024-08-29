#include "network.h"
#include "helpers.h"
#include <errno.h>

extern nvmlReturn_t gl_nvml_result; // global; use for panics
extern char *so_buffer; // global; use for socket io

ssize_t sso_read(const int socket_fd, char *buffer /*out*/, const size_t size);
void assign_task(const int client_fd, const char *action, const json_object *jobj);

// handlers
void nvmlDeviceGetMemoryInfo_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetDetailsAll_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetThermalSettings_handler(const int client_fd, const json_object *jobj);

int bind_socket_with_address(const char *address) {
    assert(address != NULL); // sanity

    const int fd_server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd_server_socket < 0)
        WTF("Could not create socket!");
    struct sockaddr_un sock_addr;
    sock_addr.sun_family = AF_UNIX;
    strcpy(sock_addr.sun_path, address); // strcpy blows, memcpy should be used here
    if (access(address, F_OK) == 0) unlink(address);
    // remove the file if it exists from a zombie process or whatever the fuck
    if (bind(fd_server_socket, (struct sockaddr *) &sock_addr, sizeof(struct sockaddr_un)) == -1)
        WTF("Could not bind socket!");
    return fd_server_socket;
}


void process(const int client_fd, const struct timeval *tv_timeout) {
    assert(client_fd >= 0); // sanity
    assert(tv_timeout != NULL); // sanity

    // the lifecycle of this object is that this will live until the program closes, whereupon everything
    //  will be freed by the OS (hopefully, lol)
    if (so_buffer == NULL) so_buffer = malloc(SO_INPUT_BUFFER_SIZE);
    MEMSET_BUFF(so_buffer, SO_INPUT_BUFFER_SIZE);

    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, tv_timeout, sizeof(struct timeval)) < 0) {
        PRINTLN_SO("Failed to set socket timeout! Returning early...");
        return;
    }

    const ssize_t bytes_received = sso_read(client_fd, so_buffer, SO_INPUT_BUFFER_SIZE);
    if (bytes_received < 0) {
        PRINTLN_SO("Failed to read from socket! Returning early...");
        return;
    }

    PRINTLN("Received %zd bytes: %s\n", bytes_received, so_buffer);
    enum json_tokener_error error;
    json_object *jobj = json_tokener_parse_verbose(so_buffer, &error);
    if (jobj == NULL) {
        PRINTLN_SO("Failed to parse JSON object w/ json-c w/ err %d ! Writing to client_fd out, and returning early...",error);
        RESPOND(client_fd, NULL, JSON_PARSING_FAILED, "Failed parsing of JSON, view daemon logs for error code...");
        return;
    }

    json_object *action_field = json_object_object_get(jobj, "action");
    if (action_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'action' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA,
                 "Invalid JSON schema: 'action' field does not exist in $ (root) jobj");
        if (json_object_put(jobj) != 0)
            WTF("Couldn't free jobj!");
        return;
    }

    const char *action = json_object_get_string(action_field);
    if (action == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'action' field does have a valid value");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'action' field does have a valid value");
        if (json_object_put(jobj) != 0)
            WTF("Couldn't free jobj!");
        return;
    }

    assign_task(client_fd, action, jobj);
    json_object_put(jobj);
}

void assign_task(const int client_fd, const char *action, const json_object *jobj) {
    assert(jobj != NULL); // sanity
    PRINTLN_SO("Got action '%s', length %lu", action, strlen(action));
    if (strcmp(action, "nvmlDeviceGetTemperature") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1g92d1c5182a14dd4be7090e3c1480b121
        // TODO impl
        PRINTLN_SO("nvmlDeviceGetTemperature");
    } else if (strcmp(action, "nvmlDeviceGetTemperatureThreshold") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1g271ba78911494f33fc079b204a929405
        // Note: This API is no longer the preferred interface for retrieving the following temperature thresholds on Ada and later architectures: NVML_TEMPERATURE_THRESHOLD_SHUTDOWN, NVML_TEMPERATURE_THRESHOLD_SLOWDOWN, NVML_TEMPERATURE_THRESHOLD_MEM_MAX and NVML_TEMPERATURE_THRESHOLD_GPU_MAX.
        //  Support for reading these temperature thresholds for
        //  Ada and later architectures would be removed from this API in future releases.Please
        //  use nvmlDeviceGetFieldValues with NVML_FI_DEV_TEMPERATURE_* fields to retrieve temperature thresholds on these architectures
        PRINTLN_SO("nvmlDeviceGetTemperatureThreshold_handler");
        // TODO impl
    } else if (strcmp(action, "nvmlDeviceGetThermalSettings") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1gf0c51f78525ea6fbc1a83bd75db098c7
        PRINTLN_SO("nvmlDeviceGetThermalSettings_handler");
        nvmlDeviceGetThermalSettings_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetMemoryInfo_v2")){
        PRINTLN_SO("nvmlDeviceGetMemoryInfo_handler");
        nvmlDeviceGetMemoryInfo_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetDetailsAll") == 0) {
        // custom 'action'; this will get all the important details required
        PRINTLN_SO("nvmlDeviceGetDetailsAll_handler");
        nvmlDeviceGetDetailsAll_handler(client_fd, jobj);
    } else {
        PRINTLN_SO("Got erroneous action %s, couldn't resolve provided action to any valid action!", action);
        RESPOND(client_fd, NULL, UNDEFINED_INVALID_ACTION, "Couldn't resolve provided action to any valid envyd or NVML action.");
    }
}

void nvmlDeviceGetMemoryInfo_handler(const int client_fd, const json_object *jobj) {
    // nvmlDeviceGetMemoryInfo_v2
    // TODO impl
}

void nvmlDeviceGetDetailsAll_handler(const int client_fd, const json_object *jobj) {
    // half a meg will literally handle even clusters, I'd hope lol (should fit about 400 devices, counting overhead)
    char* buffer = calloc(sizeof(char), 524288);
    unsigned int device_count;
    gl_nvml_result = nvmlDeviceGetCount_v2(&device_count);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't get count of devices!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't get count of devices");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Catastrophic failure when grabbing device count! Is NVML instance up?");

    sprintf(buffer, "{\"count\": %d, \"data\": [", device_count);
    int failure_count = 0;
    for (int i = 0; i < device_count; ++i) {
        nvmlDevice_t device;
        gl_nvml_result = nvmlDeviceGetHandleByIndex_v2(i, &device);
        if (ERROR(gl_nvml_result)) {
            PRINTLN_SO("Couldn't get device by index %d!", i);
            ++failure_count;
            continue;
        }
        if (FATAL(gl_nvml_result)) WTF("Catastrophic failure while getting device handle by index %d!", i);

        char uuid[128];
        gl_nvml_result = nvmlDeviceGetUUID(device, uuid, 256);
        if (ERROR(gl_nvml_result)) {
            PRINTLN_SO("Couldn't get uuid by index %d!", i);
            ++failure_count;
            continue;
        }
        if (FATAL(gl_nvml_result)) WTF("Catastrophic failure while getting uuid by index %d!", i);

        char name[128];
        gl_nvml_result = nvmlDeviceGetName(device, name, 256);
        if (ERROR(gl_nvml_result)) {
            PRINTLN_SO("Couldn't get device name by index %d!", i);
            ++failure_count;
            continue;
        }
        if (FATAL(gl_nvml_result)) WTF("Catastrophic failure while getting device name by index %d!", i);

        char gsp_version[64];
        gl_nvml_result = nvmlDeviceGetGspFirmwareVersion(device, gsp_version);
        if (ERROR(gl_nvml_result)) {
            PRINTLN_SO("Couldn't get gsp version by index %d!", i);
            ++failure_count;
            continue;
        }
        if (FATAL(gl_nvml_result)) WTF("Catastrophic failure while getting gsp version by index %d!", i);

        unsigned int gsp_mode;
        unsigned int default_mode;
        gl_nvml_result = nvmlDeviceGetGspFirmwareMode(device, &gsp_mode, &default_mode);
        if (ERROR(gl_nvml_result)) {
            PRINTLN_SO("Couldn't get gsp mode by index %d!", i);
            ++failure_count;
            continue;
        }
        if (FATAL(gl_nvml_result)) WTF("Catastrophic failure while getting gsp mode by index %d!", i);

        char device_buff[512];
        sprintf(
            device_buff,
            ",{"
                "\"uuid\":" "\"%s\","
                "\"name\":" "\"%s\","
                "\"gsp_version\":" "\"%s\","
                "\"gsp_mode\":" "%d,"
                "\"gsp_default-mode\":" "%d"
            "}",
            uuid,
            name,
            gsp_version,
            gsp_mode,
            default_mode);

        strcat(buffer, device_buff);
    }
    // nvmlDeviceGetCount_v2
    // nvmlDeviceGetHandleByIndex_v2
    // nvmlDeviceGetUUID
    // nvmlDeviceGetGspFirmwareVersion
    // nvmlDeviceGetGspFirmwareMode
    const size_t len = strlen(buffer);  // close the array
    buffer[len] = "]}";

    const int status = failure_count > 0 ? NVML_ERROR_UNKNOWN : NVML_SUCCESS;
    const char* desc = failure_count > 0 ? "Failed to get details for some GPUs." : "Successfully successfully generated device list.";
    RESPOND(client_fd, buffer, map_nvmlReturn_t_to_string(status), desc);
    free(buffer);
}

void nvmlDeviceGetThermalSettings_handler(const int client_fd, const json_object *jobj) {
    json_object *uuid_field = json_object_object_get(jobj, "uuid");
    if (uuid_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'uuid' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'uuid' field does not exist in $ (root) jobj");
        return;
    }

    const char *uuid = json_object_get_string(uuid_field);
    if (uuid == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'uuid' field does have a valid value");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'uuid' field does have a valid value");
        return;
    }

    const json_object *sensorIndex_field = json_object_object_get(jobj, "sensorIndex");
    if (sensorIndex_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'sensorIndex' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'sensorIndex' field does not exist in $ (root) jobj");
        return;
    }

    const uint32_t sensor_index = json_object_get_int(sensorIndex_field);
    if (errno == EINVAL && sensor_index == 0) {
        PRINTLN_SO("Invalid JSON schema: 'sensorIndex' field does have a valid value");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'sensorIndex' field does have a valid value");
        return;
    }

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    nvmlGpuThermalSettings_t gpu_thermal_settings;
    gl_nvml_result = nvmlDeviceGetThermalSettings(device, sensor_index, &gpu_thermal_settings);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_UNKNOWN) {
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't match sensor");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get GPU thermal settings for uuid %s sensor %d!", uuid, sensor_index);

    char buff[2048];
    snprintf(
        buff, 2048,
        "{"
            "\"count\": %d, "
            "\"sensors\": ["
                "{"
                    "\"controller\": \"%s\","
                    "\"target\": \"%s\","
                    "\"currentTemp\": %d,"
                    "\"defaultMaxTemp\": %d,"
                    "\"defaultMinTemp\": %d"
                "},"
                "{"
                    "\"controller\": \"%s\","
                    "\"target\": \"%s\","
                    "\"currentTemp\": %d,"
                    "\"defaultMaxTemp\": %d,"
                    "\"defaultMinTemp\": %d"
                "},"
                "{"
                    "\"controller\": \"%s\","
                    "\"target\": \"%s\","
                    "\"currentTemp\": %d,"
                    "\"defaultMaxTemp\": %d,"
                    "\"defaultMinTemp\": %d"
                "}"
            "]"
        "}",
        gpu_thermal_settings.count,
        map_nvmlThermalController_t_to_string(gpu_thermal_settings.sensor[0].controller),
        map_nvmlThermalTarget_t_to_string(gpu_thermal_settings.sensor[0].target),
        gpu_thermal_settings.sensor[0].currentTemp,
        gpu_thermal_settings.sensor[0].defaultMaxTemp,
        gpu_thermal_settings.sensor[0].defaultMinTemp,
        map_nvmlThermalController_t_to_string(gpu_thermal_settings.sensor[1].controller),
        map_nvmlThermalTarget_t_to_string(gpu_thermal_settings.sensor[1].target),
        gpu_thermal_settings.sensor[1].currentTemp,
        gpu_thermal_settings.sensor[1].defaultMaxTemp,
        gpu_thermal_settings.sensor[1].defaultMinTemp,
        map_nvmlThermalController_t_to_string(gpu_thermal_settings.sensor[2].controller),
        map_nvmlThermalTarget_t_to_string(gpu_thermal_settings.sensor[2].target),
        gpu_thermal_settings.sensor[2].currentTemp,
        gpu_thermal_settings.sensor[2].defaultMaxTemp,
        gpu_thermal_settings.sensor[2].defaultMinTemp)
    ;

    RESPOND(client_fd, buff, map_nvmlReturn_t_to_string(gl_nvml_result), "Successfully retrieved data!");
}

// ----------------------------- NETWORK STUFF -----------------------------

/**
 * S-afe SO-cket read
 * @param socket_fd client_fd
 * @param buffer output
 * @param size   size of buffer
 * @return
 */
ssize_t sso_read(const int socket_fd, char *buffer /*out*/, const size_t size) {
    assert(buffer != NULL); // sanity
    ssize_t total_bytes = 0;

    PRINTLN_SO("Will read from fd (%d) into buffer of size %llu", socket_fd, size);
    while (total_bytes < size) {
        const ssize_t bytes_received = read(socket_fd, buffer + total_bytes, size - total_bytes);

        if (bytes_received == 0) {
            PRINTLN_SO("No more data to receive (received <= 0 len bytes), read total bytes %llu...", total_bytes);
            break;
        }

        if (bytes_received == -1) {
            PRINTLN_SO("Error reading from socket; memsetting buffer to 0 (deleting data), and returning early w/ -1!");
            MEMSET_BUFF(buffer, size);
            total_bytes = -1;
            break;
        }

        total_bytes += bytes_received;
    }

    return total_bytes;
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
