#include <stdio.h>
#include <string.h>

#include "telemetry_text.h"

static int expect_invalid(const char *line)
{
    TelemetryFrame frame;
    return telemetry_text_parse_sensor_line(line, &frame) ? 1 : 0;
}

int main(void)
{
    const char *valid_line =
        "sensor seq=42 tick=1234 T=-0.05 H=63.42 P=100653 valid=0x03 ok=42";
    TelemetryFrame frame = {0};
    char json[160];

    if (!telemetry_text_parse_sensor_line(valid_line, &frame) ||
        frame.sequence != 42U || frame.sensor.timestamp_ms != 1234U ||
        frame.sensor.temperature_centi_c != -5 ||
        frame.sensor.humidity_centi_percent != 6342U ||
        frame.sensor.pressure_pa != 100653U || frame.sensor.valid != 3U)
    {
        fprintf(stderr, "valid text parse failed\n");
        return 1;
    }
    if (!telemetry_text_format_json(&frame, json, sizeof(json)) ||
        strstr(json, "\"temp_centi\":-5") == NULL ||
        strstr(json, "\"valid\":3") == NULL)
    {
        fprintf(stderr, "JSON formatting failed: %s\n", json);
        return 2;
    }
    if (telemetry_text_format_json(&frame, json, 8U) ||
        telemetry_text_format_json(NULL, json, sizeof(json)) ||
        telemetry_text_parse_sensor_line(valid_line, NULL) ||
        telemetry_text_parse_sensor_line(NULL, &frame))
    {
        fprintf(stderr, "invalid output arguments were accepted\n");
        return 3;
    }

    if (expect_invalid("sensor tick=1 T=25.00 H=60.00 P=100000 valid=0x03") ||
        expect_invalid("sensor seq=1 tick=1 T=25.123 H=60.00 P=100000 valid=0x03") ||
        expect_invalid("sensor seq=1 tick=1 T=25.00 H=100.01 P=100000 valid=0x03") ||
        expect_invalid("sensor seq=1 tick=1 T=25.00 H=60.00 P=200001 valid=0x03") ||
        expect_invalid("sensor seq=1 tick=1 T=25.00 H=60.00 P=100000 valid=0x0G") ||
        expect_invalid("sensor notseq=1 tick=1 T=25.00 H=60.00 P=100000 valid=0x03") ||
        expect_invalid("sensor seq=999999999999999999999999 tick=1 T=25.00 H=60.00 P=100000 valid=0x03"))
    {
        fprintf(stderr, "invalid text input was accepted\n");
        return 4;
    }
    puts("telemetry text tests passed");
    return 0;
}
