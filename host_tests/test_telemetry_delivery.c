#include <stdio.h>

#include "telemetry_delivery.h"

int main(void)
{
    TelemetryDeliveryState state;
    uint32_t acknowledged_sequence = 0U;

    telemetry_delivery_init(&state);
    if (telemetry_delivery_begin(&state, 42, 125U, 1000U) !=
        TELEMETRY_DELIVERY_ACCEPTED)
    {
        fprintf(stderr, "accepted publish was not tracked\n");
        return 1;
    }
    if (!state.in_flight || state.message_id != 42 || state.sequence != 125U)
    {
        fprintf(stderr, "in-flight identity was not retained\n");
        return 2;
    }
    if (telemetry_delivery_begin(&state, 43, 126U, 1001U) != TELEMETRY_DELIVERY_BUSY)
    {
        fprintf(stderr, "second concurrent publish should be blocked\n");
        return 3;
    }
    if (telemetry_delivery_on_puback(&state, 41, &acknowledged_sequence) !=
        TELEMETRY_DELIVERY_ACK_UNMATCHED || !state.in_flight)
    {
        fprintf(stderr, "unmatched PUBACK changed the in-flight record\n");
        return 4;
    }
    if (telemetry_delivery_on_puback(&state, 42, &acknowledged_sequence) !=
            TELEMETRY_DELIVERY_ACK_MATCHED ||
        acknowledged_sequence != 125U || state.in_flight ||
        state.matched_pubacks != 1U)
    {
        fprintf(stderr, "matching PUBACK did not confirm the expected sequence\n");
        return 5;
    }
    if (telemetry_delivery_on_puback(&state, 42, &acknowledged_sequence) !=
            TELEMETRY_DELIVERY_ACK_UNMATCHED ||
        state.matched_pubacks != 1U || state.unmatched_pubacks != 2U)
    {
        fprintf(stderr, "duplicate PUBACK was not ignored as unmatched\n");
        return 6;
    }

    if (telemetry_delivery_begin(&state, -1, 126U, 1002U) !=
            TELEMETRY_DELIVERY_INVALID || state.in_flight)
    {
        fprintf(stderr, "failed publish return value was incorrectly tracked\n");
        return 7;
    }
    if (telemetry_delivery_begin(&state, 50, 126U, 2000U) !=
        TELEMETRY_DELIVERY_ACCEPTED)
    {
        return 8;
    }
    if (!telemetry_delivery_on_disconnect(&state) || state.in_flight ||
        state.disconnect_retries != 1U || state.sequence != 126U)
    {
        fprintf(stderr, "disconnect should retain queued sequence for retry\n");
        return 9;
    }
    if (telemetry_delivery_begin(&state, 3, 126U, 2001U) !=
            TELEMETRY_DELIVERY_ACCEPTED ||
        telemetry_delivery_on_puback(&state, 3, &acknowledged_sequence) !=
            TELEMETRY_DELIVERY_ACK_MATCHED ||
        acknowledged_sequence != 126U)
    {
        fprintf(stderr, "re-published sequence was not confirmed after reconnect\n");
        return 10;
    }
    if (telemetry_delivery_on_disconnect(&state))
    {
        fprintf(stderr, "idle disconnect should not request a retry\n");
        return 11;
    }

    if (telemetry_delivery_begin(&state, 55, 128U, 3000U) !=
            TELEMETRY_DELIVERY_ACCEPTED ||
        !telemetry_delivery_retry(&state) || state.in_flight ||
        state.retry_requests != 2U || state.disconnect_retries != 1U ||
        state.sequence != 128U)
    {
        fprintf(stderr, "lost-event recovery should retain the sequence for retry\n");
        return 12;
    }

    if (telemetry_delivery_begin(&state, 60, 127U, 4000U) !=
        TELEMETRY_DELIVERY_ACCEPTED ||
        telemetry_delivery_on_deleted(&state, 59) || !state.in_flight ||
        !telemetry_delivery_on_deleted(&state, 60) || state.in_flight ||
        state.sequence != 127U || state.outbox_deletions != 1U)
    {
        fprintf(stderr, "matching outbox-expiry event should retain sequence for retry\n");
        return 13;
    }

    if (telemetry_delivery_begin(&state, 61, 129U, UINT32_MAX - 4U) !=
            TELEMETRY_DELIVERY_ACCEPTED ||
        telemetry_delivery_retry_if_expired(&state, 4U, 10U) ||
        !state.in_flight ||
        !telemetry_delivery_retry_if_expired(&state, 6U, 10U) ||
        state.in_flight || state.sequence != 129U)
    {
        fprintf(stderr, "delivery timeout should handle tick wrap and retain the FIFO sequence\n");
        return 14;
    }

    puts("telemetry delivery state tests passed");
    return 0;
}
