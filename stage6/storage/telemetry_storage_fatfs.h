#ifndef TELEMETRY_STORAGE_FATFS_H
#define TELEMETRY_STORAGE_FATFS_H

#include <stddef.h>
#include <stdint.h>

#include "telemetry_storage.h"

#define TELEMETRY_STORAGE_SEEK_END UINT32_MAX

/*
 * This is the target-independent boundary for a future FatFs adapter. The
 * application supplies wrappers around f_open/f_read/f_write/f_lseek/
 * f_truncate/f_sync. The seek wrapper interprets
 * TELEMETRY_STORAGE_SEEK_END as the end of file. Keeping those calls injected lets the record logic be
 * tested on the host without pretending that an SD card was tested.
 */
typedef struct
{
    void *context;
    int (*open)(void *context, const char *path, void **handle);
    int (*close)(void *context, void *handle);
    int (*read)(void *context, void *handle, uint8_t *buffer,
                size_t capacity, size_t *read_length);
    int (*write)(void *context, void *handle, const uint8_t *buffer,
                 size_t length, size_t *written_length);
    int (*seek)(void *context, void *handle, uint32_t offset);
    int (*truncate)(void *context, void *handle, uint32_t length);
    int (*flush)(void *context, void *handle);
} TelemetryStorageFileOps;

typedef struct
{
    TelemetryStorageFileOps ops;
    void *handle;
} TelemetryStorageFatFsFile;

int telemetry_storage_fatfs_open(TelemetryStorageFatFsFile *file,
                                 const TelemetryStorageFileOps *ops,
                                 const char *path);
int telemetry_storage_fatfs_close(TelemetryStorageFatFsFile *file);
int telemetry_storage_fatfs_load(TelemetryStorageFatFsFile *file,
                                 TelemetryStorageRecord *records,
                                 size_t capacity, size_t *count);
int telemetry_storage_fatfs_append(TelemetryStorageFatFsFile *file,
                                   const TelemetryStorageRecord *record);
int telemetry_storage_fatfs_rewrite(TelemetryStorageFatFsFile *file,
                                    const TelemetryStorageRecord *records,
                                    size_t count);

#endif
