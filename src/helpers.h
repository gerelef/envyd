#ifndef HELPERS_H
#define HELPERS_H

#include <assert.h>
#include <limits.h>
#include <nvml.h>

extern nvmlReturn_t gl_nvml_result; // global; use for panics

#define WTF(...) do { \
    PRINTLN(__VA_ARGS__); \
    do { printf("\033[31m[%s:%d][%s][PANIC][FATAL ERROR: %s]\033[0m ", __FILE_NAME__, __LINE__, __PRETTY_FUNCTION__, map_nvmlReturn_t_to_string(gl_nvml_result)); printf(__VA_ARGS__); printf("\n"); } while (0); \
    nvmlShutdown(); /* might fail but who gives a shit when we panic lmao */ \
    exit(gl_nvml_result); \
    } while (0)

#define MEMSET_BUFF(buff, len) do { assert(so_buffer != NULL); memset(so_buffer, 0, len); } while(0)

#define PRINTLN_SO(...) do { printf("\033[36m[%s:%d][%s][SOCK]\033[0m ", __FILE_NAME__, __LINE__, __PRETTY_FUNCTION__); printf(__VA_ARGS__); printf("\n"); } while (0)
#define PRINTLN(...) do { printf("\033[34m[%s:%d][%s][INFO]\033[0m ", __FILE_NAME__, __LINE__, __PRETTY_FUNCTION__); printf(__VA_ARGS__); printf("\n"); } while (0)

#define OK(nvmlReturn) nvmlReturn == NVML_SUCCESS
#define ERROR(nvmlReturn) nvmlReturn == NVML_ERROR_INVALID_ARGUMENT \
    || nvmlReturn == NVML_ERROR_NOT_SUPPORTED \
    || nvmlReturn == NVML_ERROR_NO_PERMISSION \
    || nvmlReturn == NVML_ERROR_INSUFFICIENT_SIZE \
    || nvmlReturn == NVML_ERROR_INSUFFICIENT_POWER \
    || nvmlReturn == NVML_ERROR_GPU_IS_LOST \
    || nvmlReturn == NVML_ERROR_RESET_REQUIRED \
    || nvmlReturn == NVML_ERROR_OPERATING_SYSTEM \
    || nvmlReturn == NVML_ERROR_LIB_RM_VERSION_MISMATCH \
    || nvmlReturn == NVML_ERROR_IN_USE \
    || nvmlReturn == NVML_ERROR_MEMORY \
    || nvmlReturn == NVML_ERROR_NO_DATA \
    || nvmlReturn == NVML_ERROR_VGPU_ECC_NOT_SUPPORTED \
    || nvmlReturn == NVML_ERROR_INSUFFICIENT_RESOURCES \
    || nvmlReturn == NVML_ERROR_FREQ_NOT_SUPPORTED \
    || nvmlReturn == NVML_ERROR_INSUFFICIENT_RESOURCES \
    || nvmlReturn == NVML_ERROR_FREQ_NOT_SUPPORTED \
    || nvmlReturn == NVML_ERROR_ARGUMENT_VERSION_MISMATCH \
    || nvmlReturn == NVML_ERROR_NOT_READY \
    || nvmlReturn == NVML_ERROR_GPU_NOT_FOUND \
    || nvmlReturn == NVML_ERROR_INVALID_STATE
#define FATAL(nvmlReturn) !(OK(nvmlReturn) || ERROR(nvmlReturn))

typedef enum sizeDenominator_enum: unsigned long long {
    KILOBYTES = 1024,
    MEGABYTES = 1024 * KILOBYTES,
    GIGABYTES = 1024 * MEGABYTES,
    TERABYTES = 1024 * GIGABYTES,
} sizeDenominator_t;

static double bytes_to_denominator(const sizeDenominator_t denominator, const unsigned long long byteCount) {
    return (double) byteCount / (double) denominator;
}

static nvmlRestrictedAPI_t map_nvmlRestrictedAPI_t_to_enum(const char *restricted_api) {
    if (strcmp("NVML_RESTRICTED_API_SET_APPLICATION_CLOCKS", restricted_api) == 0) {
        return NVML_RESTRICTED_API_SET_APPLICATION_CLOCKS;
    }
    if (strcmp("NVML_RESTRICTED_API_SET_AUTO_BOOSTED_CLOCKS", restricted_api) == 0) {
        return NVML_RESTRICTED_API_SET_AUTO_BOOSTED_CLOCKS;
    }
    return NVML_RESTRICTED_API_COUNT;
}

