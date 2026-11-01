#include "sensor_extrema.h"

#include <string.h>

void sensor_extrema_clear(SensorExtrema *extrema)
{
    if (extrema != NULL)
    {
        memset(extrema, 0, sizeof(*extrema));
    }
}

void sensor_extrema_update(SensorExtrema *extrema, const SensorFrame *frame)
{
    if ((extrema == NULL) || (frame == NULL))
    {
        return;
    }

    if ((frame->valid & SENSOR_FRAME_VALID_AHT20) != 0U)
    {
        if (extrema->temperature_valid == 0U)
        {
            extrema->minimum_temperature_centi_c =
                frame->temperature_centi_c;
            extrema->maximum_temperature_centi_c =
                frame->temperature_centi_c;
            extrema->minimum_humidity_centi_percent =
                frame->humidity_centi_percent;
            extrema->maximum_humidity_centi_percent =
                frame->humidity_centi_percent;
            extrema->temperature_valid = 1U;
            extrema->humidity_valid = 1U;
        }
        else
        {
            if (frame->temperature_centi_c <
                extrema->minimum_temperature_centi_c)
            {
                extrema->minimum_temperature_centi_c =
                    frame->temperature_centi_c;
            }
            if (frame->temperature_centi_c >
                extrema->maximum_temperature_centi_c)
            {
                extrema->maximum_temperature_centi_c =
                    frame->temperature_centi_c;
            }
            if (frame->humidity_centi_percent <
                extrema->minimum_humidity_centi_percent)
            {
                extrema->minimum_humidity_centi_percent =
                    frame->humidity_centi_percent;
            }
            if (frame->humidity_centi_percent >
                extrema->maximum_humidity_centi_percent)
            {
                extrema->maximum_humidity_centi_percent =
                    frame->humidity_centi_percent;
            }
        }
    }

    if ((frame->valid & SENSOR_FRAME_VALID_BMP280) != 0U)
    {
        if (extrema->pressure_valid == 0U)
        {
            extrema->minimum_pressure_pa = frame->pressure_pa;
            extrema->maximum_pressure_pa = frame->pressure_pa;
            extrema->pressure_valid = 1U;
        }
        else
        {
            if (frame->pressure_pa < extrema->minimum_pressure_pa)
            {
                extrema->minimum_pressure_pa = frame->pressure_pa;
            }
            if (frame->pressure_pa > extrema->maximum_pressure_pa)
            {
                extrema->maximum_pressure_pa = frame->pressure_pa;
            }
        }
    }
}
