#include "telemetry_storage.h"

#include <stdio.h>
#include <string.h>

#define STORAGE_MAGIC0 0x4CU
#define STORAGE_MAGIC1 0x54U
#define STORAGE_VERSION 1U
#define STORAGE_PAYLOAD_SIZE 21U
#define STORAGE_CRC_OFFSET 25U

static void put_u32_le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static uint32_t get_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

int telemetry_storage_encode_record(const TelemetryStorageRecord *record,
                                    uint8_t *buffer, size_t capacity)
{
    uint16_t crc;

    if ((record == NULL) || (buffer == NULL) ||
        (capacity < TELEMETRY_STORAGE_RECORD_SIZE))
    {
        return -1;
    }

    buffer[0] = STORAGE_MAGIC0;
    buffer[1] = STORAGE_MAGIC1;
    buffer[2] = STORAGE_VERSION;
    buffer[3] = STORAGE_PAYLOAD_SIZE;
    put_u32_le(&buffer[4], record->sequence);
    put_u32_le(&buffer[8], record->sensor.timestamp_ms);
    put_u32_le(&buffer[12], (uint32_t)record->sensor.temperature_centi_c);
    put_u32_le(&buffer[16], record->sensor.humidity_centi_percent);
    put_u32_le(&buffer[20], record->sensor.pressure_pa);
    buffer[24] = record->sensor.valid;
    crc = telemetry_crc16(buffer, STORAGE_CRC_OFFSET);
    buffer[25] = (uint8_t)crc;
    buffer[26] = (uint8_t)(crc >> 8);
    return 0;
}

int telemetry_storage_decode_record(const uint8_t *buffer, size_t length,
                                    TelemetryStorageRecord *record)
{
    uint16_t received_crc;

    if ((buffer == NULL) || (record == NULL) ||
        (length < TELEMETRY_STORAGE_RECORD_SIZE) ||
        (buffer[0] != STORAGE_MAGIC0) || (buffer[1] != STORAGE_MAGIC1) ||
        (buffer[2] != STORAGE_VERSION) ||
        (buffer[3] != STORAGE_PAYLOAD_SIZE))
    {
        return -1;
    }
    received_crc = (uint16_t)buffer[25] | ((uint16_t)buffer[26] << 8);
    if (received_crc != telemetry_crc16(buffer, STORAGE_CRC_OFFSET))
    {
        return -1;
    }
    record->sequence = get_u32_le(&buffer[4]);
    record->sensor.timestamp_ms = get_u32_le(&buffer[8]);
    record->sensor.temperature_centi_c = (int32_t)get_u32_le(&buffer[12]);
    record->sensor.humidity_centi_percent = get_u32_le(&buffer[16]);
    record->sensor.pressure_pa = get_u32_le(&buffer[20]);
    record->sensor.valid = buffer[24];
    return 0;
}

static int rewrite_file(const TelemetryFileStorage *storage)
{
    FILE *file = fopen(storage->path, "wb");
    uint8_t buffer[TELEMETRY_STORAGE_RECORD_SIZE];

    if (file == NULL)
    {
        return -1;
    }
    for (size_t i = 0U; i < storage->count; ++i)
    {
        if (telemetry_storage_encode_record(&storage->records[i], buffer,
                                            sizeof(buffer)) != 0)
        {
            (void)fclose(file);
            return -1;
        }
        if (fwrite(buffer, 1U, sizeof(buffer), file) != sizeof(buffer))
        {
            (void)fclose(file);
            return -1;
        }
    }
    if (fflush(file) != 0 || fclose(file) != 0)
    {
        return -1;
    }
    return 0;
}

int telemetry_storage_open(TelemetryFileStorage *storage, const char *path)
{
    FILE *file;
    uint8_t buffer[TELEMETRY_STORAGE_RECORD_SIZE];

    if ((storage == NULL) || (path == NULL) ||
        (strlen(path) >= sizeof(storage->path)))
    {
        return -1;
    }
    *storage = (TelemetryFileStorage){0};
    (void)strcpy(storage->path, path);
    file = fopen(storage->path, "rb");
    if (file == NULL)
    {
        return rewrite_file(storage);
    }
    while (storage->count < TELEMETRY_STORAGE_MAX_RECORDS)
    {
        size_t read_count = fread(buffer, 1U, sizeof(buffer), file);
        TelemetryStorageRecord record;
        if (read_count != sizeof(buffer) ||
            telemetry_storage_decode_record(buffer, read_count, &record) != 0)
        {
            break;
        }
        storage->records[storage->count++] = record;
    }
    (void)fclose(file);
    return rewrite_file(storage);
}

int telemetry_storage_append(TelemetryFileStorage *storage,
                             const TelemetryStorageRecord *record)
{
    FILE *file;
    uint8_t buffer[TELEMETRY_STORAGE_RECORD_SIZE];

    if ((storage == NULL) || (record == NULL) ||
        (storage->count >= TELEMETRY_STORAGE_MAX_RECORDS))
    {
        return -1;
    }
    if (telemetry_storage_encode_record(record, buffer, sizeof(buffer)) != 0)
    {
        return -1;
    }
    file = fopen(storage->path, "ab");
    if (file == NULL)
    {
        return -1;
    }
    if ((fwrite(buffer, 1U, sizeof(buffer), file) != sizeof(buffer)) ||
        (fflush(file) != 0))
    {
        (void)fclose(file);
        return -1;
    }
    if (fclose(file) != 0)
    {
        return -1;
    }
    storage->records[storage->count++] = *record;
    return 0;
}

int telemetry_storage_peek(const TelemetryFileStorage *storage,
                           TelemetryStorageRecord *record)
{
    if ((storage == NULL) || (record == NULL) || (storage->count == 0U))
    {
        return -1;
    }
    *record = storage->records[0];
    return 0;
}

int telemetry_storage_acknowledge(TelemetryFileStorage *storage,
                                  uint32_t sequence)
{
    if ((storage == NULL) || (storage->count == 0U) ||
        (storage->records[0].sequence != sequence))
    {
        return -1;
    }
    for (size_t i = 1U; i < storage->count; ++i)
    {
        storage->records[i - 1U] = storage->records[i];
    }
    storage->count--;
    return rewrite_file(storage);
}

size_t telemetry_storage_count(const TelemetryFileStorage *storage)
{
    return storage == NULL ? 0U : storage->count;
}
