#include "aht20.h"

#include <stddef.h>

static int send_command(AHT20 *device, const uint8_t *command, uint16_t length)
{
    if ((device == NULL) || (device->bus.write == NULL))
    {
        return -1;
    }
    return device->bus.write(device->bus.context, command, length);
}

int aht20_init(AHT20 *device, const AHT20_Bus *bus)
{
    static const uint8_t status_command[] = {0x71U};
    static const uint8_t initialize_command[] = {0xBEU, 0x08U, 0x00U};
    uint8_t status;

    if ((device == NULL) || (bus == NULL) || (bus->read == NULL) ||
        (bus->write == NULL))
    {
        return -1;
    }
    *device = (AHT20){.bus = *bus};
    if (device->bus.delay_ms != NULL)
    {
        /* AHT20 requires a short power-up settling interval. */
        device->bus.delay_ms(device->bus.context, 40U);
    }
    if (send_command(device, status_command, sizeof(status_command)) != 0)
    {
        return -2;
    }
    if (device->bus.read(device->bus.context, &status, 1U) != 0)
    {
        return -3;
    }
    if ((status & 0x08U) == 0U)
    {
        if (send_command(device, initialize_command, sizeof(initialize_command)) != 0)
        {
            return -4;
        }
        if (device->bus.delay_ms != NULL)
        {
            device->bus.delay_ms(device->bus.context, 10U);
        }
    }
    device->initialized = 1U;
    return 0;
}

int aht20_read_sample(AHT20 *device, AHT20_Sample *sample)
{
    static const uint8_t trigger_command[] = {0xACU, 0x33U, 0x00U};
    uint8_t data[6];
    uint32_t raw_humidity;
    uint32_t raw_temperature;

    if ((device == NULL) || (sample == NULL) || (device->initialized == 0U))
    {
        return -1;
    }
    if (send_command(device, trigger_command, sizeof(trigger_command)) != 0)
    {
        device->initialized = 0U;
        return -2;
    }
    if (device->bus.delay_ms != NULL)
    {
        device->bus.delay_ms(device->bus.context, 80U);
    }
    if (device->bus.read(device->bus.context, data, sizeof(data)) != 0)
    {
        device->initialized = 0U;
        return -3;
    }
    if ((data[0] & 0x80U) != 0U || (data[0] & 0x08U) == 0U)
    {
        device->initialized = 0U;
        return -4;
    }
    raw_humidity = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) |
                   ((uint32_t)data[3] >> 4);
    raw_temperature = (((uint32_t)data[3] & 0x0FU) << 16) |
                      ((uint32_t)data[4] << 8) | data[5];
    /* The raw values are 20-bit. Promote before multiplying to avoid overflow. */
    sample->humidity_centi_percent = (uint32_t)(((uint64_t)raw_humidity * 10000ULL +
                                                 524287ULL) /
                                                1048575ULL);
    sample->temperature_centi_c = (int32_t)(((uint64_t)raw_temperature * 20000ULL +
                                             524287ULL) /
                                            1048575ULL) - 5000;
    return 0;
}
