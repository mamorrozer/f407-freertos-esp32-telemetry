#include <stdio.h>
#include <string.h>

#include "sensor_frame.h"
#include "telemetry_bridge.h"
#include "telemetry_protocol.h"
#include "telemetry_stats.h"
#include "telemetry_text.h"

static TelemetryProtocolResult feed_bytes(TelemetryProtocolDecoder *decoder,
                                          const uint8_t *bytes,
                                          size_t length,
                                          TelemetryFrame *frame)
{
    TelemetryProtocolResult last_result = TELEMETRY_PROTOCOL_NONE;
    for (size_t i = 0U; i < length; ++i)
    {
        TelemetryProtocolResult result = telemetry_decoder_feed(decoder,
                                                                 bytes[i], frame);
        if (result != TELEMETRY_PROTOCOL_NONE)
        {
            last_result = result;
        }
    }
    return last_result;
}

static int check_sensor_frame(uint32_t sequence, uint32_t tick)
{
    SensorFrame source = {
        .sequence = sequence,
        .timestamp_ms = tick,
        .temperature_centi_c = -125,
        .humidity_centi_percent = 6342U,
        .pressure_pa = 100653U,
        .valid = SENSOR_FRAME_VALID_BMP280 | SENSOR_FRAME_VALID_AHT20,
    };
    uint8_t encoded[TELEMETRY_FRAME_MAX_SIZE];
    size_t encoded_length = 0U;
    TelemetryProtocolDecoder decoder;
    TelemetryFrame decoded = {0};
    char json[192];

    if (telemetry_bridge_encode_sensor_frame(&source, encoded,
                                              sizeof(encoded), &encoded_length) != 0 ||
        encoded_length != TELEMETRY_FRAME_TOTAL_SIZE)
    {
        fprintf(stderr, "F407 bridge failed to encode sequence %lu\n",
                (unsigned long)sequence);
        return 1;
    }

    telemetry_decoder_init(&decoder);
    /* Split input in two chunks, as a UART driver may deliver partial data. */
    if (feed_bytes(&decoder, encoded, 7U, &decoded) != TELEMETRY_PROTOCOL_NONE ||
        feed_bytes(&decoder, &encoded[7], encoded_length - 7U, &decoded) !=
            TELEMETRY_PROTOCOL_FRAME)
    {
        fprintf(stderr, "ESP32 decoder failed to assemble sequence %lu\n",
                (unsigned long)sequence);
        return 2;
    }

    if (decoded.sequence != source.sequence ||
        decoded.sensor.timestamp_ms != source.timestamp_ms ||
        decoded.sensor.temperature_centi_c != source.temperature_centi_c ||
        decoded.sensor.humidity_centi_percent != source.humidity_centi_percent ||
        decoded.sensor.pressure_pa != source.pressure_pa ||
        decoded.sensor.valid != source.valid)
    {
        fprintf(stderr, "sensor fields changed across UART frame sequence %lu\n",
                (unsigned long)sequence);
        return 3;
    }

    if (!telemetry_text_format_json(&decoded, json, sizeof(json)) ||
        strstr(json, "\"seq\":") == NULL ||
        strstr(json, "\"temp_centi\":-125") == NULL ||
        strstr(json, "\"humidity_centi\":6342") == NULL ||
        strstr(json, "\"pressure_pa\":100653") == NULL ||
        strstr(json, "\"valid\":3") == NULL)
    {
        fprintf(stderr, "JSON output did not preserve decoded sensor fields\n");
        return 4;
    }
    return 0;
}

int main(void)
{
    TelemetrySensorPayload payload = {
        .timestamp_ms = 100U,
        .temperature_centi_c = 2500,
        .humidity_centi_percent = 6000U,
        .pressure_pa = 100000U,
        .valid = 3U,
    };
    TelemetryProtocolDecoder decoder;
    TelemetryRuntimeStats stats;
    TelemetryFrame frame = {0};
    uint8_t encoded[TELEMETRY_FRAME_MAX_SIZE];
    size_t encoded_length = 0U;
    const uint8_t noise[] = {0x11U, 0x22U};

    if (check_sensor_frame(46U, 1000U) != 0 ||
        check_sensor_frame(47U, 2000U) != 0 ||
        check_sensor_frame(51U, 3000U) != 0)
    {
        return 1;
    }

    telemetry_stats_init(&stats);
    telemetry_decoder_init(&decoder);
    (void)feed_bytes(&decoder, noise, sizeof(noise), &frame);
    if (decoder.format_errors != 0U || decoder.length != 0U)
    {
        fprintf(stderr, "pre-sync noise should be ignored without parser state\n");
        return 2;
    }

    for (uint32_t sequence = 46U; sequence <= 51U; ++sequence)
    {
        if ((sequence == 48U) || (sequence == 49U) || (sequence == 50U))
        {
            continue;
        }
        payload.timestamp_ms = sequence * 100U;
        if (telemetry_encode_sensor(sequence, &payload, encoded,
                                    sizeof(encoded), &encoded_length) != 0)
        {
            return 3;
        }
        if (sequence == 47U)
        {
            encoded[TELEMETRY_FRAME_CRC_OFFSET] ^= 0x01U;
            if (feed_bytes(&decoder, encoded, encoded_length, &frame) !=
                TELEMETRY_PROTOCOL_CRC_ERROR)
            {
                fprintf(stderr, "corrupted frame was not rejected as CRC error\n");
                return 4;
            }
            /* Retry the valid sequence to model an independently received frame. */
            if (telemetry_encode_sensor(sequence, &payload, encoded,
                                        sizeof(encoded), &encoded_length) != 0)
            {
                return 5;
            }
        }
        if (feed_bytes(&decoder, encoded, encoded_length, &frame) !=
            TELEMETRY_PROTOCOL_FRAME)
        {
            fprintf(stderr, "decoder did not recover at sequence %lu\n",
                    (unsigned long)sequence);
            return 6;
        }
        telemetry_stats_observe_sequence(&stats, frame.sequence);
    }

    if (stats.frames_received != 3U || stats.sequence_gaps != 3U ||
        stats.last_sequence != 51U || decoder.crc_errors != 1U)
    {
        fprintf(stderr, "end-to-end receive and sequence accounting mismatch\n");
        return 7;
    }
    puts("telemetry pipeline integration tests passed");
    return 0;
}