static nvmlClockId_t map_nvmlClockId_t_to_enum(const char *clock_id_s) {
    if (strcmp("NVML_CLOCK_ID_CURRENT", clock_id_s) == 0) {
        return NVML_CLOCK_ID_CURRENT;
    }
    if (strcmp("NVML_CLOCK_ID_APP_CLOCK_TARGET", clock_id_s) == 0) {
        return NVML_CLOCK_ID_APP_CLOCK_TARGET;
    }
    if (strcmp("NVML_CLOCK_ID_APP_CLOCK_DEFAULT", clock_id_s) == 0) {
        return NVML_CLOCK_ID_APP_CLOCK_DEFAULT;
    }
    if (strcmp("NVML_CLOCK_ID_CUSTOMER_BOOST_MAX", clock_id_s) == 0) {
        return NVML_CLOCK_ID_CUSTOMER_BOOST_MAX;
    }
    return NVML_CLOCK_ID_COUNT;
}

static nvmlClockType_t map_nvmlClockType_t_to_enum(const char *clock_type_s) {
    if (strcmp("NVML_CLOCK_GRAPHICS", clock_type_s) == 0) {
        return NVML_CLOCK_GRAPHICS;
    }
    if (strcmp("NVML_CLOCK_SM", clock_type_s) == 0) {
        return NVML_CLOCK_SM;
    }
    if (strcmp("NVML_CLOCK_MEM", clock_type_s) == 0) {
        return NVML_CLOCK_MEM;
    }
    if (strcmp("NVML_CLOCK_VIDEO", clock_type_s) == 0) {
        return NVML_CLOCK_VIDEO;
    }
    return NVML_CLOCK_COUNT;
}

static nvmlPstates_t map_nvmlPstates_t_to_enum(const char *pstate_s) {
    if (strcmp("NVML_PSTATE_0", pstate_s) == 0) {
        return NVML_PSTATE_0;
    }
    if (strcmp("NVML_PSTATE_1", pstate_s) == 0) {
        return NVML_PSTATE_1;
    }
    if (strcmp("NVML_PSTATE_2", pstate_s) == 0) {
        return NVML_PSTATE_2;
    }
    if (strcmp("NVML_PSTATE_3", pstate_s) == 0) {
        return NVML_PSTATE_3;
    }
    if (strcmp("NVML_PSTATE_4", pstate_s) == 0) {
        return NVML_PSTATE_4;
    }
    if (strcmp("NVML_PSTATE_5", pstate_s) == 0) {
        return NVML_PSTATE_5;
    }
    if (strcmp("NVML_PSTATE_6", pstate_s) == 0) {
        return NVML_PSTATE_6;
    }
    if (strcmp("NVML_PSTATE_7", pstate_s) == 0) {
        return NVML_PSTATE_7;
    }
    if (strcmp("NVML_PSTATE_8", pstate_s) == 0) {
        return NVML_PSTATE_8;
    }
    if (strcmp("NVML_PSTATE_9", pstate_s) == 0) {
        return NVML_PSTATE_9;
    }
    if (strcmp("NVML_PSTATE_10", pstate_s) == 0) {
        return NVML_PSTATE_10;
    }
    if (strcmp("NVML_PSTATE_11", pstate_s) == 0) {
        return NVML_PSTATE_11;
    }
    if (strcmp("NVML_PSTATE_12", pstate_s) == 0) {
        return NVML_PSTATE_12;
    }
    if (strcmp("NVML_PSTATE_13", pstate_s) == 0) {
        return NVML_PSTATE_13;
    }
    if (strcmp("NVML_PSTATE_14", pstate_s) == 0) {
        return NVML_PSTATE_14;
    }
    if (strcmp("NVML_PSTATE_15", pstate_s) == 0) {
        return NVML_PSTATE_15;
    }

    return NVML_PSTATE_UNKNOWN;
}

static nvmlPowerScopeType_t map_nvmlPowerScopeType_t_to_enum(const char *power_scope) {
    if (strcmp("NVML_POWER_SCOPE_GPU", power_scope) == 0) {
        return NVML_POWER_SCOPE_GPU;
    }
    if (strcmp("NVML_POWER_SCOPE_MODULE", power_scope) == 0) {
        return NVML_POWER_SCOPE_MODULE;
    }
    if (strcmp("NVML_POWER_SCOPE_MEMORY", power_scope) == 0) {
        return NVML_POWER_SCOPE_MEMORY;
    }
    return CHAR_MAX;
}

