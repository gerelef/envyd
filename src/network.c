#include "network.h"
#include "helpers.h"
#include <nvdialog.h>
#include <errno.h>
#include <limits.h>

extern nvmlReturn_t gl_nvml_result; // global; use for panics
extern char *so_buffer;             // global; use for socket io

ssize_t sso_read(const int socket_fd, char *buffer /*out*/, const size_t size);
void assign_task(const int client_fd, const char *action, const json_object *jobj);

// handlers
// clocks
void nvmlDeviceGetAdaptiveClockInfoStatus_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetClock_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetClockInfo_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetClockOffsets_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetMaxClockInfo_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetSupportedGraphicsClocks_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetSupportedMemoryClocks_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceSetClockOffsets_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceSetMemoryLockedClocks_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceSetApplicationsClocks_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceSetGpuLockedClocks_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceResetApplicationsClocks_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceResetGpuLockedClocks_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceResetMemoryLockedClocks_handler(const int client_fd, const json_object *jobj);
// power
void nvmlDeviceGetPowerManagementDefaultLimit_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetPowerManagementLimit_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetPowerManagementLimitConstraints_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetPowerUsage_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceSetPowerManagementLimit_handler(const int client_fd, const json_object *jobj);
// fans
void nvmlDeviceGetNumFans_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetFanSpeed_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetMinMaxFanSpeed_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetTargetFanSpeed_handler(const int client_fd, const json_object *jobj);
// restrictions
void nvmlDeviceSetAPIRestriction_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetAPIRestriction_handler(const int client_fd, const json_object *jobj);
// thermals
void nvmlDeviceGetTemperature_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetTemperatureThreshold_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetThermalSettings_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceSetTemperatureThreshold_handler(const int client_fd, const json_object *jobj);
// generic
void nvmlDeviceGetMemoryInfo_handler(const int client_fd, const json_object *jobj);
void nvmlDeviceGetDetailsAll_handler(const int client_fd, const json_object *jobj);

/**
 * @return 0 on authorized, != 0 on non-authorized
 */
int is_authorized(const char* token) {
    WTF("Not implemented!");  // TODO implement
}

/**
 * @return NVD_REPLY_CANCEL in case of errors, NVD_REPLY_YES or NVD_REPLY_NO for user choices.
 */
NvdReply authorize(const char* api, const char* token) {
    const size_t buff_size = 8192;
    char* buff = calloc(sizeof(char), buff_size);
    snprintf(buff, buff_size, "Client is asking for access to %s.\n Authorize with token %s?", api, token);
    const NvdQuestionButton question_button = NVD_YES_NO;
    NvdQuestionBox* dialog = nvd_dialog_question_new(
        "Authorization",
        buff,
        question_button
    );
    if (!dialog) return NVD_REPLY_CANCEL;

    const NvdReply reply = nvd_get_reply(dialog);
    nvd_free_object(dialog);
    free(buff);
    // cascade options to YES, NO; CANCEL is reserved for error handling
    return reply == NVD_REPLY_OK ? NVD_REPLY_OK : NVD_REPLY_NO;
}

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

    const ssize_t bytes_received = sso_read(client_fd, so_buffer, SO_INPUT_BUFFER_SIZE - 1);
    if (bytes_received < 0) {
        PRINTLN_SO("Failed to read from socket! Returning early...");
        return;
    }

    PRINTLN_SO("Received %zd bytes: %s\n", bytes_received, so_buffer);
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
        if (json_object_put(jobj) != 0) WTF("Couldn't free jobj!");
        return;
    }

    const char *action = json_object_get_string(action_field);
    if (action == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'action' field does have a valid value");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'action' field does have a valid value");
        if (json_object_put(jobj) != 0) WTF("Couldn't free jobj!");
        return;
    }

    assign_task(client_fd, action, jobj);
    json_object_put(jobj);
}

