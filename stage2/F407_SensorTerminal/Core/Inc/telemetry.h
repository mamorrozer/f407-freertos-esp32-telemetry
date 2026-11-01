#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stddef.h>
#include <stdint.h>

#define TELEMETRY_LINE_MAX_LENGTH 128U

typedef enum
{
    TELEMETRY_FORMAT_OK = 0,
    TELEMETRY_FORMAT_INVALID_ARGUMENT = -1,
    TELEMETRY_FORMAT_BUFFER_TOO_SMALL = -2,
    TELEMETRY_FORMAT_ENCODING_ERROR = -3
} TelemetryFormatResult;

typedef struct
{
    uint32_t sequence;
    uint32_t produced_tick;
} TelemetryMessage;

int telemetry_format_line(char *buffer,
                          size_t buffer_size,
                          const TelemetryMessage *message,
                          uint32_t send_ok,
                          uint32_t send_fail,
                          uint32_t queue_depth,
                          uint32_t queue_peak,
                          uint32_t stack_free);

#endif