static nvmlTemperatureThresholds_t map_nvmlTemperatureThresholds_t_to_enum(const char *temperature_thresholds) {
    if (strcmp("NVML_TEMPERATURE_THRESHOLD_SHUTDOWN", temperature_thresholds) == 0) {
        return NVML_TEMPERATURE_THRESHOLD_SHUTDOWN;
    }
    if (strcmp("NVML_TEMPERATURE_THRESHOLD_SLOWDOWN", temperature_thresholds) == 0) {
        return NVML_TEMPERATURE_THRESHOLD_SLOWDOWN;
    }
    if (strcmp("NVML_TEMPERATURE_THRESHOLD_MEM_MAX", temperature_thresholds) == 0) {
        return NVML_TEMPERATURE_THRESHOLD_MEM_MAX;
    }
    if (strcmp("NVML_TEMPERATURE_THRESHOLD_GPU_MAX", temperature_thresholds) == 0) {
        return NVML_TEMPERATURE_THRESHOLD_GPU_MAX;
    }
    if (strcmp("NVML_TEMPERATURE_THRESHOLD_ACOUSTIC_MIN", temperature_thresholds) == 0) {
        return NVML_TEMPERATURE_THRESHOLD_ACOUSTIC_MIN;
    }
    if (strcmp("NVML_TEMPERATURE_THRESHOLD_ACOUSTIC_CURR", temperature_thresholds) == 0) {
        return NVML_TEMPERATURE_THRESHOLD_ACOUSTIC_CURR;
    }
    if (strcmp("NVML_TEMPERATURE_THRESHOLD_ACOUSTIC_MAX", temperature_thresholds) == 0) {
        return NVML_TEMPERATURE_THRESHOLD_ACOUSTIC_MAX;
    }
    if (strcmp("NVML_TEMPERATURE_THRESHOLD_GPS_CURR", temperature_thresholds) == 0) {
        return NVML_TEMPERATURE_THRESHOLD_GPS_CURR;
    }
    return NVML_TEMPERATURE_THRESHOLD_COUNT;
}

static char *map_nvmlReturn_t_to_string(const nvmlReturn_t nvmlReturn) {
    switch (nvmlReturn) {
        case NVML_SUCCESS:
            return "NVML_SUCCESS";
        case NVML_ERROR_UNINITIALIZED:
            return "NVML_ERROR_UNINITIALIZED";
        case NVML_ERROR_INVALID_ARGUMENT: // error
            return "NVML_ERROR_INVALID_ARGUMENT";
        case NVML_ERROR_NOT_SUPPORTED: // error
            return "NVML_ERROR_NOT_SUPPORTED";
        case NVML_ERROR_NO_PERMISSION: // error
            return "NVML_ERROR_NO_PERMISSION";
        case NVML_ERROR_ALREADY_INITIALIZED:
            return "NVML_ERROR_ALREADY_INITIALIZED";
        case NVML_ERROR_NOT_FOUND:
            return "NVML_ERROR_NOT_FOUND";
        case NVML_ERROR_INSUFFICIENT_SIZE: // error
            return "NVML_ERROR_INSUFFICIENT_SIZE";
        case NVML_ERROR_INSUFFICIENT_POWER: // error
            return "NVML_ERROR_INSUFFICIENT_POWER";
        case NVML_ERROR_DRIVER_NOT_LOADED:
            return "NVML_ERROR_DRIVER_NOT_LOADED";
        case NVML_ERROR_TIMEOUT:
            return "NVML_ERROR_TIMEOUT";
        case NVML_ERROR_IRQ_ISSUE:
            return "NVML_ERROR_IRQ_ISSUE";
        case NVML_ERROR_LIBRARY_NOT_FOUND:
            return "NVML_ERROR_LIBRARY_NOT_FOUND";
        case NVML_ERROR_FUNCTION_NOT_FOUND:
            return "NVML_ERROR_FUNCTION_NOT_FOUND";
        case NVML_ERROR_CORRUPTED_INFOROM:
            return "NVML_ERROR_CORRUPTED_INFOROM";
        case NVML_ERROR_GPU_IS_LOST: // error
            return "NVML_ERROR_GPU_IS_LOST";
        case NVML_ERROR_RESET_REQUIRED: // error
            return "NVML_ERROR_RESET_REQUIRED";
        case NVML_ERROR_OPERATING_SYSTEM: // error
            return "NVML_ERROR_OPERATING_SYSTEM";
        case NVML_ERROR_LIB_RM_VERSION_MISMATCH: // error
            return "NVML_ERROR_LIB_RM_VERSION_MISMATCH";
        case NVML_ERROR_IN_USE: // error
            return "NVML_ERROR_IN_USE";
        case NVML_ERROR_MEMORY: // error
            return "NVML_ERROR_MEMORY";
        case NVML_ERROR_NO_DATA: // error
            return "NVML_ERROR_NO_DATA";
        case NVML_ERROR_VGPU_ECC_NOT_SUPPORTED: // error
            return "NVML_ERROR_VGPU_ECC_NOT_SUPPORTED";
        case NVML_ERROR_INSUFFICIENT_RESOURCES: // error
            return "NVML_ERROR_INSUFFICIENT_RESOURCES";
        case NVML_ERROR_FREQ_NOT_SUPPORTED: // error
            return "NVML_ERROR_FREQ_NOT_SUPPORTED";
        case NVML_ERROR_ARGUMENT_VERSION_MISMATCH: // error
            return "NVML_ERROR_ARGUMENT_VERSION_MISMATCH";
        case NVML_ERROR_DEPRECATED:
            return "NVML_ERROR_DEPRECATED";
        case NVML_ERROR_NOT_READY: // error
            return "NVML_ERROR_NOT_READY";
        case NVML_ERROR_GPU_NOT_FOUND: // error
            return "NVML_ERROR_GPU_NOT_FOUND";
        case NVML_ERROR_INVALID_STATE: // error
            return "NVML_ERROR_INVALID_STATE";
        default:
            return "NVML_ERROR_UNKNOWN";
    }
}

