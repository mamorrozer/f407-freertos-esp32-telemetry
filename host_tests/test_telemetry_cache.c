#include <stdio.h>

#include "telemetry_cache.h"

int main(void)
{
    TelemetryCache cache;
    TelemetryCacheRecord input = {.sequence = 10U};
    TelemetryCacheRecord output;

    if (telemetry_cache_count(NULL) != 0U ||
        telemetry_cache_push(NULL, &input) == 0 ||
        telemetry_cache_push(&cache, NULL) == 0 ||
        telemetry_cache_peek(NULL, &output) == 0 ||
        telemetry_cache_pop(NULL, &output) == 0)
    {
        fprintf(stderr, "cache invalid argument handling failed\n");
        return 1;
    }

    telemetry_cache_init(&cache);
    input.sensor.timestamp_ms = 100U;
    if (telemetry_cache_push(&cache, &input) != 0 ||
        telemetry_cache_peek(&cache, &output) != 0 ||
        output.sequence != 10U || output.sensor.timestamp_ms != 100U ||
        telemetry_cache_pop(&cache, &output) != 0 ||
        telemetry_cache_count(&cache) != 0U)
    {
        fprintf(stderr, "FIFO order test failed\n");
        return 2;
    }
    for (uint32_t sequence = 0U; sequence < TELEMETRY_CACHE_CAPACITY; ++sequence)
    {
        input.sequence = sequence;
        if (telemetry_cache_push(&cache, &input) != 0)
        {
            fprintf(stderr, "cache filled unexpectedly\n");
            return 3;
        }
    }
    if (telemetry_cache_push(&cache, &input) == 0 || cache.dropped != 1U)
    {
        fprintf(stderr, "overflow accounting failed\n");
        return 4;
    }
    for (uint32_t sequence = 0U; sequence < TELEMETRY_CACHE_CAPACITY; ++sequence)
    {
        if (telemetry_cache_pop(&cache, &output) != 0 ||
            output.sequence != sequence)
        {
            fprintf(stderr, "FIFO sequence failed at %lu\n",
                    (unsigned long)sequence);
            return 5;
        }
    }

    for (uint32_t sequence = 100U;
         sequence < 100U + TELEMETRY_CACHE_CAPACITY; ++sequence)
    {
        input.sequence = sequence;
        if (telemetry_cache_push(&cache, &input) != 0)
        {
            fprintf(stderr, "cache wrap-around fill failed\n");
            return 6;
        }
    }
    for (uint32_t sequence = 100U;
         sequence < 100U + TELEMETRY_CACHE_CAPACITY; ++sequence)
    {
        if (telemetry_cache_pop(&cache, &output) != 0 ||
            output.sequence != sequence)
        {
            fprintf(stderr, "cache wrap-around order failed\n");
            return 7;
        }
    }
    puts("telemetry cache tests passed");
    return 0;
}
