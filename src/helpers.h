#ifndef HELPERS_H
#define HELPERS_H

#include <nvml.h>
#include <stdlib.h>

typedef enum logLevel_enum: unsigned char {
    ERROR = 0b00000001,
    WARNING = 0b00000010,
    INFO = 0b00000100,
    DEBUG = 0b00001000,
    TRACE = 0b00010000
} logLevel_t;

extern logLevel_t current_log_level; // global; use for logging
extern char *log_buffer;
#define LOG_BUFFER_SIZE 16384
extern nvmlReturn_t gl_nvml_result; // global; use for panics

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

// ReSharper disable once CppNonInlineFunctionDefinitionInHeaderFile
void _logl(const logLevel_t level, const char *filename, const int lc, const char *fn, const char *fmt, ...);

#define LOG_ERROR(...) _logl(ERROR, __FILE_NAME__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#define LOG_WARNING(...) _logl(WARNING, __FILE_NAME__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#define LOG_INFO(...) _logl(INFO, __FILE_NAME__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#define LOG_DEBUG(...) _logl(DEBUG, __FILE_NAME__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#define LOG_TRACE(...) _logl(TRACE, __FILE_NAME__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#define WTF(...) \
    do { \
        LOG_ERROR("FATAL ERROR: %s", map_nvmlReturn_t_to_string(gl_nvml_result)); \
        LOG_ERROR(__VA_ARGS__); \
    } while (0)

typedef enum sizeDenominator_enum: unsigned long long {
    KILOBYTES = 1024,
    MEGABYTES = 1024 * KILOBYTES,
    GIGABYTES = 1024 * MEGABYTES,
    TERABYTES = 1024 * GIGABYTES,
} sizeDenominator_t;

/**
 * @param src INPUT/OUTPUT src to modify
 */
void rstrip(char *src);
double bytes_to_denominator(const sizeDenominator_t denominator, const unsigned long long byteCount);
nvmlRestrictedAPI_t map_nvmlRestrictedAPI_t_to_enum(const char *restricted_api);
nvmlClockId_t map_nvmlClockId_t_to_enum(const char *clock_id_s);
nvmlClockType_t map_nvmlClockType_t_to_enum(const char *clock_type_s);
nvmlPstates_t map_nvmlPstates_t_to_enum(const char *pstate_s);
nvmlPowerScopeType_t map_nvmlPowerScopeType_t_to_enum(const char *power_scope);
nvmlTemperatureThresholds_t map_nvmlTemperatureThresholds_t_to_enum(const char *temperature_thresholds);
char *map_nvmlReturn_t_to_string(const nvmlReturn_t nvmlReturn);
char *map_nvmlThermalController_t_to_string(const nvmlThermalController_t nvml_thermal_controller);
char *map_nvmlThermalTarget_t_to_string(const nvmlThermalTarget_t nvml_thermal_target);

#endif
