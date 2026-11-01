#include <stdio.h>

#include "telemetry_power_policy.h"

int main(void)
{
    TelemetryPowerInputs inputs = {
        .network_ready = true,
        .pending_data = false,
        .sample_due = false,
        .idle_ms = 1500U,
    };

    if (telemetry_power_evaluate(&inputs) != TELEMETRY_POWER_SLEEP_ELIGIBLE) {
        fprintf(stderr, "sleep eligibility was not recognized\n");
        return 1;
    }
    inputs.pending_data = true;
    if (telemetry_power_evaluate(&inputs) != TELEMETRY_POWER_ACTIVE) {
        fprintf(stderr, "pending data was ignored\n");
        return 2;
    }
    inputs.pending_data = false;
    inputs.idle_ms = 100U;
    if (telemetry_power_evaluate(&inputs) != TELEMETRY_POWER_IDLE) {
        fprintf(stderr, "idle state was not recognized\n");
        return 3;
    }
    puts("telemetry power policy tests passed");
    return 0;
}
