#include "telemetry_bridge.h"

#include "telemetry_protocol.h"

int telemetry_bridge_encode_sensor_frame(const SensorFrame *sensor,
                                         uint8_t *buffer,
                                         size_t capacity,
                                         size_t *encoded_length)
{
    TelemetrySensorPayload payload;

    if ((sensor == NULL) || (buffer == NULL) || (encoded_length == NULL))
    {
        return -1;
    }
    payload.timestamp_ms = sensor->timestamp_ms;
    payload.temperature_centi_c = sensor->temperature_centi_c;
    payload.humidity_centi_percent = sensor->humidity_centi_percent;
    payload.pressure_pa = sensor->pressure_pa;
    payload.valid = sensor->valid;
    return telemetry_encode_sensor(sensor->sequence, &payload, buffer,
                                   capacity, encoded_length);
}
