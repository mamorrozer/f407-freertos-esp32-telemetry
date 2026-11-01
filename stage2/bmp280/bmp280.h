#ifndef BMP280_H
#define BMP280_H

#include <stdint.h>

#define BMP280_CHIP_ID 0x58U
#define BMP280_I2C_ADDR_LOW 0x76U
#define BMP280_I2C_ADDR_HIGH 0x77U

typedef int (*bmp280_read_fn)(void *context, uint8_t register_address,
                              uint8_t *data, uint16_t length);
typedef int (*bmp280_write_fn)(void *context, uint8_t register_address,
                               const uint8_t *data, uint16_t length);

typedef struct
{
    bmp280_read_fn read;
    bmp280_write_fn write;
    void *context;
} BMP280_Bus;

typedef struct
{
    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;
    uint16_t dig_p1;
    int16_t dig_p2;
    int16_t dig_p3;
    int16_t dig_p4;
    int16_t dig_p5;
    int16_t dig_p6;
    int16_t dig_p7;
    int16_t dig_p8;
    int16_t dig_p9;
} BMP280_Calibration;

typedef struct
{
    BMP280_Bus bus;
    BMP280_Calibration calibration;
    int32_t t_fine;
    uint8_t address;
    uint8_t initialized;
} BMP280;

typedef struct
{
    int32_t temperature_centi_c;
    uint32_t pressure_pa;
} BMP280_Sample;

int bmp280_init(BMP280 *device, const BMP280_Bus *bus, uint8_t address);
int bmp280_read_sample(BMP280 *device, BMP280_Sample *sample);

#endif
