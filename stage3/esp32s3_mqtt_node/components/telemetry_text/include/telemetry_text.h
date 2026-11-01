#ifndef TELEMETRY_TEXT_H
#define TELEMETRY_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "telemetry_protocol.h"

bool telemetry_text_parse_sensor_line(const char *line,
                                      TelemetryFrame *frame);
bool telemetry_text_format_json(const TelemetryFrame *frame,
                                char *buffer,
                                size_t capacity);

#endif
