#include <stdio.h>

#include "telemetry_config.h"

int main(void)
{
    TelemetryConfig config;

    telemetry_config_defaults(&config);
    if (telemetry_config_validate(&config) != TELEMETRY_CONFIG_OK ||
        telemetry_config_parse_command(
            "CFG period_ms=2000;temp_high=3500;humidity_high=8000;alarm=1",
            &config) != TELEMETRY_CONFIG_OK ||
        config.sample_period_ms != 2000U ||
        config.temperature_high_centi_c != 3500 ||
        config.humidity_high_centi_percent != 8000U ||
        !config.alarm_enabled) {
        fprintf(stderr, "valid configuration command failed\n");
        return 1;
    }

    if (telemetry_config_parse_command("CFG period_ms=99", &config) !=
            TELEMETRY_CONFIG_INVALID_VALUE ||
        config.sample_period_ms != 2000U) {
        fprintf(stderr, "out-of-range configuration was accepted\n");
        return 2;
    }
    if (telemetry_config_parse_command("CFG unknown=1", &config) !=
        TELEMETRY_CONFIG_UNKNOWN_KEY) {
        fprintf(stderr, "unknown configuration key was accepted\n");
        return 3;
    }
    if (telemetry_config_parse_command("CFG alarm=2", &config) !=
        TELEMETRY_CONFIG_INVALID_VALUE) {
        fprintf(stderr, "invalid boolean was accepted\n");
        return 4;
    }
    puts("telemetry config tests passed");
    return 0;
}
