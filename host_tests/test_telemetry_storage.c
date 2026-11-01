#include <stdio.h>

#include "telemetry_storage.h"

static const char *path = "telemetry_storage_test.bin";

int main(void)
{
    TelemetryFileStorage storage;
    TelemetryStorageRecord input = {
        .sequence = 7U,
        .sensor = {.timestamp_ms = 99U, .temperature_centi_c = 2500,
                   .humidity_centi_percent = 6000U, .pressure_pa = 100000U,
                   .valid = 3U},
    };
    TelemetryStorageRecord output;
    FILE *file;
    const unsigned char truncated[] = {0x4CU, 0x54U, 0x01U};

    (void)remove(path);
    if (telemetry_storage_open(&storage, path) != 0 ||
        telemetry_storage_append(&storage, &input) != 0 ||
        telemetry_storage_count(&storage) != 1U ||
        telemetry_storage_peek(&storage, &output) != 0 ||
        output.sequence != input.sequence ||
        output.sensor.pressure_pa != input.sensor.pressure_pa) {
        fprintf(stderr, "storage append/peek failed\n");
        return 1;
    }
    if (telemetry_storage_acknowledge(&storage, 99U) == 0 ||
        telemetry_storage_acknowledge(&storage, input.sequence) != 0 ||
        telemetry_storage_count(&storage) != 0U) {
        fprintf(stderr, "storage acknowledgement failed\n");
        return 2;
    }
    if (telemetry_storage_append(&storage, &input) != 0) {
        return 3;
    }
    file = fopen(path, "ab");
    if (file == NULL) {
        return 4;
    }
    (void)fwrite(truncated, 1U, sizeof(truncated), file);
    (void)fclose(file);
    if (telemetry_storage_open(&storage, path) != 0 ||
        telemetry_storage_count(&storage) != 1U ||
        telemetry_storage_peek(&storage, &output) != 0 ||
        output.sequence != input.sequence) {
        fprintf(stderr, "incomplete record recovery failed\n");
        return 5;
    }
    (void)remove(path);
    puts("telemetry storage tests passed");
    return 0;
}
