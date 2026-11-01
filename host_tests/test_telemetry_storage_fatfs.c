#include <stdio.h>
#include <string.h>

#include "telemetry_storage_fatfs.h"

#define FAKE_CAPACITY 1024U

typedef struct
{
    unsigned char bytes[FAKE_CAPACITY];
    size_t length;
    size_t position;
} FakeFile;

static int fake_open(void *context, const char *path, void **handle)
{
    (void)path;
    *handle = context;
    return 0;
}

static int fake_close(void *context, void *handle)
{
    (void)context;
    (void)handle;
    return 0;
}

static int fake_read(void *context, void *handle, uint8_t *buffer,
                     size_t capacity, size_t *read_length)
{
    FakeFile *file = (FakeFile *)handle;
    size_t available = file->position < file->length
                           ? file->length - file->position
                           : 0U;
    size_t amount = available < capacity ? available : capacity;
    (void)context;
    memcpy(buffer, &file->bytes[file->position], amount);
    file->position += amount;
    *read_length = amount;
    return 0;
}

static int fake_write(void *context, void *handle, const uint8_t *buffer,
                      size_t length, size_t *written_length)
{
    FakeFile *file = (FakeFile *)handle;
    size_t end = file->position + length;
    (void)context;
    if (end > FAKE_CAPACITY)
    {
        return -1;
    }
    memcpy(&file->bytes[file->position], buffer, length);
    file->position = end;
    if (end > file->length)
    {
        file->length = end;
    }
    *written_length = length;
    return 0;
}

static int fake_seek(void *context, void *handle, uint32_t offset)
{
    FakeFile *file = (FakeFile *)handle;
    (void)context;
    file->position = offset == TELEMETRY_STORAGE_SEEK_END
                         ? file->length
                         : offset;
    return file->position <= file->length ? 0 : -1;
}

static int fake_truncate(void *context, void *handle, uint32_t length)
{
    FakeFile *file = (FakeFile *)handle;
    (void)context;
    if (length > FAKE_CAPACITY)
    {
        return -1;
    }
    file->length = length;
    if (file->position > length)
    {
        file->position = length;
    }
    return 0;
}

static int fake_flush(void *context, void *handle)
{
    (void)context;
    (void)handle;
    return 0;
}

int main(void)
{
    FakeFile fake = {0};
    TelemetryStorageFileOps ops = {
        .context = &fake,
        .open = fake_open,
        .close = fake_close,
        .read = fake_read,
        .write = fake_write,
        .seek = fake_seek,
        .truncate = fake_truncate,
        .flush = fake_flush,
    };
    TelemetryStorageFatFsFile file;
    TelemetryStorageRecord records[2] = {
        {.sequence = 1U, .sensor = {.timestamp_ms = 10U,
                                    .temperature_centi_c = -100,
                                    .humidity_centi_percent = 5000U,
                                    .pressure_pa = 100000U,
                                    .valid = 3U}},
        {.sequence = 2U, .sensor = {.timestamp_ms = 20U,
                                    .temperature_centi_c = 2500,
                                    .humidity_centi_percent = 6000U,
                                    .pressure_pa = 100100U,
                                    .valid = 3U}},
    };
    TelemetryStorageRecord loaded[2] = {0};
    size_t count = 0U;

    if (telemetry_storage_fatfs_open(&file, &ops, "telemetry.bin") != 0 ||
        telemetry_storage_fatfs_append(&file, &records[0]) != 0 ||
        telemetry_storage_fatfs_append(&file, &records[1]) != 0 ||
        telemetry_storage_fatfs_load(&file, loaded, 2U, &count) != 0 ||
        count != 2U || loaded[0].sequence != 1U || loaded[1].sequence != 2U)
    {
        fprintf(stderr, "FatFs boundary append/load failed\n");
        return 1;
    }
    fake.length -= 3U;
    if (telemetry_storage_fatfs_load(&file, loaded, 2U, &count) != 0 ||
        count != 1U || loaded[0].sequence != 1U)
    {
        fprintf(stderr, "FatFs boundary tail recovery failed\n");
        return 2;
    }
    if (telemetry_storage_fatfs_rewrite(&file, &records[1], 1U) != 0 ||
        telemetry_storage_fatfs_load(&file, loaded, 2U, &count) != 0 ||
        count != 1U || loaded[0].sequence != 2U ||
        telemetry_storage_fatfs_close(&file) != 0)
    {
        fprintf(stderr, "FatFs boundary rewrite failed\n");
        return 3;
    }
    puts("telemetry storage FatFs boundary tests passed");
    return 0;
}
