#include <stdio.h>

#include "telemetry_health.h"

int main(void)
{
    TelemetryHealthWatch watch;

    telemetry_health_init(&watch, 100U);
    if (telemetry_health_is_stale(&watch, 0U)) {
        fprintf(stderr, "unseen task was incorrectly considered stale\n");
        return 1;
    }
    telemetry_health_seen(&watch, 1000U);
    if (telemetry_health_is_stale(&watch, 1099U) ||
        !telemetry_health_is_stale(&watch, 1100U)) {
        fprintf(stderr, "watchdog boundary behavior failed\n");
        return 2;
    }
    puts("telemetry health tests passed");
    return 0;
}
