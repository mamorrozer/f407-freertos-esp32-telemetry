#ifndef TELEMETRY_POWER_POLICY_H
#define TELEMETRY_POWER_POLICY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    TELEMETRY_POWER_ACTIVE = 0,
    TELEMETRY_POWER_IDLE = 1,
    TELEMETRY_POWER_SLEEP_ELIGIBLE = 2
} TelemetryPowerState;

typedef struct
{
    bool network_ready;
    bool pending_data;
    bool sample_due;
    uint32_t idle_ms;
} TelemetryPowerInputs;

TelemetryPowerState telemetry_power_evaluate(const TelemetryPowerInputs *inputs);

#endif