void assign_task(const int client_fd, const char *action, const json_object *jobj) {
    assert(jobj != NULL); // sanity
    PRINTLN_SO("Got action '%s', length %lu", action, strlen(action));
    if (strcmp(action, "nvmlDeviceGetAdaptiveClockInfoStatus") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1gf615cda86fd569ce30a25d441b0f5c3a
        PRINTLN_SO("nvmlDeviceGetAdaptiveClockInfoStatus_handler");
        nvmlDeviceGetAdaptiveClockInfoStatus_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetClock") == 0) {
        PRINTLN_SO("nvmlDeviceGetClock_handler");
        nvmlDeviceGetClock_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetClockInfo") == 0) {
        PRINTLN_SO("nvmlDeviceGetClockInfo_handler");
        nvmlDeviceGetClockInfo_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetClockOffsets") == 0) {
        PRINTLN_SO("nvmlDeviceGetClockOffsets");
        nvmlDeviceGetClockOffsets_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetMaxClockInfo") == 0) {
        PRINTLN_SO("nvmlDeviceGetMaxClockInfo_handler");
        nvmlDeviceGetMaxClockInfo_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetSupportedGraphicsClocks") == 0) {
        PRINTLN_SO("nvmlDeviceGetSupportedGraphicsClocks_handler");
        nvmlDeviceGetSupportedGraphicsClocks_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetSupportedMemoryClocks") == 0) {
        PRINTLN_SO("nvmlDeviceGetSupportedMemoryClocks_handler");
        nvmlDeviceGetSupportedMemoryClocks_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceSetClockOffsets") == 0) {
        PRINTLN_SO("nvmlDeviceSetClockOffsets_handler");
        CHECK_AUTHORIZATION("setting clock offsets", jobj);
        nvmlDeviceSetClockOffsets_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceSetMemoryLockedClocks") == 0) {
        PRINTLN_SO("nvmlDeviceSetMemoryLockedClocks_handler");
        CHECK_AUTHORIZATION("setting memory locked clocks", jobj);
        nvmlDeviceSetMemoryLockedClocks_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceSetApplicationsClocks") == 0) {
        PRINTLN_SO("nvmlDeviceSetApplicationsClocks_handler");
        CHECK_AUTHORIZATION("setting application locked clocks", jobj);
        nvmlDeviceSetApplicationsClocks_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceSetGpuLockedClocks") == 0) {
        PRINTLN_SO("nvmlDeviceSetMemoryLockedClocks_handler");
        CHECK_AUTHORIZATION("setting gpu locked clocks", jobj);
        nvmlDeviceSetGpuLockedClocks_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceResetApplicationsClocks") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceCommands.html#group__nvmlDeviceCommands_1gbe6c0458851b3db68fa9d1717b32acd1
        PRINTLN_SO("nvmlDeviceResetApplicationsClocks_handler");
        CHECK_AUTHORIZATION("resetting application clocks", jobj);
        nvmlDeviceResetApplicationsClocks_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceResetGpuLockedClocks") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceCommands.html#group__nvmlDeviceCommands_1g51a3ca282a33471fe50c19751a99ead2
        PRINTLN_SO("nvmlDeviceResetGpuLockedClocks_handler");
        CHECK_AUTHORIZATION("resetting gpu locked clocks", jobj);
        nvmlDeviceResetGpuLockedClocks_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceResetMemoryLockedClocks") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceCommands.html#group__nvmlDeviceCommands_1gc131dbdbebe753f63b254e0ec76f7154
        PRINTLN_SO("nvmlDeviceResetMemoryLockedClocks_handler");
        CHECK_AUTHORIZATION("resetting memory locked clocks", jobj);
        nvmlDeviceResetMemoryLockedClocks_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetPowerManagementDefaultLimit") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1gd3ffb56cd39d079013dbfaba941eb31b
        PRINTLN_SO("nvmlDeviceGetPowerManagementDefaultLimit_handler");
        nvmlDeviceGetPowerManagementDefaultLimit_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetPowerManagementLimit") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1gf754f109beca3a4a8c8c1cd650d7d66c
        PRINTLN_SO("nvmlDeviceGetPowerManagementLimit_handler");
        nvmlDeviceGetPowerManagementLimit_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetPowerManagementLimitConstraints") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1g350d841176116e366284df0e5e2fe2bf
        PRINTLN_SO("nvmlDeviceGetPowerManagementLimitConstraints_handler");
        nvmlDeviceGetPowerManagementLimitConstraints_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetPowerUsage") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1g7ef7dff0ff14238d08a19ad7fb23fc87
        PRINTLN_SO("nvmlDeviceGetPowerUsage_handler");
        nvmlDeviceGetPowerUsage_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceSetPowerManagementLimit") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1gd10040f340986af6cda91e71629edb2b
        PRINTLN_SO("nvmlDeviceSetPowerManagementLimit_handler");
        CHECK_AUTHORIZATION("setting power management limit", jobj);
        nvmlDeviceSetPowerManagementLimit_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetNumFans") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1g49dfc28b9d0c68f487f9321becbcad3e
        PRINTLN_SO("nvmlDeviceGetNumFans_handler");
        nvmlDeviceGetNumFans_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetFanSpeed") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1g49dfc28b9d0c68f487f9321becbcad3e
        PRINTLN_SO("nvmlDeviceGetFanSpeed_handler");
        nvmlDeviceGetFanSpeed_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetMinMaxFanSpeed") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1g6922296589fef4898133a1ec30ec7cf5
        PRINTLN_SO("nvmlDeviceGetMinMaxFanSpeed_handler");
        nvmlDeviceGetMinMaxFanSpeed_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetTargetFanSpeed") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1g821108f7d34dc47811a2a29ad76f7969
        PRINTLN_SO("nvmlDeviceGetTargetFanSpeed_handler");
        nvmlDeviceGetTargetFanSpeed_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceSetAPIRestriction") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1g49dfc28b9d0c68f487f9321becbcad3e
        PRINTLN_SO("nvmlDeviceSetAPIRestriction_handler");
        CHECK_AUTHORIZATION("setting api restrictions", jobj);
        nvmlDeviceSetAPIRestriction_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetAPIRestriction") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1g49dfc28b9d0c68f487f9321becbcad3e
        PRINTLN_SO("nvmlDeviceGetAPIRestriction_handler");
        nvmlDeviceGetAPIRestriction_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetTemperature") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1g92d1c5182a14dd4be7090e3c1480b121
        PRINTLN_SO("nvmlDeviceGetTemperature_handler");
        nvmlDeviceGetTemperature_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetTemperatureThreshold") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1g271ba78911494f33fc079b204a929405
        // Note: This API is no longer the preferred interface for retrieving the following temperature thresholds on Ada and later architectures: NVML_TEMPERATURE_THRESHOLD_SHUTDOWN, NVML_TEMPERATURE_THRESHOLD_SLOWDOWN, NVML_TEMPERATURE_THRESHOLD_MEM_MAX and NVML_TEMPERATURE_THRESHOLD_GPU_MAX.
        //  Support for reading these temperature thresholds for
        //  Ada and later architectures would be removed from this API in future releases.Please
        //  use nvmlDeviceGetFieldValues with NVML_FI_DEV_TEMPERATURE_* fields to retrieve temperature thresholds on these architectures
        PRINTLN_SO("nvmlDeviceGetTemperatureThreshold_handler");
        nvmlDeviceGetTemperatureThreshold_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetThermalSettings") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1gf0c51f78525ea6fbc1a83bd75db098c7
        PRINTLN_SO("nvmlDeviceGetThermalSettings_handler");
        nvmlDeviceGetThermalSettings_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceSetTemperatureThreshold") == 0) {
        // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceCommands.html#group__nvmlDeviceCommands_1g0258912fc175951b8efe1440ca59e200
        PRINTLN_SO("nvmlDeviceSetTemperatureThreshold_handler");
        // FIXME this needs to be checked; I couldn't set it even with sudo rights (failed w/ INVALID_ARGUMENT)
        CHECK_AUTHORIZATION("setting temperature threshold", jobj);
        nvmlDeviceSetTemperatureThreshold_handler(client_fd, jobj);
    } else if (strcmp(action, "nvmlDeviceGetMemoryInfo") == 0){
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

// ----------------------------- CLOCKS -----------------------------

void nvmlDeviceGetAdaptiveClockInfoStatus_handler(const int client_fd, const json_object *jobj) {
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

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    unsigned int status;
    gl_nvml_result = nvmlDeviceGetAdaptiveClockInfoStatus(device, &status);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve get adaptive clock info status for device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't reset adaptive clock info status for device!");
        return;
    }

    char* statusb = status == NVML_ADAPTIVE_CLOCKING_INFO_STATUS_ENABLED ? "true" : "false";
    char buffer[64];
    sprintf(buffer, "{ \"adaptiveClockStatus\": %s }", statusb);
    RESPOND(client_fd, buffer, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
}

void nvmlDeviceGetClock_handler(const int client_fd, const json_object *jobj) {
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

    json_object *clock_type_field = json_object_object_get(jobj, "clockType");
    if (clock_type_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'clockType' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'clockType' field does not exist in $ (root) jobj");
        return;
    }

    const char *clock_type_s = json_object_get_string(clock_type_field);
    if (clock_type_s == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'clockType' field does have a valid value");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'clockType' field does have a valid value");
        return;
    }

    const nvmlClockType_t clock_type = map_nvmlClockType_t_to_enum(clock_type_s);
    if (clock_type == NVML_CLOCK_COUNT) {
        PRINTLN_SO("Invalid JSON schema: 'clockType' field did not evaluate to anything within the nvmlClockType_t (value %s must be a string of the enum value)", clock_type_s);
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'clockType' field did not evaluate to anything within the nvmlClockType_t (value must be a string of the enum value)");
        return;
    }

    json_object *clock_id_field = json_object_object_get(jobj, "clockId");
    if (clock_id_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'clockId' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'clockId' field does not exist in $ (root) jobj");
        return;
    }

    const char *clock_id_s = json_object_get_string(clock_id_field);
    if (clock_id_s == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'clockId' field does have a valid value");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'clockId' field does have a valid value");
        return;
    }

    const nvmlClockId_t clock_id = map_nvmlClockId_t_to_enum(clock_id_s);
    if (clock_id == NVML_CLOCK_ID_COUNT) {
        PRINTLN_SO("Invalid JSON schema: 'clockId' field did not evaluate to anything within the nvmlClockId_t (value must be a string of the enum value)");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'clockId' field did not evaluate to anything within the nvmlClockId_t (value must be a string of the enum value)");
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

    unsigned int clock_mhz = 1;
    gl_nvml_result = nvmlDeviceGetClock(device, clock_type, clock_id, &clock_mhz);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't get clock for device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't get clock for device!");
        return;
    }

    char buff[64];
    sprintf(
        buff,
        "{ \"clockMHz\": %u }",
        clock_mhz
    );
    RESPOND(client_fd, buff, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
}

void nvmlDeviceGetClockInfo_handler(const int client_fd, const json_object *jobj) {
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

    json_object *clock_type_field = json_object_object_get(jobj, "clockType");
    if (clock_type_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'clockType' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'clockType' field does not exist in $ (root) jobj");
        return;
    }

    const char *clock_type_s = json_object_get_string(clock_type_field);
    if (clock_type_s == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'clockType' field does have a valid value");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'clockType' field does have a valid value");
        return;
    }

    const nvmlClockType_t clock_type = map_nvmlClockType_t_to_enum(clock_type_s);
    if (clock_type == NVML_CLOCK_COUNT) {
        PRINTLN_SO("Invalid JSON schema: 'clockType' field did not evaluate to anything within the nvmlClockType_t (value %s must be a string of the enum value)", clock_type_s);
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'clockType' field did not evaluate to anything within the nvmlClockType_t (value must be a string of the enum value)");
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

    unsigned int clock = 1;
    gl_nvml_result = nvmlDeviceGetClockInfo(device, clock_type, &clock);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't get clock for device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't get clock for device!");
        return;
    }

    char buff[64];
    sprintf(
        buff,
        "{ \"clock\": %u }",
        clock
    );
    RESPOND(client_fd, buff, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
}

void nvmlDeviceGetClockOffsets_handler(const int client_fd, const json_object *jobj) {
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

    json_object *clock_type_field = json_object_object_get(jobj, "clockType");
    if (clock_type_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'clockType' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'clockType' field does not exist in $ (root) jobj");
        return;
    }

    const char *clock_type_s = json_object_get_string(clock_type_field);
    if (clock_type_s == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'clockType' field does have a valid value");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'clockType' field does have a valid value");
        return;
    }

    const nvmlClockType_t clock_type = map_nvmlClockType_t_to_enum(clock_type_s);
    if (clock_type == NVML_CLOCK_COUNT) {
        PRINTLN_SO("Invalid JSON schema: 'clockType' field did not evaluate to anything within the nvmlClockType_t (value %s must be a string of the enum value)", clock_type_s);
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'clockType' field did not evaluate to anything within the nvmlClockType_t (value must be a string of the enum value)");
        return;
    }

    json_object *pstate_type_field = json_object_object_get(jobj, "pstate");
    if (pstate_type_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'pstate' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'pstate' field does not exist in $ (root) jobj");
        return;
    }

    const char *pstate_s = json_object_get_string(pstate_type_field);
    if (pstate_s == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'pstate' field does have a valid value");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'pstate' field does have a valid value");
        return;
    }

    const nvmlPstates_t pstate = map_nvmlClockType_t_to_enum(pstate_s);
    if (pstate == NVML_PSTATE_UNKNOWN) {
        PRINTLN_SO("Invalid JSON schema: 'pstate' field did not evaluate to anything within the nvmlPstates_t (value %s must be a string of the enum value)", pstate_s);
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'pstate' field did not evaluate to anything within the nvmlClockType_t (value must be a string of the enum value)");
        return;
    }

    // clockType
    // pstate
    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    nvmlClockOffset_t info = {0};
    info.version = NVML_STRUCT_VERSION(ClockOffset, 1);
    info.type = clock_type;
    info.pstate = pstate;
    gl_nvml_result = nvmlDeviceGetClockOffsets(device, &info);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve get offsets for device %s", uuid);
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve get offsets for device!");
        return;
    }

    char buff[256];
    sprintf(
        buff,
        "{\"clockOffsetMHz\": %d, \"minClockOffsetMHz\": %d, \"maxClockOffsetMHz\": %d}",
        info.clockOffsetMHz,
        info.minClockOffsetMHz,
        info.maxClockOffsetMHz
    );
    RESPOND(client_fd, buff, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
}

