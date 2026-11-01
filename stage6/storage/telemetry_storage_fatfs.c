#include "telemetry_storage_fatfs.h"

#include <limits.h>
#include <string.h>

static int valid_file(const TelemetryStorageFatFsFile *file)
{
    return file != NULL && file->handle != NULL &&
           file->ops.read != NULL && file->ops.write != NULL &&
           file->ops.seek != NULL && file->ops.truncate != NULL &&
           file->ops.flush != NULL;
}

int telemetry_storage_fatfs_open(TelemetryStorageFatFsFile *file,
                                 const TelemetryStorageFileOps *ops,
                                 const char *path)
{
    void *handle = NULL;

    if ((file == NULL) || (ops == NULL) || (path == NULL) ||
        (ops->open == NULL) || (ops->close == NULL) ||
        (ops->read == NULL) || (ops->write == NULL) ||
        (ops->seek == NULL) || (ops->truncate == NULL) ||
        (ops->flush == NULL) || ops->open(ops->context, path, &handle) != 0 ||
        handle == NULL)
    {
        return -1;
    }
    file->ops = *ops;
    file->handle = handle;
    return 0;
}

int telemetry_storage_fatfs_close(TelemetryStorageFatFsFile *file)
{
    int result;

    if ((file == NULL) || (file->handle == NULL) ||
        (file->ops.close == NULL))
    {
        return -1;
    }
    result = file->ops.close(file->ops.context, file->handle);
    file->handle = NULL;
    return result;
}

int telemetry_storage_fatfs_load(TelemetryStorageFatFsFile *file,
                                 TelemetryStorageRecord *records,
                                 size_t capacity, size_t *count)
{
    uint8_t buffer[TELEMETRY_STORAGE_RECORD_SIZE];
    size_t loaded = 0U;

    if (!valid_file(file) || (records == NULL) || (count == NULL) ||
        (capacity > TELEMETRY_STORAGE_MAX_RECORDS))
    {
        return -1;
    }
    if (file->ops.seek(file->ops.context, file->handle, 0U) != 0)
    {
        return -1;
    }
    while (loaded < capacity)
    {
        size_t read_length = 0U;
        TelemetryStorageRecord record;
        if (file->ops.read(file->ops.context, file->handle, buffer,
                           sizeof(buffer), &read_length) != 0)
        {
            return -1;
        }
        if (read_length == 0U)
        {
            break;
        }
        if ((read_length != sizeof(buffer)) ||
            telemetry_storage_decode_record(buffer, read_length, &record) != 0)
        {
            break;
        }
        records[loaded++] = record;
    }
    *count = loaded;
    return 0;
}

int telemetry_storage_fatfs_append(TelemetryStorageFatFsFile *file,
                                   const TelemetryStorageRecord *record)
{
    uint8_t buffer[TELEMETRY_STORAGE_RECORD_SIZE];
    size_t written = 0U;

    if (!valid_file(file) || (record == NULL) ||
        telemetry_storage_encode_record(record, buffer, sizeof(buffer)) != 0 ||
        file->ops.seek(file->ops.context, file->handle,
                       TELEMETRY_STORAGE_SEEK_END) != 0 ||
        file->ops.write(file->ops.context, file->handle, buffer,
                        sizeof(buffer), &written) != 0 ||
        written != sizeof(buffer) ||
        file->ops.flush(file->ops.context, file->handle) != 0)
    {
        return -1;
    }
    return 0;
}

int telemetry_storage_fatfs_rewrite(TelemetryStorageFatFsFile *file,
                                    const TelemetryStorageRecord *records,
                                    size_t count)
{
    uint8_t buffer[TELEMETRY_STORAGE_RECORD_SIZE];

    if (!valid_file(file) || ((records == NULL) && (count != 0U)) ||
        (count > TELEMETRY_STORAGE_MAX_RECORDS) ||
        file->ops.truncate(file->ops.context, file->handle, 0U) != 0 ||
        file->ops.seek(file->ops.context, file->handle, 0U) != 0)
    {
        return -1;
    }
    for (size_t index = 0U; index < count; ++index)
    {
        size_t written = 0U;
        if (telemetry_storage_encode_record(&records[index], buffer,
                                            sizeof(buffer)) != 0 ||
            file->ops.write(file->ops.context, file->handle, buffer,
                            sizeof(buffer), &written) != 0 ||
            written != sizeof(buffer))
        {
            return -1;
        }
    }
    return file->ops.flush(file->ops.context, file->handle);
}
