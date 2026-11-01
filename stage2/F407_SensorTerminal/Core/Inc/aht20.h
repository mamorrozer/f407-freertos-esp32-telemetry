#ifndef AHT20_H
#define AHT20_H

#include <stdint.h>

#define AHT20_I2C_ADDRESS 0x38U

typedef int (*aht20_read_fn)(void *context, uint8_t *data, uint16_t length);
typedef int (*aht20_write_fn)(void *context, const uint8_t *data,
                             uint16_t length);
typedef void (*aht20_delay_fn)(void *context, uint32_t milliseconds);

typedef struct
{
    aht20_read_fn read;
    aht20_write_fn write;
    aht20_delay_fn delay_ms;
    void *context;
} AHT20_Bus;

typedef struct
{
    AHT20_Bus bus;
    uint8_t initialized;
} AHT20;

typedef struct
{
    int32_t temperature_centi_c;
    uint32_t humidity_centi_percent;
} AHT20_Sample;

int aht20_init(AHT20 *device, const AHT20_Bus *bus);
int aht20_read_sample(AHT20 *device, AHT20_Sample *sample);

#endif