void nvmlDeviceGetMaxClockInfo_handler(const int client_fd, const json_object *jobj) {
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

    json_object *clock_type_field = json_object_object_get(jobj, "clockType");
    if (clock_type_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'clockType' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'clockType' field does not exist in $ (root) jobj");
        return;
    }

    const char *clock_type_s = json_object_get_string(clock_type_field);
    if (clock_type_s == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'clockType' field does have a valid value");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'clockType' field does have a valid value");
        return;
    }

    const nvmlClockType_t clock_type = map_nvmlClockType_t_to_enum(clock_type_s);
    if (clock_type == NVML_CLOCK_COUNT) {
        PRINTLN_SO("Invalid JSON schema: 'clockType' field did not evaluate to anything within the nvmlClockType_t (value %s must be a string of the enum value)", clock_type_s);
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'clockType' field did not evaluate to anything within the nvmlClockType_t (value must be a string of the enum value)");
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

    unsigned int clock = 1;
    gl_nvml_result = nvmlDeviceGetMaxClockInfo(device, clock_type, &clock);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't get max clock for device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't get max clock for device!");
        return;
    }

    char buff[64];
    sprintf(
        buff,
        "{ \"clock\": %u }",
        clock
    );
    RESPOND(client_fd, buff, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
}

void nvmlDeviceGetSupportedGraphicsClocks_handler(const int client_fd, const json_object *jobj) {
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

    const json_object *memory_clock_field = json_object_object_get(jobj, "memoryClockMHZ");
    if (memory_clock_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'memoryClockMHZ' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'memoryClockMHZ' field does not exist in $ (root) jobj");
        return;
    }

    if (!json_object_is_type(memory_clock_field, json_type_int)) {
        PRINTLN_SO("Invalid JSON schema: 'memoryClockMHZ' field is not an int");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'memoryClockMHZ' field is not an int");
        return;
    }

    const int memoryClockMHZ = json_object_get_int(memory_clock_field);
    if (memoryClockMHZ < 0) {
        PRINTLN_SO("Invalid JSON schema: 'memoryClockMHZ' field is not >= 0");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'memoryClockMHZ' field is not >= 0");
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

    unsigned int count = 1024;
    unsigned int* clocksMHZ = calloc(sizeof(unsigned int), count);
    gl_nvml_result = nvmlDeviceGetSupportedGraphicsClocks(device, memoryClockMHZ, &count, clocksMHZ);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        free(clocksMHZ);
        PRINTLN_SO("Couldn't resolve get supported graphics clocks for device w/ count %u!", count);
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve get supported graphics clocks for device!");
        return;
    }

    char* buff = calloc(sizeof(char), count*16);
    sprintf(buff, "[");
    for (int i = 0; i < count; ++i) {
        char supported_clock[72];
        sprintf(supported_clock, "%s%d", i > 0 ? "," : "", clocksMHZ[i]);
        strcat(buff, supported_clock);
    }
    buff[strlen(buff)] = ']';
    RESPOND(client_fd, buff, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
    free(clocksMHZ);
    free(buff);
}

void nvmlDeviceGetSupportedMemoryClocks_handler(const int client_fd, const json_object *jobj) {
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

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    unsigned int count = 1024;
    unsigned int* clocksMHZ = calloc(sizeof(unsigned int), count);
    gl_nvml_result = nvmlDeviceGetSupportedMemoryClocks(device, &count, clocksMHZ);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        free(clocksMHZ);
        PRINTLN_SO("Couldn't resolve get supported memory clocks for device w/ count %u!", count);
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve get supported memory clocks for device!");
        return;
    }

    char* buff = calloc(sizeof(char), 8192);
    sprintf(buff, "[");
    for (int i = 0; i < count; ++i) {
        char supported_clock[72];
        sprintf(supported_clock, "%s%d", i > 0 ? "," : "", clocksMHZ[i]);
        strcat(buff, supported_clock);
    }
    buff[strlen(buff)] = ']';
    RESPOND(client_fd, buff, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
    free(clocksMHZ);
    free(buff);
}

void nvmlDeviceSetClockOffsets_handler(const int client_fd, const json_object *jobj) {
    // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries_1g7a0bb4cb513396b7f42be81ac2ea4428
    // TODO impl
}

void nvmlDeviceSetMemoryLockedClocks_handler(const int client_fd, const json_object *jobj) {
    // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceCommands.html#group__nvmlDeviceCommands_1g3cab0aaf0e46aa76469f18707e5867f1
    // TODO impl
}

void nvmlDeviceSetApplicationsClocks_handler(const int client_fd, const json_object *jobj) {
    // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceCommands.html#group__nvmlDeviceCommands_1gc2a9a8db6fffb2604d27fd67e8d5d87f
    // TODO impl
}

void nvmlDeviceSetGpuLockedClocks_handler(const int client_fd, const json_object *jobj) {
    // https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceCommands.html#group__nvmlDeviceCommands_1gc9b58cd685f4deee575400e2e6ac76cb
    // TODO impl
}

void nvmlDeviceResetApplicationsClocks_handler(const int client_fd, const json_object *jobj) {
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

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    gl_nvml_result = nvmlDeviceResetApplicationsClocks(device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve reset applications clocks to device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't reset applications clocks to device!");
        return;
    }

    RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Successfully reset applications clocks!");
}

void nvmlDeviceResetGpuLockedClocks_handler(const int client_fd, const json_object *jobj) {
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

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    gl_nvml_result = nvmlDeviceResetGpuLockedClocks(device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve reset gpu clocks to device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't reset gpu clocks to device!");
        return;
    }

    RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Successfully reset gpu clocks!");
}

void nvmlDeviceResetMemoryLockedClocks_handler(const int client_fd, const json_object *jobj) {
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

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    gl_nvml_result = nvmlDeviceResetMemoryLockedClocks(device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve reset memory clocks to device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't reset memory clocks to device!");
        return;
    }

    RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Successfully reset memory clocks!");
}

// ----------------------------- POWER -----------------------------

void nvmlDeviceGetPowerManagementDefaultLimit_handler(const int client_fd, const json_object *jobj) {
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

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    unsigned int default_limit;
    gl_nvml_result = nvmlDeviceGetPowerManagementDefaultLimit(device, &default_limit);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't get default limit!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't get default limit!");
        return;
    }

    char buffer[64];
    sprintf(buffer, "{ \"defaultLimit\": %u }", default_limit);
    RESPOND(client_fd, buffer, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
}

void nvmlDeviceGetPowerManagementLimit_handler(const int client_fd, const json_object *jobj) {
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

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    unsigned int limit;
    gl_nvml_result = nvmlDeviceGetPowerManagementLimit(device, &limit);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't get limit!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't get limit!");
        return;
    }

    char buffer[64];
    sprintf(buffer, "{ \"limit\": %u }", limit);
    RESPOND(client_fd, buffer, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
}

void nvmlDeviceGetPowerManagementLimitConstraints_handler(const int client_fd, const json_object *jobj) {
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

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    unsigned int min_limit;
    unsigned int max_limit;
    gl_nvml_result = nvmlDeviceGetPowerManagementLimitConstraints(device, &min_limit, &max_limit);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't get fan min/max limit!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't get fan min/max limit");
        return;
    }

    char buffer[96];
    sprintf(buffer, "{ \"minLimit\": %u, \"maxLimit\": %u }", min_limit, max_limit);
    RESPOND(client_fd, buffer, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
}

void nvmlDeviceGetPowerUsage_handler(const int client_fd, const json_object *jobj) {
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

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    unsigned int power;
    gl_nvml_result = nvmlDeviceGetPowerUsage(device, &power);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't get power usage!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't get power usage!");
        return;
    }

    char buffer[64];
    sprintf(buffer, "{ \"power\": %u }", power);
    RESPOND(client_fd, buffer, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
}

void nvmlDeviceSetPowerManagementLimit_handler(const int client_fd, const json_object *jobj) {
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

    json_object *scope_field = json_object_object_get(jobj, "powerScope");
    if (scope_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'powerScope' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'powerScope' field does not exist in $ (root) jobj");
        return;
    }

    const char *scope = json_object_get_string(scope_field);
    if (scope == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'powerScope' field does have a valid value");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'powerScope' field does have a valid value");
        return;
    }

    const nvmlPowerScopeType_t scope_type = map_nvmlPowerScopeType_t_to_enum(scope);
    if (scope_type == CHAR_MAX) {
        PRINTLN_SO("Invalid JSON schema: 'powerScope' field did not evaluate to anything within the nvmlPowerScopeType_t (value must be NVML_POWER_SCOPE_GPU or NVML_POWER_SCOPE_MODULE or NVML_POWER_SCOPE_MEMORY)");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'powerScope' field did not evaluate to anything within the nvmlPowerScopeType_t (value must be NVML_POWER_SCOPE_GPU or NVML_POWER_SCOPE_MODULE or NVML_POWER_SCOPE_MEMORY)");
        return;
    }

    const json_object *power_value_field = json_object_object_get(jobj, "powerValueMw");
    if (power_value_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'powerValueMw' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'powerValueMw' field does not exist in $ (root) jobj");
        return;
    }

    if (!json_object_is_type(power_value_field, json_type_int)) {
        PRINTLN_SO("Invalid JSON schema: 'powerValueMw' field is not an int");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'powerValueMw' field is not an int");
        return;
    }

    const int power_value = json_object_get_int(power_value_field);
    if (power_value < 0) {
        PRINTLN_SO("Invalid JSON schema: 'powerValueMw' field is not >= 0");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'powerValueMw' field is not >= 0");
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

    nvmlPowerValue_v2_t power_value_s = {0};
    power_value_s.version = nvmlPowerValue_v2;
    // powerScope NVML_POWER_SCOPE_GPU or NVML_POWER_SCOPE_MODULE or NVML_POWER_SCOPE_MEMORY
    power_value_s.powerScope = scope_type;
    power_value_s.powerValueMw = power_value;
    gl_nvml_result = nvmlDeviceSetPowerManagementLimit_v2(device, &power_value_s);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't set power management limit w/ uuid %s and version %u, type %d, mw %u", uuid, power_value_s.version, power_value_s.powerScope, power_value_s.powerValueMw);
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't set power management limit");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't set power management limit w/ uuid %s and version %u, type %d, mw %u", uuid, power_value_s.version, power_value_s.powerScope, power_value_s.powerValueMw);

    RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
}

// ----------------------------- FANS -----------------------------

void nvmlDeviceGetNumFans_handler(const int client_fd, const json_object *jobj) {
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

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    unsigned int num_fans;
    gl_nvml_result = nvmlDeviceGetNumFans(device, &num_fans);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't get count of fans!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't get count of fans");
        return;
    }

    char buffer[64];
    sprintf(buffer, "{ \"count\": %u }", num_fans);
    RESPOND(client_fd, buffer, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
}

void nvmlDeviceGetFanSpeed_handler(const int client_fd, const json_object *jobj) {
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

    const json_object *fan_field = json_object_object_get(jobj, "fan");
    if (fan_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'fan' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'fan' field does not exist in $ (root) jobj");
        return;
    }

    if (!json_object_is_type(fan_field, json_type_int)) {
        PRINTLN_SO("Invalid JSON schema: 'fan' field is not an int");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'fan' field is not an int");
        return;
    }

    const int fan_index = json_object_get_int(fan_field);
    if (fan_index < 0) {
        PRINTLN_SO("Invalid JSON schema: 'fan' field is not >= 0");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'fan' field is not >= 0");
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

    unsigned int num_speed;
    gl_nvml_result = nvmlDeviceGetFanSpeed_v2(device, fan_index, &num_speed);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't get fan speed!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't get fan speed");
        return;
    }

    char buffer[64];
    sprintf(buffer, "{ \"speed\": %u }", num_speed);
    RESPOND(client_fd, buffer, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
}

void nvmlDeviceGetMinMaxFanSpeed_handler(const int client_fd, const json_object *jobj) {
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

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    unsigned int min_speed;
    unsigned int max_speed;
    gl_nvml_result = nvmlDeviceGetMinMaxFanSpeed(device, &min_speed, &max_speed);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't get fan min/max speed!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't get fan min/max speed");
        return;
    }

    char buffer[96];
    sprintf(buffer, "{ \"minSpeed\": %u, \"maxSpeed\": %u }", min_speed, max_speed);
    RESPOND(client_fd, buffer, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
}

void nvmlDeviceGetTargetFanSpeed_handler(const int client_fd, const json_object *jobj) {
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

    const json_object *fan_field = json_object_object_get(jobj, "fan");
    if (fan_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'fan' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'fan' field does not exist in $ (root) jobj");
        return;
    }

    if (!json_object_is_type(fan_field, json_type_int)) {
        PRINTLN_SO("Invalid JSON schema: 'fan' field is not an int");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'fan' field is not an int");
        return;
    }

    const int fan_index = json_object_get_int(fan_field);
    if (fan_index < 0) {
        PRINTLN_SO("Invalid JSON schema: 'fan' field is not >= 0");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'fan' field is not >= 0");
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

    unsigned int num_speed;
    gl_nvml_result = nvmlDeviceGetTargetFanSpeed(device, fan_index, &num_speed);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't get fan speed!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't get fan speed");
        return;
    }

    char buffer[64];
    sprintf(buffer, "{ \"speed\": %u }", num_speed);
    RESPOND(client_fd, buffer, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
}

// ----------------------------- RESTRICTIONS -----------------------------

void nvmlDeviceSetAPIRestriction_handler(const int client_fd, const json_object *jobj) {
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

    json_object *type_field = json_object_object_get(jobj, "apiType");
    if (type_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'apiType' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'apiType' field does not exist in $ (root) jobj");
        return;
    }

    const char *type = json_object_get_string(type_field);
    if (type == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'apiType' field does have a valid value");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'apiType' field does have a valid value");
        return;
    }

    const nvmlRestrictedAPI_t api_type = map_nvmlRestrictedAPI_t_to_enum(type);
    if (api_type == NVML_RESTRICTED_API_COUNT) {
        PRINTLN_SO("Invalid JSON schema: 'apiType' field did not evaluate to anything within the nvmlRestrictedAPI_t enum");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'apiType' field did not evaluate to anything within the nvmlRestrictedAPI_t enum");
        return;
    }

    const json_object *isRestricted_field = json_object_object_get(jobj, "isRestricted");
    if (isRestricted_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'isRestricted' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'isRestricted' field does not exist in $ (root) jobj");
        return;
    }

    if (!json_object_is_type(isRestricted_field, json_type_boolean)) {
        PRINTLN_SO("Invalid JSON schema: 'isRestricted' field is not a boolean");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'isRestricted' field is not a boolean");
        return;
    }

    const json_bool is_restricted = json_object_get_boolean(isRestricted_field);

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    const nvmlEnableState_t nvmlRestricted = is_restricted == 1;
    gl_nvml_result = nvmlDeviceSetAPIRestriction(device, api_type, nvmlRestricted);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't set API restriction!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't set API restriction");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't set API restriction w/ uuid %s and type %d", uuid, api_type);

    RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
}

void nvmlDeviceGetAPIRestriction_handler(const int client_fd, const json_object *jobj) {
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

    json_object *type_field = json_object_object_get(jobj, "apiType");
    if (type_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'apiType' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'apiType' field does not exist in $ (root) jobj");
        return;
    }

    const char *type = json_object_get_string(type_field);
    if (type == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'apiType' field does have a valid value");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'apiType' field does have a valid value");
        return;
    }

    const nvmlRestrictedAPI_t api_type = map_nvmlRestrictedAPI_t_to_enum(type);
    if (api_type == NVML_RESTRICTED_API_COUNT) {
        PRINTLN_SO("Invalid JSON schema: 'apiType' field did not evaluate to anything within the nvmlRestrictedAPI_t enum");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'apiType' field did not evaluate to anything within the nvmlRestrictedAPI_t enum");
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

    nvmlEnableState_t is_restricted;
    gl_nvml_result = nvmlDeviceGetAPIRestriction(device, api_type, &is_restricted);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't resolve API restriction!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve API restriction");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get API restriction w/ uuid %s and type %d", uuid, api_type);

    char buff[64];
    snprintf(buff, 64, "{ \"restricted\": %s}", is_restricted == NVML_FEATURE_ENABLED ? "true" : "false");
    RESPOND(client_fd, buff, map_nvmlReturn_t_to_string(gl_nvml_result), "Successfully retrieved API restrictions!");
}

// ----------------------------- THERMALS  -----------------------------

void nvmlDeviceGetTemperature_handler(const int client_fd, const json_object *jobj) {
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

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    unsigned int temperature;
    gl_nvml_result = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temperature);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_UNKNOWN) {
        PRINTLN_SO("Couldn't get temperature!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't get temperature");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get GPU temperature for uuid %s sensor %d!", uuid, NVML_TEMPERATURE_GPU);

    char buff[64];
    snprintf(buff, 64, "{ \"temperature\": %d}", temperature);
    RESPOND(client_fd, buff, map_nvmlReturn_t_to_string(gl_nvml_result), "Successfully retrieved temperature!");
}

