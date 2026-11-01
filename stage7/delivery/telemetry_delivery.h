#ifndef TELEMETRY_DELIVERY_H
#define TELEMETRY_DELIVERY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    TELEMETRY_DELIVERY_INVALID = -1,
    TELEMETRY_DELIVERY_BUSY = 0,
    TELEMETRY_DELIVERY_ACCEPTED = 1
} TelemetryDeliveryBeginResult;

typedef enum
{
    TELEMETRY_DELIVERY_ACK_INVALID = -1,
    TELEMETRY_DELIVERY_ACK_UNMATCHED = 0,
    TELEMETRY_DELIVERY_ACK_MATCHED = 1
} TelemetryDeliveryAckResult;

typedef struct
{
    bool in_flight;
    int message_id;
    uint32_t sequence;
    uint32_t accepted_at_ms;
    uint32_t matched_pubacks;
    uint32_t unmatched_pubacks;
    uint32_t retry_requests;
    uint32_t disconnect_retries;
    uint32_t outbox_deletions;
} TelemetryDeliveryState;

void telemetry_delivery_init(TelemetryDeliveryState *state);
TelemetryDeliveryBeginResult telemetry_delivery_begin(TelemetryDeliveryState *state,
                                                      int message_id,
                                                      uint32_t sequence,
                                                      uint32_t accepted_at_ms);
TelemetryDeliveryAckResult telemetry_delivery_on_puback(
    TelemetryDeliveryState *state,
    int message_id,
    uint32_t *acknowledged_sequence);
bool telemetry_delivery_on_disconnect(TelemetryDeliveryState *state);
bool telemetry_delivery_retry(TelemetryDeliveryState *state);
bool telemetry_delivery_retry_if_expired(TelemetryDeliveryState *state,
                                         uint32_t now_ms,
                                         uint32_t timeout_ms);
bool telemetry_delivery_on_deleted(TelemetryDeliveryState *state,
                                   int message_id);

#endif
