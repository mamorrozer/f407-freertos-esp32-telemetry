#ifndef TELEMETRY_FAULTS_H
#define TELEMETRY_FAULTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    bool drop_next;
    bool corrupt_crc_next;
    bool corrupt_length_next;
    uint32_t delay_ms;
} TelemetryFaultPlan;

typedef struct
{
    uint32_t dropped;
    uint32_t crc_corrupted;
    uint32_t length_corrupted;
    uint32_t delayed;
} TelemetryFaultCounters;

void telemetry_faults_init(TelemetryFaultPlan *plan,
                            TelemetryFaultCounters *counters);
int telemetry_faults_apply(TelemetryFaultPlan *plan,
                           TelemetryFaultCounters *counters,
                           uint8_t *frame,
                           size_t length,
                           uint32_t *delay_ms);

#endif