void nvmlDeviceGetTemperatureThreshold_handler(const int client_fd, const json_object *jobj) {
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

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    unsigned int shutdown;
    nvmlReturn_t lo_nvml_result = gl_nvml_result;
    gl_nvml_result = nvmlDeviceGetTemperatureThreshold(device, NVML_TEMPERATURE_THRESHOLD_SHUTDOWN, &shutdown);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't resolve temperature threshold info to device!");
        gl_nvml_result == NVML_ERROR_NOT_SUPPORTED ? shutdown = UINT_MAX : 0; // noop
        lo_nvml_result = gl_nvml_result;
    }
    if (FATAL(gl_nvml_result)) WTF("Catastrophic failure when getting temperature threshold for uuid %s", uuid);

    unsigned int slowdown;
    gl_nvml_result = nvmlDeviceGetTemperatureThreshold(device, NVML_TEMPERATURE_THRESHOLD_SLOWDOWN, &slowdown);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't resolve temperature threshold info to device!");
        gl_nvml_result == NVML_ERROR_NOT_SUPPORTED ? slowdown = UINT_MAX : 0; // noop
        lo_nvml_result = gl_nvml_result;
    }
    if (FATAL(gl_nvml_result)) WTF("Catastrophic failure when getting temperature threshold for uuid %s", uuid);

    unsigned int mem_max;
    gl_nvml_result = nvmlDeviceGetTemperatureThreshold(device, NVML_TEMPERATURE_THRESHOLD_MEM_MAX, &mem_max);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't resolve temperature threshold info to device!");
        gl_nvml_result == NVML_ERROR_NOT_SUPPORTED ? mem_max = UINT_MAX : 0; // noop
        lo_nvml_result = gl_nvml_result;
    }
    if (FATAL(gl_nvml_result)) WTF("Catastrophic failure when getting temperature threshold for uuid %s", uuid);

    unsigned int gpu_max;
    gl_nvml_result = nvmlDeviceGetTemperatureThreshold(device, NVML_TEMPERATURE_THRESHOLD_GPU_MAX, &gpu_max);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't resolve temperature threshold info to device!");
        gl_nvml_result == NVML_ERROR_NOT_SUPPORTED ? gpu_max = UINT_MAX : 0; // noop
        lo_nvml_result = gl_nvml_result;
    }
    if (FATAL(gl_nvml_result)) WTF("Catastrophic failure when getting temperature threshold for uuid %s", uuid);

    unsigned int acoustic_min;
    gl_nvml_result = nvmlDeviceGetTemperatureThreshold(device, NVML_TEMPERATURE_THRESHOLD_ACOUSTIC_MIN, &acoustic_min);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't resolve temperature threshold info to device!");
        gl_nvml_result == NVML_ERROR_NOT_SUPPORTED ? acoustic_min = UINT_MAX : 0; // noop
        lo_nvml_result = gl_nvml_result;
    }
    if (FATAL(gl_nvml_result)) WTF("Catastrophic failure when getting temperature threshold for uuid %s", uuid);

    unsigned int acoustic_curr;
    gl_nvml_result = nvmlDeviceGetTemperatureThreshold(device, NVML_TEMPERATURE_THRESHOLD_ACOUSTIC_CURR, &acoustic_curr);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't resolve temperature threshold info to device!");
        gl_nvml_result == NVML_ERROR_NOT_SUPPORTED ? acoustic_curr = UINT_MAX : 0;
        lo_nvml_result = gl_nvml_result;
    }
    if (FATAL(gl_nvml_result)) WTF("Catastrophic failure when getting temperature threshold for uuid %s", uuid);

    unsigned int acoustic_max;
    gl_nvml_result = nvmlDeviceGetTemperatureThreshold(device, NVML_TEMPERATURE_THRESHOLD_ACOUSTIC_MAX, &acoustic_max);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't resolve temperature threshold info to device!");
        gl_nvml_result == NVML_ERROR_NOT_SUPPORTED ? acoustic_max = UINT_MAX : 0; // noop
        lo_nvml_result = gl_nvml_result;
    }
    if (FATAL(gl_nvml_result)) WTF("Catastrophic failure when getting temperature threshold for uuid %s", uuid);

    unsigned int gps_curr;
    gl_nvml_result = nvmlDeviceGetTemperatureThreshold(device, NVML_TEMPERATURE_THRESHOLD_GPS_CURR, &gps_curr);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't resolve temperature threshold info to device!");
        gl_nvml_result == NVML_ERROR_NOT_SUPPORTED ? gps_curr = UINT_MAX : 0; // noop
        lo_nvml_result = gl_nvml_result;
    }
    if (FATAL(gl_nvml_result)) WTF("Catastrophic failure when getting temperature threshold for uuid %s", uuid);

    char buffer[256];
    sprintf(
        buffer,
        "{"
            "\"shutdown\": %u,"
            "\"slowdown\": %u,"
            "\"mem_max\": %u,"
            "\"gpu_max\": %u,"
            "\"acoustic_min\": %u,"
            "\"acoustic_curr\": %u,"
            "\"acoustic_max\": %u,"
            "\"gps_curr\": %u"
        "}",
        shutdown,
        slowdown,
        mem_max,
        gpu_max,
        acoustic_min,
        acoustic_curr,
        acoustic_max,
        gps_curr
    );
    char *desc = lo_nvml_result == NVML_ERROR_NOT_SUPPORTED ? "Some values might be garbage (denoted by UINT_MAX)" : NULL;
    RESPOND(client_fd, buffer, map_nvmlReturn_t_to_string(lo_nvml_result), desc);
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

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    nvmlGpuThermalSettings_t gpu_thermal_settings = {0};
    gl_nvml_result = nvmlDeviceGetThermalSettings(device, NVML_TEMPERATURE_GPU, &gpu_thermal_settings);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_UNKNOWN) {
        PRINTLN_SO("Couldn't match sensor!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't match sensor");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get GPU thermal settings for uuid %s sensor %d!", uuid, NVML_TEMPERATURE_GPU);

    char buff[2048];
    snprintf(
        buff, 2048,
        "{"
            "\"count\": %d, "
            // this might expand in the future; the NVML_TEMPERATURE_COUNT is 3, but there is only 1 enum val here
            "\"sensors\": ["
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
        map_nvmlThermalController_t_to_string(gpu_thermal_settings.sensor[NVML_TEMPERATURE_GPU].controller),
        map_nvmlThermalTarget_t_to_string(gpu_thermal_settings.sensor[NVML_TEMPERATURE_GPU].target),
        gpu_thermal_settings.sensor[NVML_TEMPERATURE_GPU].currentTemp,
        gpu_thermal_settings.sensor[NVML_TEMPERATURE_GPU].defaultMaxTemp,
        gpu_thermal_settings.sensor[NVML_TEMPERATURE_GPU].defaultMinTemp)
    ;

    RESPOND(client_fd, buff, map_nvmlReturn_t_to_string(gl_nvml_result), "Successfully retrieved data!");
}

