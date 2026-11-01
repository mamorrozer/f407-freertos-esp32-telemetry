#include "telemetry_power_policy.h"

#include <stddef.h>

TelemetryPowerState telemetry_power_evaluate(const TelemetryPowerInputs *inputs)
{
    if (inputs == NULL || inputs->pending_data || inputs->sample_due)
    {
        return TELEMETRY_POWER_ACTIVE;
    }
    if (inputs->network_ready && (inputs->idle_ms >= 1000U))
    {
        return TELEMETRY_POWER_SLEEP_ELIGIBLE;
    }
    return TELEMETRY_POWER_IDLE;
}
