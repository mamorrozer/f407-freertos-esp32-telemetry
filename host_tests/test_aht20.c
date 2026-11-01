#include <stdio.h>
#include <string.h>

#include "aht20.h"

typedef struct
{
    uint8_t status;
    uint8_t sample[6];
    unsigned writes;
    unsigned delays;
} FakeAhtBus;

static int fake_read(void *context, uint8_t *data, uint16_t length)
{
    FakeAhtBus *bus = (FakeAhtBus *)context;
    if (length == 1U)
    {
        data[0] = bus->status;
    }
    else if (length == sizeof(bus->sample))
    {
        memcpy(data, bus->sample, sizeof(bus->sample));
    }
    else
    {
        return -1;
    }
    return 0;
}

static int fake_write(void *context, const uint8_t *data, uint16_t length)
{
    FakeAhtBus *bus = (FakeAhtBus *)context;
    if ((length == 3U) && (data[0] == 0xBEU))
    {
        bus->status |= 0x08U;
        bus->sample[0] |= 0x08U;
    }
    bus->writes++;
    return 0;
}

static void fake_delay(void *context, uint32_t milliseconds)
{
    FakeAhtBus *bus = (FakeAhtBus *)context;
    bus->delays += milliseconds;
}

int main(void)
{
    FakeAhtBus fake = {.status = 0x00U, .sample = {0x00U, 0x80U, 0x00U,
                                                    0x06U, 0x00U, 0x00U}};
    AHT20_Bus bus = {.read = fake_read, .write = fake_write,
                     .delay_ms = fake_delay, .context = &fake};
    AHT20 device;
    AHT20_Sample sample;

    if (aht20_init(&device, &bus) != 0 || aht20_read_sample(&device, &sample) != 0)
    {
        fprintf(stderr, "AHT20 transaction failed\n");
        return 1;
    }
    if (sample.temperature_centi_c != 2500 ||
        sample.humidity_centi_percent != 5000 || fake.writes != 3U ||
        fake.delays != 130U)
    {
        fprintf(stderr, "unexpected AHT20 result: T=%ld H=%lu writes=%u delays=%u\n",
                (long)sample.temperature_centi_c,
                (unsigned long)sample.humidity_centi_percent,
                fake.writes, fake.delays);
        return 2;
    }
    sample = (AHT20_Sample){.temperature_centi_c = 123,
                            .humidity_centi_percent = 456U};
    fake.sample[0] = 0U;
    fake.status = 0U;
    if (aht20_read_sample(&device, &sample) == 0 ||
        device.initialized != 0U || sample.temperature_centi_c != 123 ||
        sample.humidity_centi_percent != 456U)
    {
        fprintf(stderr, "uncalibrated power-reset sample was reported as valid\n");
        return 3;
    }
    if (aht20_init(&device, &bus) != 0 ||
        aht20_read_sample(&device, &sample) != 0 ||
        sample.temperature_centi_c != 2500 || sample.humidity_centi_percent != 5000U)
    {
        fprintf(stderr, "AHT20 did not recover after reinitialization\n");
        return 4;
    }
    puts("AHT20 tests passed");
    return 0;
}
