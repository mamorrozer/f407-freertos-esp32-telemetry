#include "bmp280.h"

#include <stddef.h>

#define REG_CHIP_ID 0xD0U
#define REG_CALIBRATION 0x88U
#define REG_CONFIG 0xF5U
#define REG_CTRL_MEAS 0xF4U
#define REG_STATUS 0xF3U
#define REG_DATA 0xF7U

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static int16_t read_i16_le(const uint8_t *data)
{
    return (int16_t)read_u16_le(data);
}

static int32_t compensate_temperature(BMP280 *device, int32_t adc_temperature)
{
    int32_t var1;
    int32_t var2;

    var1 = ((((adc_temperature >> 3) - ((int32_t)device->calibration.dig_t1 << 1)) *
             (int32_t)device->calibration.dig_t2) >> 11);
    var2 = (((((adc_temperature >> 4) - (int32_t)device->calibration.dig_t1) *
              ((adc_temperature >> 4) - (int32_t)device->calibration.dig_t1)) >> 12) *
            (int32_t)device->calibration.dig_t3) >> 14;
    device->t_fine = var1 + var2;
    return (device->t_fine * 5 + 128) >> 8;
}

static uint32_t compensate_pressure(const BMP280 *device, int32_t adc_pressure)
{
    int64_t var1;
    int64_t var2;
    int64_t pressure;

    var1 = (int64_t)device->t_fine - 128000;
    var2 = var1 * var1 * (int64_t)device->calibration.dig_p6;
    var2 += (var1 * (int64_t)device->calibration.dig_p5) << 17;
    var2 += ((int64_t)device->calibration.dig_p4) << 35;
    var1 = ((var1 * var1 * (int64_t)device->calibration.dig_p3) >> 8) +
           ((var1 * (int64_t)device->calibration.dig_p2) << 12);
    var1 = ((((int64_t)1 << 47) + var1) * (int64_t)device->calibration.dig_p1) >> 33;
    if (var1 == 0)
    {
        return 0U;
    }
    pressure = 1048576 - adc_pressure;
    pressure = (((pressure << 31) - var2) * 3125) / var1;
    var1 = ((int64_t)device->calibration.dig_p9 * (pressure >> 13) *
            (pressure >> 13)) >> 25;
    var2 = ((int64_t)device->calibration.dig_p8 * pressure) >> 19;
    pressure = ((pressure + var1 + var2) >> 8) +
               ((int64_t)device->calibration.dig_p7 << 4);
    return (uint32_t)((pressure + 128) >> 8);
}

int bmp280_init(BMP280 *device, const BMP280_Bus *bus, uint8_t address)
{
    uint8_t chip_id;
    uint8_t status;
    uint8_t calibration[24];
    uint8_t value;

    if ((device == NULL) || (bus == NULL) || (bus->read == NULL) ||
        (bus->write == NULL) || ((address != BMP280_I2C_ADDR_LOW) &&
                                 (address != BMP280_I2C_ADDR_HIGH)))
    {
        return -1;
    }
    *device = (BMP280){0};
    device->bus = *bus;
    device->address = address;
    if (bus->read(bus->context, REG_CHIP_ID, &chip_id, 1U) != 0 ||
        chip_id != BMP280_CHIP_ID ||
        bus->read(bus->context, REG_STATUS, &status, 1U) != 0 ||
        (status & 0x01U) != 0U ||
        bus->read(bus->context, REG_CALIBRATION, calibration,
                  sizeof(calibration)) != 0)
    {
        return -2;
    }
    device->calibration.dig_t1 = read_u16_le(&calibration[0]);
    device->calibration.dig_t2 = read_i16_le(&calibration[2]);
    device->calibration.dig_t3 = read_i16_le(&calibration[4]);
    device->calibration.dig_p1 = read_u16_le(&calibration[6]);
    device->calibration.dig_p2 = read_i16_le(&calibration[8]);
    device->calibration.dig_p3 = read_i16_le(&calibration[10]);
    device->calibration.dig_p4 = read_i16_le(&calibration[12]);
    device->calibration.dig_p5 = read_i16_le(&calibration[14]);
    device->calibration.dig_p6 = read_i16_le(&calibration[16]);
    device->calibration.dig_p7 = read_i16_le(&calibration[18]);
    device->calibration.dig_p8 = read_i16_le(&calibration[20]);
    device->calibration.dig_p9 = read_i16_le(&calibration[22]);

    value = 0xA0U;
    if (bus->write(bus->context, REG_CONFIG, &value, 1U) != 0)
    {
        return -3;
    }
    value = 0x27U;
    if (bus->write(bus->context, REG_CTRL_MEAS, &value, 1U) != 0)
    {
        return -4;
    }
    device->initialized = 1U;
    return 0;
}

int bmp280_read_sample(BMP280 *device, BMP280_Sample *sample)
{
    uint8_t status;
    uint8_t ctrl_meas;
    uint8_t data[6];
    int32_t raw_pressure;
    int32_t raw_temperature;

    if ((device == NULL) || (sample == NULL) || (device->initialized == 0U))
    {
        return -1;
    }
    if (device->bus.read(device->bus.context, REG_STATUS, &status, 1U) != 0 ||
        device->bus.read(device->bus.context, REG_CTRL_MEAS, &ctrl_meas, 1U) != 0 ||
        (status & 0x01U) != 0U || (ctrl_meas & 0x03U) == 0U ||
        device->bus.read(device->bus.context, REG_DATA, data, sizeof(data)) != 0)
    {
        device->initialized = 0U;
        return -2;
    }
    raw_pressure = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) |
                   ((int32_t)data[2] >> 4);
    raw_temperature = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) |
                      ((int32_t)data[5] >> 4);
    if ((raw_pressure == 0x80000) || (raw_temperature == 0x80000))
    {
        device->initialized = 0U;
        return -3;
    }
    sample->temperature_centi_c = compensate_temperature(device, raw_temperature);
    sample->pressure_pa = compensate_pressure(device, raw_pressure);
    return 0;
}
