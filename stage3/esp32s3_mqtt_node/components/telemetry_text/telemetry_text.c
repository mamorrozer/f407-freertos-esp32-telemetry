#include "telemetry_text.h"

#include <stdio.h>
#include <string.h>

static bool is_separator(char value)
{
    return (value == '\0') || (value == ' ') || (value == '\t') ||
           (value == ',') || (value == '\r') || (value == '\n');
}

static bool find_field(const char *line,
                       const char *key,
                       const char **value_start,
                       const char **value_end)
{
    size_t key_length;
    const char *cursor;

    if ((line == NULL) || (key == NULL) || (value_start == NULL) ||
        (value_end == NULL))
    {
        return false;
    }
    key_length = strlen(key);
    cursor = line;
    while (*cursor != '\0')
    {
        const char *token_start;
        const char *token_end;
        while (is_separator(*cursor))
        {
            ++cursor;
        }
        if (*cursor == '\0')
        {
            break;
        }
        token_start = cursor;
        while (!is_separator(*cursor))
        {
            ++cursor;
        }
        token_end = cursor;
        if (((size_t)(token_end - token_start) > key_length) &&
            (memcmp(token_start, key, key_length) == 0))
        {
            *value_start = token_start + key_length;
            *value_end = token_end;
            return true;
        }
    }
    return false;
}

static bool parse_u32(const char *start,
                      const char *end,
                      unsigned base,
                      uint32_t maximum,
                      uint32_t *value)
{
    uint32_t parsed = 0U;
    bool found_digit = false;

    if ((start == NULL) || (end == NULL) || (value == NULL) || (start >= end))
    {
        return false;
    }
    while (start < end)
    {
        unsigned digit;
        if ((*start >= '0') && (*start <= '9'))
        {
            digit = (unsigned)(*start - '0');
        }
        else if ((base == 16U) && (*start >= 'a') && (*start <= 'f'))
        {
            digit = (unsigned)(*start - 'a') + 10U;
        }
        else if ((base == 16U) && (*start >= 'A') && (*start <= 'F'))
        {
            digit = (unsigned)(*start - 'A') + 10U;
        }
        else
        {
            return false;
        }
        if (digit >= base || parsed > (maximum - digit) / base)
        {
            return false;
        }
        parsed = parsed * base + digit;
        found_digit = true;
        ++start;
    }
    if (!found_digit)
    {
        return false;
    }
    *value = parsed;
    return true;
}

static bool parse_fixed(const char *start,
                        const char *end,
                        int32_t minimum,
                        int32_t maximum,
                        int32_t *value)
{
    bool negative = false;
    uint32_t whole = 0U;
    uint32_t fraction = 0U;
    uint32_t fraction_digits = 0U;
    uint32_t magnitude_limit;
    uint32_t scaled;

    if ((start == NULL) || (end == NULL) || (value == NULL) || (start >= end))
    {
        return false;
    }
    if (*start == '-')
    {
        negative = true;
        ++start;
    }
    if ((start >= end) || (*start < '0') || (*start > '9'))
    {
        return false;
    }
    magnitude_limit = negative ? (uint32_t)(-(int64_t)minimum)
                               : (uint32_t)maximum;
    while ((start < end) && (*start >= '0') && (*start <= '9'))
    {
        const uint32_t digit = (uint32_t)(*start - '0');
        if (whole > magnitude_limit / 100U ||
            (whole == magnitude_limit / 100U && digit > magnitude_limit % 100U))
        {
            return false;
        }
        whole = whole * 10U + digit;
        ++start;
    }
    if ((start < end) && (*start == '.'))
    {
        ++start;
        while ((start < end) && (*start >= '0') && (*start <= '9') &&
               (fraction_digits < 2U))
        {
            fraction = fraction * 10U + (uint32_t)(*start - '0');
            ++fraction_digits;
            ++start;
        }
        if (fraction_digits == 0U)
        {
            return false;
        }
        if (fraction_digits == 1U)
        {
            fraction *= 10U;
        }
    }
    if ((start != end) || (whole > magnitude_limit / 100U))
    {
        return false;
    }
    scaled = whole * 100U + fraction;
    if (scaled > magnitude_limit)
    {
        return false;
    }
    *value = negative ? -(int32_t)scaled : (int32_t)scaled;
    return (*value >= minimum) && (*value <= maximum);
}

static bool get_u32_field(const char *line,
                          const char *key,
                          unsigned base,
                          uint32_t maximum,
                          uint32_t *value)
{
    const char *start;
    const char *end;
    return find_field(line, key, &start, &end) &&
           parse_u32(start, end, base, maximum, value);
}

static bool get_fixed_field(const char *line,
                            const char *key,
                            int32_t minimum,
                            int32_t maximum,
                            int32_t *value)
{
    const char *start;
    const char *end;
    return find_field(line, key, &start, &end) &&
           parse_fixed(start, end, minimum, maximum, value);
}

bool telemetry_text_parse_sensor_line(const char *line, TelemetryFrame *frame)
{
    uint32_t sequence;
    uint32_t timestamp;
    uint32_t pressure;
    uint32_t valid;
    int32_t temperature;
    int32_t humidity;

    if ((frame == NULL) ||
        !get_u32_field(line, "seq=", 10U, UINT32_MAX, &sequence) ||
        !get_u32_field(line, "tick=", 10U, UINT32_MAX, &timestamp) ||
        !get_fixed_field(line, "T=", -4000, 8500, &temperature) ||
        !get_fixed_field(line, "H=", 0, 10000, &humidity) ||
        !get_u32_field(line, "P=", 10U, 200000U, &pressure) ||
        !get_u32_field(line, "valid=0x", 16U, 255U, &valid))
    {
        return false;
    }
    *frame = (TelemetryFrame){
        .version = TELEMETRY_FRAME_VERSION,
        .type = TELEMETRY_FRAME_TYPE_SENSOR,
        .sequence = sequence,
        .sensor = {
            .timestamp_ms = timestamp,
            .temperature_centi_c = temperature,
            .humidity_centi_percent = (uint32_t)humidity,
            .pressure_pa = pressure,
            .valid = (uint8_t)valid,
        },
    };
    return true;
}

bool telemetry_text_format_json(const TelemetryFrame *frame,
                                char *buffer,
                                size_t capacity)
{
    int written;

    if ((frame == NULL) || (buffer == NULL) || (capacity == 0U))
    {
        return false;
    }
    written = snprintf(buffer, capacity,
                       "{\"seq\":%lu,\"tick\":%lu,\"temp_centi\":%ld,"
                       "\"humidity_centi\":%lu,\"pressure_pa\":%lu,\"valid\":%u}",
                       (unsigned long)frame->sequence,
                       (unsigned long)frame->sensor.timestamp_ms,
                       (long)frame->sensor.temperature_centi_c,
                       (unsigned long)frame->sensor.humidity_centi_percent,
                       (unsigned long)frame->sensor.pressure_pa,
                       (unsigned int)frame->sensor.valid);
    return (written >= 0) && ((size_t)written < capacity);
}