static char *map_nvmlThermalController_t_to_string(const nvmlThermalController_t nvml_thermal_controller) {
    switch (nvml_thermal_controller) {
        case NVML_THERMAL_CONTROLLER_NONE:
            return "NVML_THERMAL_CONTROLLER_NONE";
        case NVML_THERMAL_CONTROLLER_GPU_INTERNAL:
            return "NVML_THERMAL_CONTROLLER_GPU_INTERNAL";
        case NVML_THERMAL_CONTROLLER_ADM1032:
            return "NVML_THERMAL_CONTROLLER_ADM1032";
        case NVML_THERMAL_CONTROLLER_ADT7461:
            return "NVML_THERMAL_CONTROLLER_ADT7461";
        case NVML_THERMAL_CONTROLLER_MAX6649:
            return "NVML_THERMAL_CONTROLLER_MAX6649";
        case NVML_THERMAL_CONTROLLER_MAX1617:
            return "NVML_THERMAL_CONTROLLER_MAX1617";
        case NVML_THERMAL_CONTROLLER_LM99:
            return "NVML_THERMAL_CONTROLLER_LM99";
        case NVML_THERMAL_CONTROLLER_LM89:
            return "NVML_THERMAL_CONTROLLER_LM89";
        case NVML_THERMAL_CONTROLLER_LM64:
            return "NVML_THERMAL_CONTROLLER_LM64";
        case NVML_THERMAL_CONTROLLER_G781:
            return "NVML_THERMAL_CONTROLLER_G781";
        case NVML_THERMAL_CONTROLLER_ADT7473:
            return "NVML_THERMAL_CONTROLLER_ADT7473";
        case NVML_THERMAL_CONTROLLER_SBMAX6649:
            return "NVML_THERMAL_CONTROLLER_SBMAX6649";
        case NVML_THERMAL_CONTROLLER_VBIOSEVT:
            return "NVML_THERMAL_CONTROLLER_VBIOSEVT";
        case NVML_THERMAL_CONTROLLER_OS:
            return "NVML_THERMAL_CONTROLLER_OS";
        case NVML_THERMAL_CONTROLLER_NVSYSCON_CANOAS:
            return "NVML_THERMAL_CONTROLLER_NVSYSCON_CANOAS";
        case NVML_THERMAL_CONTROLLER_NVSYSCON_E551:
            return "NVML_THERMAL_CONTROLLER_NVSYSCON_E551";
        case NVML_THERMAL_CONTROLLER_MAX6649R:
            return "NVML_THERMAL_CONTROLLER_MAX6649R";
        case NVML_THERMAL_CONTROLLER_ADT7473S:
            return "NVML_THERMAL_CONTROLLER_ADT7473S";
        default:
            return "NVML_THERMAL_CONTROLLER_UNKNOWN";
    }
}

static char *map_nvmlThermalTarget_t_to_string(const nvmlThermalTarget_t nvml_thermal_target) {
    switch (nvml_thermal_target) {
        case NVML_THERMAL_TARGET_NONE:
            return "NVML_THERMAL_TARGET_NONE";
        case NVML_THERMAL_TARGET_GPU:
            return "NVML_THERMAL_TARGET_GPU";
        case NVML_THERMAL_TARGET_MEMORY:
            return "NVML_THERMAL_TARGET_MEMORY";
        case NVML_THERMAL_TARGET_POWER_SUPPLY:
            return "NVML_THERMAL_TARGET_POWER_SUPPLY";
        case NVML_THERMAL_TARGET_BOARD:
            return "NVML_THERMAL_TARGET_BOARD";
        case NVML_THERMAL_TARGET_VCD_BOARD:
            return "NVML_THERMAL_TARGET_VCD_BOARD";
        case NVML_THERMAL_TARGET_VCD_INLET:
            return "NVML_THERMAL_TARGET_VCD_INLET";
        case NVML_THERMAL_TARGET_VCD_OUTLET:
            return "NVML_THERMAL_TARGET_VCD_OUTLET";
        case NVML_THERMAL_TARGET_ALL:
            return "NVML_THERMAL_TARGET_ALL";
        default:
            return "NVML_THERMAL_TARGET_UNKNOWN";
    }
}

#endif
