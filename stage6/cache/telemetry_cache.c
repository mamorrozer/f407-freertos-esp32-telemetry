#include "telemetry_cache.h"

#include <string.h>

void telemetry_cache_init(TelemetryCache *cache)
{
    if (cache != NULL)
    {
        memset(cache, 0, sizeof(*cache));
    }
}

int telemetry_cache_push(TelemetryCache *cache,
                         const TelemetryCacheRecord *record)
{
    size_t write_index;

    if ((cache == NULL) || (record == NULL))
    {
        return -1;
    }
    if (cache->count >= TELEMETRY_CACHE_CAPACITY)
    {
        cache->dropped++;
        return -2;
    }
    write_index = (cache->head + cache->count) % TELEMETRY_CACHE_CAPACITY;
    cache->records[write_index] = *record;
    cache->count++;
    cache->pushed++;
    return 0;
}

int telemetry_cache_peek(const TelemetryCache *cache,
                         TelemetryCacheRecord *record)
{
    if ((cache == NULL) || (record == NULL) || (cache->count == 0U))
    {
        return -1;
    }
    *record = cache->records[cache->head];
    return 0;
}

int telemetry_cache_pop(TelemetryCache *cache,
                        TelemetryCacheRecord *record)
{
    if (telemetry_cache_peek(cache, record) != 0)
    {
        return -1;
    }
    cache->head = (cache->head + 1U) % TELEMETRY_CACHE_CAPACITY;
    cache->count--;
    cache->popped++;
    return 0;
}

size_t telemetry_cache_count(const TelemetryCache *cache)
{
    return cache == NULL ? 0U : cache->count;
}
