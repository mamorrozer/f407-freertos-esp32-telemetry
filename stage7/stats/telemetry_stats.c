#include "telemetry_stats.h"

#include <stddef.h>

void telemetry_stats_init(TelemetryRuntimeStats *stats)
{
    if (stats != NULL)
    {
        *stats = (TelemetryRuntimeStats){0};
    }
}

void telemetry_stats_observe_sequence(TelemetryRuntimeStats *stats,
                                       uint32_t sequence)
{
    if (stats == NULL)
    {
        return;
    }
    stats->frames_received++;
    if (stats->sequence_valid && (sequence > stats->last_sequence) &&
        (sequence - stats->last_sequence > 1U))
    {
        stats->sequence_gaps += sequence - stats->last_sequence - 1U;
    }
    stats->last_sequence = sequence;
    stats->sequence_valid = true;
}
