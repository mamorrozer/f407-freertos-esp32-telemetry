#include "telemetry_delivery.h"

#include <stddef.h>

void telemetry_delivery_init(TelemetryDeliveryState *state)
{
    if (state != NULL)
    {
        *state = (TelemetryDeliveryState){0};
    }
}

TelemetryDeliveryBeginResult telemetry_delivery_begin(TelemetryDeliveryState *state,
                                                      int message_id,
                                                      uint32_t sequence,
                                                      uint32_t accepted_at_ms)
{
    if ((state == NULL) || (message_id < 0))
    {
        return TELEMETRY_DELIVERY_INVALID;
    }
    /* 一个状态对象只跟踪一个等待 PUBACK 的消息，保证 FIFO 头和 ACK 一一对应。 */
    if (state->in_flight)
    {
        return TELEMETRY_DELIVERY_BUSY;
    }
    state->in_flight = true;
    state->message_id = message_id;
    state->sequence = sequence;
    state->accepted_at_ms = accepted_at_ms;
    return TELEMETRY_DELIVERY_ACCEPTED;
}

TelemetryDeliveryAckResult telemetry_delivery_on_puback(
    TelemetryDeliveryState *state,
    int message_id,
    uint32_t *acknowledged_sequence)
{
    if ((state == NULL) || (acknowledged_sequence == NULL) || (message_id < 0))
    {
        return TELEMETRY_DELIVERY_ACK_INVALID;
    }
    /* message_id 不匹配时不能删除缓存；这可能是迟到 ACK 或重复事件。 */
    if (!state->in_flight || (state->message_id != message_id))
    {
        state->unmatched_pubacks++;
        return TELEMETRY_DELIVERY_ACK_UNMATCHED;
    }

    *acknowledged_sequence = state->sequence;
    /* ACK 到达后由上层按序号确认并 pop FIFO；本模块只管理投递状态。 */
    state->in_flight = false;
    state->matched_pubacks++;
    return TELEMETRY_DELIVERY_ACK_MATCHED;
}

bool telemetry_delivery_on_disconnect(TelemetryDeliveryState *state)
{
    if (!telemetry_delivery_retry(state))
    {
        return false;
    }
    state->disconnect_retries++;
    return true;
}

bool telemetry_delivery_retry(TelemetryDeliveryState *state)
{
    if ((state == NULL) || !state->in_flight)
    {
        return false;
    }
    /* 重试只释放“正在等待”的状态，不代表消息已经成功送达。 */
    state->in_flight = false;
    state->retry_requests++;
    return true;
}

bool telemetry_delivery_retry_if_expired(TelemetryDeliveryState *state,
                                         uint32_t now_ms,
                                         uint32_t timeout_ms)
{
    if ((state == NULL) || !state->in_flight || (timeout_ms == 0U))
    {
        return false;
    }
    if ((uint32_t)(now_ms - state->accepted_at_ms) < timeout_ms)
    {
        return false;
    }
    return telemetry_delivery_retry(state);
}

bool telemetry_delivery_on_deleted(TelemetryDeliveryState *state,
                                   int message_id)
{
    if ((state == NULL) || !state->in_flight ||
        (message_id < 0) || (state->message_id != message_id))
    {
        return false;
    }
    state->in_flight = false;
    state->outbox_deletions++;
    return true;
}
