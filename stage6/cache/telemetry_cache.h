#ifndef TELEMETRY_CACHE_H
#define TELEMETRY_CACHE_H

#include <stddef.h>
#include <stdint.h>

#include "telemetry_protocol.h"

#ifndef TELEMETRY_CACHE_CAPACITY
#define TELEMETRY_CACHE_CAPACITY 16U
#endif

_Static_assert(TELEMETRY_CACHE_CAPACITY > 0U,
               "TELEMETRY_CACHE_CAPACITY must be greater than zero");

typedef struct
{
    uint32_t sequence;
    TelemetrySensorPayload sensor;
} TelemetryCacheRecord;

typedef struct
{
    TelemetryCacheRecord records[TELEMETRY_CACHE_CAPACITY];
    size_t head;
    size_t count;
    uint32_t dropped;
    uint32_t pushed;
    uint32_t popped;
} TelemetryCache;

void telemetry_cache_init(TelemetryCache *cache);
int telemetry_cache_push(TelemetryCache *cache,
                         const TelemetryCacheRecord *record);
int telemetry_cache_peek(const TelemetryCache *cache,
                         TelemetryCacheRecord *record);
int telemetry_cache_pop(TelemetryCache *cache,
                        TelemetryCacheRecord *record);
size_t telemetry_cache_count(const TelemetryCache *cache);

#endif
