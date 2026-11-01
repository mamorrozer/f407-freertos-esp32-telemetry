#ifndef SENSOR_EXTREMA_H
#define SENSOR_EXTREMA_H

#include <stdint.h>

#include "sensor_frame.h"

typedef struct
{
    int32_t minimum_temperature_centi_c;
    int32_t maximum_temperature_centi_c;
    uint32_t minimum_humidity_centi_percent;
    uint32_t maximum_humidity_centi_percent;
    uint32_t minimum_pressure_pa;
    uint32_t maximum_pressure_pa;
    uint8_t temperature_valid;
    uint8_t humidity_valid;
    uint8_t pressure_valid;
} SensorExtrema;

void sensor_extrema_clear(SensorExtrema *extrema);
void sensor_extrema_update(SensorExtrema *extrema,
                           const SensorFrame *frame);

#endif
