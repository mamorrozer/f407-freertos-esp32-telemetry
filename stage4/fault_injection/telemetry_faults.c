#include "telemetry_faults.h"

#include "telemetry_protocol.h"

void telemetry_faults_init(TelemetryFaultPlan *plan,
                           TelemetryFaultCounters *counters)
{
    if (plan != NULL)
    {
        *plan = (TelemetryFaultPlan){0};
    }
    if (counters != NULL)
    {
        *counters = (TelemetryFaultCounters){0};
    }
}

int telemetry_faults_apply(TelemetryFaultPlan *plan,
                           TelemetryFaultCounters *counters,
                           uint8_t *frame,
                           size_t length,
                           uint32_t *delay_ms)
{
    if ((plan == NULL) || (counters == NULL) ||
        ((frame == NULL) && (length != 0U)))
    {
        return -1;
    }
    if (delay_ms != NULL)
    {
        *delay_ms = plan->delay_ms;
    }
    if (plan->drop_next)
    {
        plan->drop_next = false;
        counters->dropped++;
        return 1;
    }
    if (plan->corrupt_crc_next && (length != 0U))
    {
        frame[length - 1U] ^= 0x01U;
        plan->corrupt_crc_next = false;
        counters->crc_corrupted++;
    }
    if (plan->corrupt_length_next)
    {
        if (length > TELEMETRY_FRAME_LENGTH_OFFSET)
        {
            frame[TELEMETRY_FRAME_LENGTH_OFFSET]++;
            counters->length_corrupted++;
        }
        plan->corrupt_length_next = false;
    }
    if (plan->delay_ms != 0U)
    {
        plan->delay_ms = 0U;
        counters->delayed++;
    }
    return 0;
}
