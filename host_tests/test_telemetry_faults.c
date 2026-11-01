#include <stdio.h>

#include "telemetry_faults.h"
#include "telemetry_protocol.h"

int main(void)
{
    TelemetryFaultPlan plan;
    TelemetryFaultCounters counters;
    uint8_t frame[5] = {0xA5U, 0x5AU, 0x01U, 0x02U, 0x7EU};
    uint32_t delay_ms = 0U;

    telemetry_faults_init(&plan, &counters);
    if (telemetry_faults_apply(&plan, &counters, frame, sizeof(frame),
                               &delay_ms) != 0 ||
        counters.dropped != 0U) {
        fprintf(stderr, "default fault plan changed the frame\n");
        return 1;
    }

    plan.drop_next = true;
    if (telemetry_faults_apply(&plan, &counters, frame, sizeof(frame),
                               &delay_ms) != 1 ||
        counters.dropped != 1U || plan.drop_next) {
        fprintf(stderr, "drop injection failed\n");
        return 2;
    }

    plan.corrupt_crc_next = true;
    frame[4] = 0x7EU;
    if (telemetry_faults_apply(&plan, &counters, frame, sizeof(frame),
                               &delay_ms) != 0 ||
        frame[4] != 0x7FU || counters.crc_corrupted != 1U) {
        fprintf(stderr, "CRC injection failed\n");
        return 3;
    }

    plan.corrupt_length_next = true;
    frame[TELEMETRY_FRAME_LENGTH_OFFSET] = 0x02U;
    if (telemetry_faults_apply(&plan, &counters, frame, sizeof(frame),
                               &delay_ms) != 0 ||
        frame[TELEMETRY_FRAME_LENGTH_OFFSET] != 0x03U ||
        frame[3] != 0x02U || counters.length_corrupted != 1U) {
        fprintf(stderr, "length injection failed\n");
        return 4;
    }

    plan.corrupt_length_next = true;
    if (telemetry_faults_apply(&plan, &counters, frame, 4U, &delay_ms) != 0 ||
        plan.corrupt_length_next || counters.length_corrupted != 1U) {
        fprintf(stderr, "short length injection changed counters\n");
        return 5;
    }

    plan.delay_ms = 25U;
    if (telemetry_faults_apply(&plan, &counters, frame, sizeof(frame),
                               &delay_ms) != 0 ||
        delay_ms != 25U || counters.delayed != 1U) {
        fprintf(stderr, "delay injection failed\n");
        return 6;
    }

    puts("telemetry fault tests passed");
    return 0;
}
