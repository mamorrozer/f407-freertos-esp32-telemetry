#ifndef TELEMETRY_STATS_H
#define TELEMETRY_STATS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t frames_received;
    uint32_t uart_bytes_received;
    uint32_t parse_failures;
    uint32_t crc_errors;
    uint32_t format_errors;
    uint32_t sequence_gaps;
    uint32_t publish_accepted;
    uint32_t publish_ok;
    uint32_t publish_fail;
    uint32_t publish_expired;
    uint32_t cache_pushed;
    uint32_t cache_dropped;
    uint32_t cache_flushed;
    uint32_t wifi_reconnects;
    uint32_t mqtt_connects;
    uint32_t mqtt_disconnects;
    uint32_t last_sequence;
    bool sequence_valid;
} TelemetryRuntimeStats;

void telemetry_stats_init(TelemetryRuntimeStats *stats);
void telemetry_stats_observe_sequence(TelemetryRuntimeStats *stats,
                                       uint32_t sequence);

#endif
