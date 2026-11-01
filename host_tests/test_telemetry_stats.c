#include <stdio.h>

#include "telemetry_stats.h"

int main(void)
{
    TelemetryRuntimeStats stats;

    telemetry_stats_init(&stats);
    telemetry_stats_observe_sequence(&stats, 10U);
    telemetry_stats_observe_sequence(&stats, 11U);
    telemetry_stats_observe_sequence(&stats, 14U);
    if (!stats.sequence_valid || stats.last_sequence != 14U ||
        stats.frames_received != 3U || stats.sequence_gaps != 2U) {
        fprintf(stderr, "sequence statistics failed\n");
        return 1;
    }
    telemetry_stats_observe_sequence(&stats, 14U);
    if (stats.sequence_gaps != 2U) {
        fprintf(stderr, "duplicate sequence was counted as a gap\n");
        return 2;
    }
    puts("telemetry stats tests passed");
    return 0;
}