void nvmlDeviceSetTemperatureThreshold_handler(const int client_fd, const json_object *jobj) {
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

    json_object *threshold_type_field = json_object_object_get(jobj, "thresholdType");
    if (threshold_type_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'thresholdType' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'thresholdType' field does not exist in $ (root) jobj");
        return;
    }

    const char *threshold_type = json_object_get_string(threshold_type_field);
    if (threshold_type == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'thresholdType' field does have a valid value");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'thresholdType' field does have a valid value");
        return;
    }

    const nvmlTemperatureThresholds_t threshold_type_t = map_nvmlTemperatureThresholds_t_to_enum(threshold_type);
    if (threshold_type_t == NVML_TEMPERATURE_THRESHOLD_COUNT) {
        PRINTLN_SO("Invalid JSON schema: 'thresholdType' field does have a valid value (is not in nvmlTemperatureThresholds_t enum)");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'thresholdType' field does have a valid value (is not in nvmlTemperatureThresholds_t enum)");
        return;
    }

    const json_object *temp_field = json_object_object_get(jobj, "temp");
    if (temp_field == NULL) {
        PRINTLN_SO("Invalid JSON schema: 'temp' field does not exist in $ (root) jobj");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'temp' field does not exist in $ (root) jobj");
        return;
    }

    if (!json_object_is_type(temp_field, json_type_int)) {
        PRINTLN_SO("Invalid JSON schema: 'temp' field is not an int");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'temp' field is not an int");
        return;
    }

    int temp = json_object_get_int(temp_field);
    if (temp < 0) {
        PRINTLN_SO("Invalid JSON schema: 'temp' field is not >= 0");
        RESPOND(client_fd, NULL, INVALID_JSON_SCHEMA, "Invalid JSON schema: 'temp' field is not >= 0");
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

    gl_nvml_result = nvmlDeviceSetTemperatureThreshold(device, threshold_type_t, &temp);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't set temperature threshold for uuid %s, type %d, temp %d", uuid, threshold_type_t, temp);
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't set temperature threshold! If this error is an INVALID_ARGUMENT type error, this might be a 'bug': https://forums.developer.nvidia.com/t/nvmldevicesettemperaturethreshold-api-returns-invalid-argument-error/279650/3");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Catastrophic failure when setting temperature for uuid %s, type %d, temp %d", uuid, threshold_type_t, temp);

    RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Successfully set temperature threshold!");
}

