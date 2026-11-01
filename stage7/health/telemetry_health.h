#ifndef TELEMETRY_HEALTH_H
#define TELEMETRY_HEALTH_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t last_seen_ms;
    uint32_t timeout_ms;
    bool seen;
} TelemetryHealthWatch;

void telemetry_health_init(TelemetryHealthWatch *watch, uint32_t timeout_ms);
void telemetry_health_seen(TelemetryHealthWatch *watch, uint32_t now_ms);
bool telemetry_health_is_stale(const TelemetryHealthWatch *watch,
                               uint32_t now_ms);

#endif
