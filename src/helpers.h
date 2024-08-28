#ifndef HELPERS_H
#define HELPERS_H

#include <stdlib.h>
#include <stdio.h>
#include <nvml.h>

extern nvmlReturn_t gl_nvml_result;  // global; use for panics

#define PANIC(...) do { \
    PRINTLN(__VA_ARGS__); \
    PRINTLN("\033[31;1m[%s:%d][PANIC]\033[0m Fatal error %s", __FILE_NAME__, __LINE__, map_nvmlReturn_t_to_string(gl_nvml_result)); \
    nvmlShutdown(); /* might fail but who gives a shit when we panic lmao */ \
    exit(gl_nvml_result); \
    } while (0)

#define PRINTLN_SO(...) do { printf("\033[36;1m[%s:%d][SOCK]\033[0m ", __FILE_NAME__, __LINE__); printf(__VA_ARGS__); printf("\n"); } while (0)
#define PRINTLN(...) do { printf("\033[34;1m[%s:%d][INFO]\033[0m ", __FILE_NAME__, __LINE__); printf(__VA_ARGS__); printf("\n"); } while (0)

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

static char *map_nvmlReturn_t_to_string(const nvmlReturn_t nvmlReturn) {
    switch (nvmlReturn) {
        case NVML_SUCCESS:
            return "NVML_SUCCESS";
        case NVML_ERROR_UNINITIALIZED:
            return "NVML_ERROR_UNINITIALIZED";
        case NVML_ERROR_INVALID_ARGUMENT:  // error
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
        case NVML_ERROR_UNKNOWN:
            return "NVML_ERROR_UNKNOWN";
        default:
            PANIC("WTF UNKNOWN NVMLRETURN %d", nvmlReturn);
    }
}

#endif //HELPERS_H
