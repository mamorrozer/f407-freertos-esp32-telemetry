#include "telemetry_health.h"

#include <stddef.h>

void telemetry_health_init(TelemetryHealthWatch *watch, uint32_t timeout_ms)
{
    if (watch != NULL)
    {
        *watch = (TelemetryHealthWatch){.timeout_ms = timeout_ms};
    }
}

void telemetry_health_seen(TelemetryHealthWatch *watch, uint32_t now_ms)
{
    if (watch != NULL)
    {
        watch->last_seen_ms = now_ms;
        watch->seen = true;
    }
}

bool telemetry_health_is_stale(const TelemetryHealthWatch *watch,
                               uint32_t now_ms)
{
    if ((watch == NULL) || !watch->seen)
    {
        return false;
    }
    return (uint32_t)(now_ms - watch->last_seen_ms) >= watch->timeout_ms;
}
