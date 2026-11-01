#include <stdio.h>

#include "telemetry_protocol.h"

static int feed_bytes(const uint8_t *bytes, size_t length,
                      TelemetryProtocolDecoder *decoder, TelemetryFrame *frame)
{
    int frames = 0;
    for (size_t i = 0U; i < length; ++i)
    {
        if (telemetry_decoder_feed(decoder, bytes[i], frame) ==
            TELEMETRY_PROTOCOL_FRAME)
        {
            frames++;
        }
    }
    return frames;
}

static uint32_t next_noise(uint32_t *state)
{
    *state = (*state * 1664525U) + 1013904223U;
    return *state;
}

int main(void)
{
    TelemetrySensorPayload input = {
        .timestamp_ms = 1234U,
        .temperature_centi_c = -125,
        .humidity_centi_percent = 6342U,
        .pressure_pa = 100653U,
        .valid = 3U,
    };
    TelemetryFrame frame = {0};
    TelemetryProtocolDecoder decoder;
    uint8_t encoded[TELEMETRY_FRAME_MAX_SIZE];
    size_t length = 0U;

    if (telemetry_crc16(NULL, 1U) != 0U || telemetry_crc16(NULL, 0U) != 0xFFFFU)
    {
        fprintf(stderr, "CRC invalid argument handling failed\n");
        return 1;
    }

    if (telemetry_encode_sensor(42U, &input, encoded,
                                TELEMETRY_FRAME_TOTAL_SIZE - 1U, &length) == 0 ||
        telemetry_encode_sensor(42U, NULL, encoded, sizeof(encoded), &length) == 0 ||
        telemetry_encode_sensor(42U, &input, encoded, sizeof(encoded), NULL) == 0)
    {
        fprintf(stderr, "invalid encoder arguments were accepted\n");
        return 2;
    }
    if (telemetry_encode_sensor(42U, &input, encoded, sizeof(encoded), &length) != 0 ||
        length != TELEMETRY_FRAME_TOTAL_SIZE)
    {
        return 3;
    }

    telemetry_decoder_init(&decoder);
    if (feed_bytes(encoded, length, &decoder, &frame) != 1 ||
        frame.sequence != 42U ||
        frame.sensor.timestamp_ms != input.timestamp_ms ||
        frame.sensor.temperature_centi_c != input.temperature_centi_c ||
        frame.sensor.humidity_centi_percent != input.humidity_centi_percent ||
        frame.sensor.pressure_pa != input.pressure_pa ||
        frame.sensor.valid != input.valid || decoder.crc_errors != 0U)
    {
        fprintf(stderr, "valid frame round-trip failed\n");
        return 4;
    }

    telemetry_decoder_init(&decoder);
    if (feed_bytes(encoded, length, &decoder, NULL) != 1 ||
        decoder.crc_errors != 0U || decoder.format_errors != 0U)
    {
        fprintf(stderr, "NULL output frame handling failed\n");
        return 5;
    }

    encoded[10] ^= 0x01U;
    telemetry_decoder_init(&decoder);
    (void)feed_bytes(encoded, length, &decoder, &frame);
    if (decoder.crc_errors != 1U)
    {
        fprintf(stderr, "CRC error was not detected\n");
        return 6;
    }

    if (telemetry_encode_sensor(7U, &input, encoded, sizeof(encoded), &length) != 0)
    {
        return 7;
    }
    encoded[TELEMETRY_FRAME_LENGTH_OFFSET] = 1U;
    telemetry_decoder_init(&decoder);
    (void)feed_bytes(encoded, length, &decoder, &frame);
    if (decoder.format_errors != 1U)
    {
        fprintf(stderr, "malformed header was not rejected\n");
        return 8;
    }

    if (telemetry_encode_sensor(7U, &input, encoded, sizeof(encoded), &length) != 0)
    {
        return 9;
    }
    telemetry_decoder_init(&decoder);
    if (feed_bytes(encoded, 9U, &decoder, &frame) != 0 ||
        feed_bytes(&encoded[9], length - 9U, &decoder, &frame) != 1)
    {
        fprintf(stderr, "split-frame parsing failed\n");
        return 10;
    }

    telemetry_decoder_init(&decoder);
    {
        const uint8_t noise[] = {0x01U, 0x02U, 0xA5U, 0x00U};
        (void)feed_bytes(noise, sizeof(noise), &decoder, &frame);
    }
    if (decoder.format_errors == 0U)
    {
        fprintf(stderr, "noise was not counted\n");
        return 11;
    }

    if (telemetry_encode_sensor(8U, &input, encoded, sizeof(encoded), &length) != 0)
    {
        return 12;
    }
    encoded[TELEMETRY_FRAME_TYPE_OFFSET] = 2U;
    telemetry_decoder_init(&decoder);
    (void)feed_bytes(encoded, length, &decoder, &frame);
    if (decoder.format_errors != 1U || decoder.crc_errors != 0U)
    {
        fprintf(stderr, "wrong type was not rejected as format error\n");
        return 13;
    }

    if (telemetry_encode_sensor(8U, &input, encoded, sizeof(encoded), &length) != 0)
    {
        return 14;
    }
    telemetry_decoder_init(&decoder);
    (void)feed_bytes(encoded, 6U, &decoder, &frame);
    if (decoder.length != TELEMETRY_FRAME_HEADER_SIZE)
    {
        fprintf(stderr, "truncated frame state was not retained\n");
        return 15;
    }

    if (telemetry_encode_sensor(9U, &input, encoded, sizeof(encoded), &length) != 0)
    {
        return 16;
    }
    telemetry_decoder_init(&decoder);
    encoded[TELEMETRY_FRAME_CRC_OFFSET] ^= 0x01U;
    if (feed_bytes(encoded, length, &decoder, &frame) != 0 ||
        decoder.crc_errors != 1U || decoder.length != 0U)
    {
        fprintf(stderr, "CRC error reset failed\n");
        return 17;
    }
    if (telemetry_encode_sensor(10U, &input, encoded, sizeof(encoded), &length) != 0 ||
        feed_bytes(encoded, length, &decoder, &frame) != 1 ||
        frame.sequence != 10U)
    {
        fprintf(stderr, "decoder did not recover after CRC error\n");
        return 18;
    }

    if (telemetry_encode_sensor(8U, &input, encoded, sizeof(encoded), &length) != 0)
    {
        return 19;
    }
    encoded[TELEMETRY_FRAME_VERSION_OFFSET] = 2U;
    {
        uint16_t crc = telemetry_crc16(&encoded[TELEMETRY_FRAME_CRC_INPUT_OFFSET],
                                       TELEMETRY_FRAME_CRC_INPUT_SIZE);
        encoded[TELEMETRY_FRAME_CRC_OFFSET] = (uint8_t)crc;
        encoded[TELEMETRY_FRAME_CRC_OFFSET + 1U] = (uint8_t)(crc >> 8);
    }
    telemetry_decoder_init(&decoder);
    (void)feed_bytes(encoded, length, &decoder, &frame);
    if (decoder.format_errors != 1U || decoder.crc_errors != 0U)
    {
        fprintf(stderr, "wrong version was not rejected as format error\n");
        return 20;
    }

    if (telemetry_encode_sensor(9U, &input, encoded, sizeof(encoded), &length) != 0)
    {
        return 21;
    }
    telemetry_decoder_init(&decoder);
    if (feed_bytes(encoded, length, &decoder, &frame) != 1)
    {
        fprintf(stderr, "decoder did not recover after a format error\n");
        return 22;
    }

    if (telemetry_encode_sensor(11U, &input, encoded, sizeof(encoded), &length) != 0)
    {
        return 23;
    }
    telemetry_decoder_init(&decoder);
    if (feed_bytes(encoded, 9U, &decoder, &frame) != 0 || decoder.length != 9U)
    {
        fprintf(stderr, "partial frame setup failed\n");
        return 24;
    }
    telemetry_decoder_reset_partial(&decoder);
    if (decoder.length != 0U || decoder.expected_length != 0U ||
        feed_bytes(encoded, length, &decoder, &frame) != 1 ||
        frame.sequence != 11U)
    {
        fprintf(stderr, "decoder did not recover after partial-frame reset\n");
        return 25;
    }

    /* Deterministic corruption sweep: every damaged frame must terminate,
       and the next valid frame must still be decoded. */
    telemetry_decoder_init(&decoder);
    for (uint32_t iteration = 0U; iteration < 128U; ++iteration)
    {
        uint32_t noise_state = 0x13579BDFU + iteration;
        uint8_t damaged[TELEMETRY_FRAME_MAX_SIZE];
        TelemetryFrame recovered = {0};
        size_t damaged_length = 0U;

        if (telemetry_encode_sensor(1000U + iteration, &input, damaged,
                                    sizeof(damaged), &damaged_length) != 0)
        {
            fprintf(stderr, "corruption sweep encode failed\n");
            return 26;
        }
        for (size_t noise_index = 0U; noise_index < 24U; ++noise_index)
        {
            (void)telemetry_decoder_feed(&decoder,
                                         (uint8_t)next_noise(&noise_state),
                                         &recovered);
        }
        damaged[TELEMETRY_FRAME_SEQUENCE_OFFSET + (iteration % 4U)] ^= 0x5AU;
        (void)feed_bytes(damaged, damaged_length, &decoder, &recovered);
        if (telemetry_encode_sensor(2000U + iteration, &input, encoded,
                                    sizeof(encoded), &length) != 0 ||
            feed_bytes(encoded, length, &decoder, &recovered) != 1 ||
            recovered.sequence != 2000U + iteration)
        {
            fprintf(stderr, "decoder failed to recover at iteration %lu\n",
                    (unsigned long)iteration);
            return 27;
        }
    }

    puts("telemetry protocol tests passed");
    return 0;
}