// ----------------------------- GENERIC  -----------------------------

void nvmlDeviceGetMemoryInfo_handler(const int client_fd, const json_object *jobj) {
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

    nvmlDevice_t device;
    gl_nvml_result = nvmlDeviceGetHandleByUUID(uuid, &device);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve UUID to any device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve UUID");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Couldn't get device handle w/ uuid %s", uuid);

    nvmlMemory_v2_t nvml_memory = {0};
    nvml_memory.version = NVML_STRUCT_VERSION(Memory, 2);
    gl_nvml_result = nvmlDeviceGetMemoryInfo_v2(device, &nvml_memory);
    if (ERROR(gl_nvml_result) || gl_nvml_result == NVML_ERROR_NOT_FOUND) {
        PRINTLN_SO("Couldn't resolve memory info to device!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't resolve memory info to device!");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Catastrophic failure when getting memory info for uuid %s", uuid);

    char buffer[256];
    sprintf(
        buffer,
        "{"
            "\"total\": %llu,"
            "\"free\": %llu,"
            "\"used\": %llu,"
            "\"reserved\": %llu"
        "}",
        nvml_memory.total,
        nvml_memory.free,
        nvml_memory.used,
        nvml_memory.reserved
    );
    RESPOND(client_fd, buffer, map_nvmlReturn_t_to_string(gl_nvml_result), NULL);
}

