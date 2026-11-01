#ifndef SENSOR_FRAME_H
#define SENSOR_FRAME_H

#include <stdint.h>

/* Fixed-point values keep the MCU-to-MCU contract independent of float ABI. */
typedef struct
{
    uint32_t sequence;
    uint32_t timestamp_ms;
    int32_t temperature_centi_c;
    uint32_t humidity_centi_percent;
    uint32_t pressure_pa;
    uint8_t valid;
} SensorFrame;

#define SENSOR_FRAME_VALID_BMP280 0x01U
#define SENSOR_FRAME_VALID_AHT20  0x02U

#endif
