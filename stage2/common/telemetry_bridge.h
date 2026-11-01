#ifndef TELEMETRY_BRIDGE_H
#define TELEMETRY_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#include "sensor_frame.h"

int telemetry_bridge_encode_sensor_frame(const SensorFrame *sensor,
                                         uint8_t *buffer,
                                         size_t capacity,
                                         size_t *encoded_length);

#endif
