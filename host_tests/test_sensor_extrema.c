#include "sensor_extrema.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    SensorExtrema extrema;
    SensorFrame frame = {
        .temperature_centi_c = 2345,
        .humidity_centi_percent = 5012U,
        .pressure_pa = 101325U,
        .valid = SENSOR_FRAME_VALID_AHT20 | SENSOR_FRAME_VALID_BMP280,
    };

    memset(&extrema, 0xA5, sizeof(extrema));
    sensor_extrema_clear(&extrema);
    assert(extrema.temperature_valid == 0U);
    assert(extrema.humidity_valid == 0U);
    assert(extrema.pressure_valid == 0U);

    sensor_extrema_update(&extrema, &frame);
    assert(extrema.temperature_valid != 0U);
    assert(extrema.minimum_temperature_centi_c == 2345);
    assert(extrema.maximum_temperature_centi_c == 2345);
    assert(extrema.minimum_humidity_centi_percent == 5012U);
    assert(extrema.maximum_pressure_pa == 101325U);

    frame.temperature_centi_c = 1900;
    frame.humidity_centi_percent = 7025U;
    frame.pressure_pa = 100000U;
    sensor_extrema_update(&extrema, &frame);
    assert(extrema.minimum_temperature_centi_c == 1900);
    assert(extrema.maximum_temperature_centi_c == 2345);
    assert(extrema.maximum_humidity_centi_percent == 7025U);
    assert(extrema.minimum_pressure_pa == 100000U);

    /* A partial frame must update only its valid sensor's statistics. */
    frame.temperature_centi_c = 2800;
    frame.humidity_centi_percent = 4000U;
    frame.pressure_pa = 90000U;
    frame.valid = SENSOR_FRAME_VALID_AHT20;
    sensor_extrema_update(&extrema, &frame);
    assert(extrema.maximum_temperature_centi_c == 2800);
    assert(extrema.minimum_humidity_centi_percent == 4000U);
    assert(extrema.minimum_pressure_pa == 100000U);

    sensor_extrema_clear(&extrema);
    assert(extrema.temperature_valid == 0U);
    assert(extrema.humidity_valid == 0U);
    assert(extrema.pressure_valid == 0U);
    sensor_extrema_update(&extrema, &frame);
    assert(extrema.minimum_temperature_centi_c == 2800);
    assert(extrema.maximum_temperature_centi_c == 2800);
    assert(extrema.pressure_valid == 0U);

    sensor_extrema_update(&extrema, NULL);
    puts("sensor_extrema_test: PASS");
    return 0;
}
