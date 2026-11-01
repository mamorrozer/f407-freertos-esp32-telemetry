#include <stdio.h>

#include "sensor_frame.h"
#include "telemetry_bridge.h"
#include "telemetry_protocol.h"

int main(void)
{
    SensorFrame sensor = {
        .sequence = 19U,
        .timestamp_ms = 2500U,
        .temperature_centi_c = -321,
        .humidity_centi_percent = 6543U,
        .pressure_pa = 100001U,
        .valid = SENSOR_FRAME_VALID_BMP280 | SENSOR_FRAME_VALID_AHT20,
    };
    uint8_t buffer[TELEMETRY_FRAME_MAX_SIZE];
    size_t length = 0U;
    TelemetryProtocolDecoder decoder;
    TelemetryFrame decoded = {0};

    if (telemetry_bridge_encode_sensor_frame(&sensor, buffer, sizeof(buffer),
                                             &length) != 0 ||
        length != TELEMETRY_FRAME_TOTAL_SIZE) {
        fprintf(stderr, "F407 bridge encoding failed\n");
        return 1;
    }
    telemetry_decoder_init(&decoder);
    for (size_t i = 0U; i < length; ++i) {
        (void)telemetry_decoder_feed(&decoder, buffer[i], &decoded);
    }
    if (decoded.sequence != sensor.sequence ||
        decoded.sensor.timestamp_ms != sensor.timestamp_ms ||
        decoded.sensor.temperature_centi_c != sensor.temperature_centi_c ||
        decoded.sensor.humidity_centi_percent != sensor.humidity_centi_percent ||
        decoded.sensor.pressure_pa != sensor.pressure_pa ||
        decoded.sensor.valid != sensor.valid || decoder.crc_errors != 0U) {
        fprintf(stderr, "F407 bridge round-trip failed\n");
        return 2;
    }
    puts("F407 telemetry encoder tests passed");
    return 0;
}
