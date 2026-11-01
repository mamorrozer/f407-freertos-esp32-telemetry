#include "telemetry.h"

#include <stdio.h>

int telemetry_format_line(char *buffer,
                          size_t buffer_size,
                          const TelemetryMessage *message,
                          uint32_t send_ok,
                          uint32_t send_fail,
                          uint32_t queue_depth,
                          uint32_t queue_peak,
                          uint32_t stack_free)
{
    int written;

    if ((buffer == NULL) || (message == NULL) || (buffer_size == 0U))
    {
        return TELEMETRY_FORMAT_INVALID_ARGUMENT;
    }

    written = snprintf(buffer,
                       buffer_size,
                       "telemetry seq=%lu tick=%lu send_ok=%lu send_fail=%lu queue=%lu queue_peak=%lu stack_free=%lu\r\n",
                       (unsigned long)message->sequence,
                       (unsigned long)message->produced_tick,
                       (unsigned long)send_ok,
                       (unsigned long)send_fail,
                       (unsigned long)queue_depth,
                       (unsigned long)queue_peak,
                       (unsigned long)stack_free);
    if (written < 0)
    {
        return TELEMETRY_FORMAT_ENCODING_ERROR;
    }
    if ((size_t)written >= buffer_size)
    {
        return TELEMETRY_FORMAT_BUFFER_TOO_SMALL;
    }

    return TELEMETRY_FORMAT_OK;
}
