#include <stdio.h>
#include <string.h>

#include "bmp280.h"

typedef struct
{
    uint8_t registers[256];
    unsigned writes;
    int disconnected;
} FakeBmpBus;

static int fake_read(void *context, uint8_t reg, uint8_t *data, uint16_t length)
{
    FakeBmpBus *bus = (FakeBmpBus *)context;
    if (bus->disconnected)
    {
        return -1;
    }
    memcpy(data, &bus->registers[reg], length);
    return 0;
}

static int fake_write(void *context, uint8_t reg, const uint8_t *data,
                      uint16_t length)
{
    FakeBmpBus *bus = (FakeBmpBus *)context;
    if (bus->disconnected)
    {
        return -1;
    }
    memcpy(&bus->registers[reg], data, length);
    bus->writes++;
    return 0;
}

static void put_u16_le(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void put_i16_le(uint8_t *data, int16_t value)
{
    put_u16_le(data, (uint16_t)value);
}

static int32_t abs_i32(int32_t value)
{
    return value < 0 ? -value : value;
}

int main(void)
{
    FakeBmpBus fake = {0};
    BMP280 device;
    BMP280_Bus bus = {.read = fake_read, .write = fake_write, .context = &fake};
    BMP280_Sample sample;

    fake.registers[0xD0] = BMP280_CHIP_ID;
    put_u16_le(&fake.registers[0x88], 27504U);
    put_i16_le(&fake.registers[0x8A], 26435);
    put_i16_le(&fake.registers[0x8C], -1000);
    put_u16_le(&fake.registers[0x8E], 36477U);
    put_i16_le(&fake.registers[0x90], -10685);
    put_i16_le(&fake.registers[0x92], 3024);
    put_i16_le(&fake.registers[0x94], 2855);
    put_i16_le(&fake.registers[0x96], 140);
    put_i16_le(&fake.registers[0x98], -7);
    put_i16_le(&fake.registers[0x9A], 15500);
    put_i16_le(&fake.registers[0x9C], -14600);
    put_i16_le(&fake.registers[0x9E], 6000);

    /* Bosch datasheet compensation example: adc_P=415148, adc_T=519888. */
    fake.registers[0xF7] = 0x65;
    fake.registers[0xF8] = 0x5A;
    fake.registers[0xF9] = 0xC0;
    fake.registers[0xFA] = 0x7E;
    fake.registers[0xFB] = 0xED;
    fake.registers[0xFC] = 0x00;

    if (bmp280_init(&device, &bus, BMP280_I2C_ADDR_LOW) != 0 ||
        bmp280_read_sample(&device, &sample) != 0)
    {
        fprintf(stderr, "BMP280 transaction failed\n");
        return 1;
    }
    if (abs_i32(sample.temperature_centi_c - 2508) > 1 ||
        (sample.pressure_pa < 100640U) || (sample.pressure_pa > 100670U) ||
        fake.registers[0xF5] != 0xA0U || fake.registers[0xF4] != 0x27U)
    {
        fprintf(stderr, "unexpected BMP280 result: T=%ld P=%lu\n",
                (long)sample.temperature_centi_c,
                (unsigned long)sample.pressure_pa);
        return 2;
    }
    /* A successful I2C read after power-up is not a completed measurement.
       Reset output registers hold the skipped-conversion value 0x80000. */
    {
        const uint8_t valid_raw[6] = {0x65U, 0x5AU, 0xC0U, 0x7EU, 0xEDU, 0U};
        const uint8_t reset_raw[6] = {0x80U, 0U, 0U, 0x80U, 0U, 0U};
        sample = (BMP280_Sample){.temperature_centi_c = 123, .pressure_pa = 456U};
        memcpy(&fake.registers[0xF7], reset_raw, sizeof(reset_raw));
        if (bmp280_read_sample(&device, &sample) == 0 ||
            device.initialized != 0U || sample.temperature_centi_c != 123 ||
            sample.pressure_pa != 456U)
        {
            fprintf(stderr, "power-reset raw data was reported as valid\n");
            return 3;
        }
        memcpy(&fake.registers[0xF7], valid_raw, sizeof(valid_raw));
        if (bmp280_init(&device, &bus, BMP280_I2C_ADDR_LOW) != 0)
        {
            return 4;
        }
        /* Mode and NVM state must be checked even if raw bytes look plausible. */
        fake.registers[0xF4] = 0U;
        if (bmp280_read_sample(&device, &sample) == 0 || device.initialized != 0U)
        {
            fprintf(stderr, "sleeping sensor was reported as valid\n");
            return 5;
        }
        if (bmp280_init(&device, &bus, BMP280_I2C_ADDR_LOW) != 0)
        {
            return 6;
        }
        fake.registers[0xF3] = 1U;
        if (bmp280_read_sample(&device, &sample) == 0 || device.initialized != 0U)
        {
            fprintf(stderr, "NVM update was reported as valid\n");
            return 7;
        }
        fake.registers[0xF3] = 0U;
        if (bmp280_init(&device, &bus, BMP280_I2C_ADDR_LOW) != 0)
        {
            return 8;
        }
        fake.disconnected = 1;
        if (bmp280_read_sample(&device, &sample) == 0 || device.initialized != 0U)
        {
            fprintf(stderr, "I2C failure did not invalidate initialization\n");
            return 9;
        }
        fake.disconnected = 0;
        fake.registers[0xF4] = 0U;
        memcpy(&fake.registers[0xF7], reset_raw, sizeof(reset_raw));
        if (bmp280_init(&device, &bus, BMP280_I2C_ADDR_LOW) != 0 ||
            fake.registers[0xF4] != 0x27U)
        {
            fprintf(stderr, "reinitialization did not restore measurement mode\n");
            return 10;
        }
        memcpy(&fake.registers[0xF7], valid_raw, sizeof(valid_raw));
        if (bmp280_read_sample(&device, &sample) != 0 ||
            abs_i32(sample.temperature_centi_c - 2508) > 1 ||
            sample.pressure_pa < 100640U || sample.pressure_pa > 100670U)
        {
            fprintf(stderr, "sample did not recover after reinitialization\n");
            return 11;
        }
        fake.registers[0xF3] = 1U;
        if (bmp280_init(&device, &bus, BMP280_I2C_ADDR_LOW) == 0 ||
            device.initialized != 0U)
        {
            fprintf(stderr, "initialization accepted calibration during NVM update\n");
            return 12;
        }
        fake.registers[0xF3] = 0U;
        if (bmp280_init(&device, &bus, BMP280_I2C_ADDR_LOW) != 0 ||
            bmp280_read_sample(&device, &sample) != 0)
        {
            fprintf(stderr, "initialization did not recover after NVM update\n");
            return 13;
        }
    }
    puts("BMP280 tests passed");
    return 0;
}
