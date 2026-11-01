#ifndef TELEMETRY_PROTOCOL_H
#define TELEMETRY_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

/* 学习点：帧格式把“找帧、校验、解码”从业务代码中分离出来。
 * 29 字节是当前版本的线协议约束，修改字段必须同步更新偏移和测试。 */
#define TELEMETRY_FRAME_SYNC0 0xA5U
#define TELEMETRY_FRAME_SYNC1 0x5AU
#define TELEMETRY_FRAME_VERSION 1U
#define TELEMETRY_FRAME_TYPE_SENSOR 1U
#define TELEMETRY_SENSOR_PAYLOAD_SIZE 17U
#define TELEMETRY_FRAME_HEADER_SIZE 6U
#define TELEMETRY_FRAME_CRC_SIZE 2U
#define TELEMETRY_FRAME_VERSION_OFFSET 2U
#define TELEMETRY_FRAME_TYPE_OFFSET 3U
#define TELEMETRY_FRAME_LENGTH_OFFSET 4U
#define TELEMETRY_FRAME_RESERVED_OFFSET 5U
#define TELEMETRY_FRAME_SEQUENCE_OFFSET 6U
#define TELEMETRY_FRAME_TIMESTAMP_OFFSET 10U
#define TELEMETRY_FRAME_TEMPERATURE_OFFSET 14U
#define TELEMETRY_FRAME_HUMIDITY_OFFSET 18U
#define TELEMETRY_FRAME_PRESSURE_OFFSET 22U
#define TELEMETRY_FRAME_VALID_OFFSET 26U
#define TELEMETRY_FRAME_CRC_OFFSET 27U
#define TELEMETRY_FRAME_CRC_INPUT_OFFSET TELEMETRY_FRAME_VERSION_OFFSET
#define TELEMETRY_FRAME_CRC_INPUT_SIZE \
    (TELEMETRY_FRAME_CRC_OFFSET - TELEMETRY_FRAME_CRC_INPUT_OFFSET)
#define TELEMETRY_FRAME_TOTAL_SIZE \
    (TELEMETRY_FRAME_HEADER_SIZE + 4U + TELEMETRY_SENSOR_PAYLOAD_SIZE + \
     TELEMETRY_FRAME_CRC_SIZE)
#define TELEMETRY_FRAME_MAX_SIZE 32U

_Static_assert(TELEMETRY_FRAME_TOTAL_SIZE == 29U,
               "telemetry frame layout must remain 29 bytes");
_Static_assert(TELEMETRY_FRAME_CRC_INPUT_SIZE == 25U,
               "telemetry CRC range must cover version through valid");

typedef struct
{
    /* 温度/湿度使用百分之一单位，避免跨 MCU 传输 float 的布局差异。 */
    uint32_t timestamp_ms;
    int32_t temperature_centi_c;
    uint32_t humidity_centi_percent;
    uint32_t pressure_pa;
    uint8_t valid;
} TelemetrySensorPayload;

typedef struct
{
    uint8_t version;
    uint8_t type;
    uint32_t sequence;
    TelemetrySensorPayload sensor;
} TelemetryFrame;

typedef struct
{
    uint8_t buffer[TELEMETRY_FRAME_MAX_SIZE];
    size_t length;
    size_t expected_length;
    uint32_t crc_errors;
    uint32_t format_errors;
} TelemetryProtocolDecoder;

typedef enum
{
    TELEMETRY_PROTOCOL_NONE = 0,
    TELEMETRY_PROTOCOL_FRAME = 1,
    TELEMETRY_PROTOCOL_CRC_ERROR = -1,
    TELEMETRY_PROTOCOL_FORMAT_ERROR = -2
} TelemetryProtocolResult;

uint16_t telemetry_crc16(const uint8_t *data, size_t length);
int telemetry_encode_sensor(uint32_t sequence,
                            const TelemetrySensorPayload *sensor,
                            uint8_t *buffer, size_t capacity,
                            size_t *encoded_length);
void telemetry_decoder_init(TelemetryProtocolDecoder *decoder);
void telemetry_decoder_reset_partial(TelemetryProtocolDecoder *decoder);
TelemetryProtocolResult telemetry_decoder_feed(TelemetryProtocolDecoder *decoder,
                                               uint8_t byte,
                                               TelemetryFrame *frame);

#endif
