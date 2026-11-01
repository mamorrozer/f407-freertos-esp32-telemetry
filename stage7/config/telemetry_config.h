#ifndef TELEMETRY_CONFIG_H
#define TELEMETRY_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define TELEMETRY_CONFIG_VERSION 1U
#define TELEMETRY_SAMPLE_PERIOD_MIN_MS 100U
#define TELEMETRY_SAMPLE_PERIOD_MAX_MS 60000U
#define TELEMETRY_TEMP_HIGH_MIN_CENTI_C (-4000)
#define TELEMETRY_TEMP_HIGH_MAX_CENTI_C 8500
#define TELEMETRY_HUMIDITY_HIGH_MIN_CENTI_PERCENT 0U
#define TELEMETRY_HUMIDITY_HIGH_MAX_CENTI_PERCENT 10000U

typedef struct
{
    uint32_t version;
    uint32_t sample_period_ms;
    int32_t temperature_high_centi_c;
    uint32_t humidity_high_centi_percent;
    bool alarm_enabled;
} TelemetryConfig;

typedef enum
{
    TELEMETRY_CONFIG_OK = 0,
    TELEMETRY_CONFIG_INVALID_ARGUMENT = -1,
    TELEMETRY_CONFIG_UNKNOWN_KEY = -2,
    TELEMETRY_CONFIG_INVALID_VALUE = -3,
    TELEMETRY_CONFIG_EMPTY = -4
} TelemetryConfigResult;

void telemetry_config_defaults(TelemetryConfig *config);
TelemetryConfigResult telemetry_config_validate(const TelemetryConfig *config);
TelemetryConfigResult telemetry_config_parse_command(const char *command,
                                                      TelemetryConfig *config);

#endif