void nvmlDeviceGetDetailsAll_handler(const int client_fd, const json_object *jobj) {
    // half a meg will literally handle even small clusters, I'd hope lol (should fit about 400 devices, counting overhead)
    char* buffer = calloc(sizeof(char), 524288);
    unsigned int device_count;
    gl_nvml_result = nvmlDeviceGetCount_v2(&device_count);
    if (ERROR(gl_nvml_result)) {
        PRINTLN_SO("Couldn't get count of devices!");
        RESPOND(client_fd, NULL, map_nvmlReturn_t_to_string(gl_nvml_result), "Couldn't get count of devices");
        return;
    }
    if (FATAL(gl_nvml_result)) WTF("Catastrophic failure when grabbing device count! Is NVML instance up?");

    sprintf(buffer, "{\"count\": %d, \"devices\": [", device_count);
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
            "%s{"
                "\"uuid\":" "\"%s\","
                "\"name\":" "\"%s\","
                "\"gsp_version\":" "\"%s\","
                "\"gsp_mode\":" "%d,"
                "\"gsp_default-mode\":" "%d"
            "}",
            i > 0 ? "," : "",
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
    buffer[len] = ']';
    buffer[len + 1] = '}';

    const int status = failure_count > 0 ? NVML_ERROR_UNKNOWN : NVML_SUCCESS;
    const char* desc = failure_count > 0 ? "Failed to get details for some GPUs." : "Successfully successfully generated device list.";
    RESPOND(client_fd, buffer, map_nvmlReturn_t_to_string(status), desc);
    free(buffer);
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
