#include "telemetry_protocol.h"

#include <string.h>

static void put_u32_le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static uint32_t get_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

uint16_t telemetry_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFU;

    if ((data == NULL) && (length != 0U))
    {
        return 0U;
    }
    /* CRC 只负责发现传输中的比特/字节变化，不负责加密或证明数据来源。 */
    for (size_t i = 0U; i < length; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0U; bit < 8U; ++bit)
        {
            crc = (crc & 0x8000U) != 0U ? (uint16_t)((crc << 1) ^ 0x1021U)
                                        : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

int telemetry_encode_sensor(uint32_t sequence,
                            const TelemetrySensorPayload *sensor,
                            uint8_t *buffer, size_t capacity,
                            size_t *encoded_length)
{
    const size_t total_length = TELEMETRY_FRAME_TOTAL_SIZE;
    uint16_t crc;

    if ((sensor == NULL) || (buffer == NULL) || (encoded_length == NULL) ||
        (capacity < total_length))
    {
        return -1;
    }
    buffer[0] = TELEMETRY_FRAME_SYNC0;
    buffer[1] = TELEMETRY_FRAME_SYNC1;
    buffer[TELEMETRY_FRAME_VERSION_OFFSET] = TELEMETRY_FRAME_VERSION;
    buffer[TELEMETRY_FRAME_TYPE_OFFSET] = TELEMETRY_FRAME_TYPE_SENSOR;
    buffer[TELEMETRY_FRAME_LENGTH_OFFSET] = TELEMETRY_SENSOR_PAYLOAD_SIZE;
    buffer[TELEMETRY_FRAME_RESERVED_OFFSET] = 0U;
    put_u32_le(&buffer[TELEMETRY_FRAME_SEQUENCE_OFFSET], sequence);
    put_u32_le(&buffer[TELEMETRY_FRAME_TIMESTAMP_OFFSET], sensor->timestamp_ms);
    put_u32_le(&buffer[TELEMETRY_FRAME_TEMPERATURE_OFFSET],
               (uint32_t)sensor->temperature_centi_c);
    put_u32_le(&buffer[TELEMETRY_FRAME_HUMIDITY_OFFSET],
               sensor->humidity_centi_percent);
    put_u32_le(&buffer[TELEMETRY_FRAME_PRESSURE_OFFSET], sensor->pressure_pa);
    buffer[TELEMETRY_FRAME_VALID_OFFSET] = sensor->valid;
    /* 不把同步字节纳入 CRC：同步字节用于找帧，版本到 valid 才是协议内容。
     * F407 与 ESP32 必须对这段范围保持完全一致。 */
    crc = telemetry_crc16(&buffer[TELEMETRY_FRAME_CRC_INPUT_OFFSET],
                          TELEMETRY_FRAME_CRC_INPUT_SIZE);
    buffer[TELEMETRY_FRAME_CRC_OFFSET] = (uint8_t)crc;
    buffer[TELEMETRY_FRAME_CRC_OFFSET + 1U] = (uint8_t)(crc >> 8);
    *encoded_length = total_length;
    return 0;
}

static void decoder_reset(TelemetryProtocolDecoder *decoder)
{
    decoder->length = 0U;
    decoder->expected_length = 0U;
}

void telemetry_decoder_init(TelemetryProtocolDecoder *decoder)
{
    if (decoder != NULL)
    {
        *decoder = (TelemetryProtocolDecoder){0};
    }
}

void telemetry_decoder_reset_partial(TelemetryProtocolDecoder *decoder)
{
    if (decoder != NULL)
    {
        decoder_reset(decoder);
    }
}

static TelemetryProtocolResult decode_complete(TelemetryProtocolDecoder *decoder,
                                                TelemetryFrame *frame)
{
    uint16_t received_crc = (uint16_t)decoder->buffer[TELEMETRY_FRAME_CRC_OFFSET] |
                            ((uint16_t)decoder->buffer[TELEMETRY_FRAME_CRC_OFFSET + 1U] << 8);
    uint16_t calculated_crc = telemetry_crc16(
        &decoder->buffer[TELEMETRY_FRAME_CRC_INPUT_OFFSET],
        TELEMETRY_FRAME_CRC_INPUT_SIZE);

    /* 先校验完整性，再把字节解释成字段；坏帧不能进入 MQTT 链路。 */
    if (received_crc != calculated_crc)
    {
        decoder->crc_errors++;
        decoder_reset(decoder);
        return TELEMETRY_PROTOCOL_CRC_ERROR;
    }
    if ((decoder->buffer[TELEMETRY_FRAME_VERSION_OFFSET] != TELEMETRY_FRAME_VERSION) ||
        (decoder->buffer[TELEMETRY_FRAME_TYPE_OFFSET] != TELEMETRY_FRAME_TYPE_SENSOR) ||
        (decoder->buffer[TELEMETRY_FRAME_LENGTH_OFFSET] != TELEMETRY_SENSOR_PAYLOAD_SIZE) ||
        (decoder->buffer[TELEMETRY_FRAME_RESERVED_OFFSET] != 0U))
    {
        decoder->format_errors++;
        decoder_reset(decoder);
        return TELEMETRY_PROTOCOL_FORMAT_ERROR;
    }

    if (frame != NULL)
    {
        frame->version = decoder->buffer[TELEMETRY_FRAME_VERSION_OFFSET];
        frame->type = decoder->buffer[TELEMETRY_FRAME_TYPE_OFFSET];
        frame->sequence = get_u32_le(&decoder->buffer[TELEMETRY_FRAME_SEQUENCE_OFFSET]);
        frame->sensor.timestamp_ms = get_u32_le(
            &decoder->buffer[TELEMETRY_FRAME_TIMESTAMP_OFFSET]);
        frame->sensor.temperature_centi_c = (int32_t)get_u32_le(
            &decoder->buffer[TELEMETRY_FRAME_TEMPERATURE_OFFSET]);
        frame->sensor.humidity_centi_percent = get_u32_le(
            &decoder->buffer[TELEMETRY_FRAME_HUMIDITY_OFFSET]);
        frame->sensor.pressure_pa = get_u32_le(
            &decoder->buffer[TELEMETRY_FRAME_PRESSURE_OFFSET]);
        frame->sensor.valid = decoder->buffer[TELEMETRY_FRAME_VALID_OFFSET];
    }
    decoder_reset(decoder);
    return TELEMETRY_PROTOCOL_FRAME;
}

TelemetryProtocolResult telemetry_decoder_feed(TelemetryProtocolDecoder *decoder,
                                               uint8_t byte,
                                               TelemetryFrame *frame)
{
    if (decoder == NULL)
    {
        return TELEMETRY_PROTOCOL_FORMAT_ERROR;
    }
    /* 字节流可能从半帧、噪声或旧文本开始，因此解码器必须允许逐字节恢复。
     * expected_length 在收到头部后确定，避免依赖固定的 UART 分包。 */
    if (decoder->length == 0U)
    {
        if (byte == TELEMETRY_FRAME_SYNC0)
        {
            decoder->buffer[decoder->length++] = byte;
        }
        return TELEMETRY_PROTOCOL_NONE;
    }
    if ((decoder->length == 1U) && (byte != TELEMETRY_FRAME_SYNC1))
    {
        decoder->length = byte == TELEMETRY_FRAME_SYNC0 ? 1U : 0U;
        if (decoder->length == 1U)
        {
            decoder->buffer[0] = byte;
        }
        decoder->format_errors++;
        return TELEMETRY_PROTOCOL_FORMAT_ERROR;
    }
    if (decoder->length >= sizeof(decoder->buffer))
    {
        decoder->format_errors++;
        decoder_reset(decoder);
        return TELEMETRY_PROTOCOL_FORMAT_ERROR;
    }
    decoder->buffer[decoder->length++] = byte;
    if (decoder->length == TELEMETRY_FRAME_HEADER_SIZE)
    {
        if ((decoder->buffer[TELEMETRY_FRAME_VERSION_OFFSET] != TELEMETRY_FRAME_VERSION) ||
            (decoder->buffer[TELEMETRY_FRAME_TYPE_OFFSET] != TELEMETRY_FRAME_TYPE_SENSOR) ||
            (decoder->buffer[TELEMETRY_FRAME_LENGTH_OFFSET] != TELEMETRY_SENSOR_PAYLOAD_SIZE) ||
            (decoder->buffer[TELEMETRY_FRAME_RESERVED_OFFSET] != 0U))
        {
            decoder->format_errors++;
            decoder_reset(decoder);
            return TELEMETRY_PROTOCOL_FORMAT_ERROR;
        }
        decoder->expected_length = TELEMETRY_FRAME_TOTAL_SIZE;
    }
    if ((decoder->expected_length != 0U) &&
        (decoder->length == decoder->expected_length))
    {
        return decode_complete(decoder, frame);
    }
    return TELEMETRY_PROTOCOL_NONE;
}
