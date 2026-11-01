#ifndef TELEMETRY_STORAGE_H
#define TELEMETRY_STORAGE_H

#include <stddef.h>
#include <stdint.h>

#include "telemetry_protocol.h"

#define TELEMETRY_STORAGE_MAX_RECORDS 32U
#define TELEMETRY_STORAGE_RECORD_SIZE 27U

typedef struct
{
    uint32_t sequence;
    TelemetrySensorPayload sensor;
} TelemetryStorageRecord;

typedef struct
{
    char path[260];
    TelemetryStorageRecord records[TELEMETRY_STORAGE_MAX_RECORDS];
    size_t count;
} TelemetryFileStorage;

int telemetry_storage_open(TelemetryFileStorage *storage, const char *path);
int telemetry_storage_append(TelemetryFileStorage *storage,
                             const TelemetryStorageRecord *record);
int telemetry_storage_peek(const TelemetryFileStorage *storage,
                           TelemetryStorageRecord *record);
int telemetry_storage_acknowledge(TelemetryFileStorage *storage,
                                  uint32_t sequence);
size_t telemetry_storage_count(const TelemetryFileStorage *storage);

int telemetry_storage_encode_record(const TelemetryStorageRecord *record,
                                    uint8_t *buffer, size_t capacity);
int telemetry_storage_decode_record(const uint8_t *buffer, size_t length,
                                    TelemetryStorageRecord *record);

#endif
