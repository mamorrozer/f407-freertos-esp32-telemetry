#include "telemetry_config.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *text)
{
    char *end;

    while (isspace((unsigned char)*text))
    {
        ++text;
    }
    end = text + strlen(text);
    while ((end > text) && isspace((unsigned char)end[-1]))
    {
        *--end = '\0';
    }
    return text;
}

static int parse_u32(const char *text, uint32_t *value)
{
    char *end;
    unsigned long parsed;

    if ((text == NULL) || (value == NULL) || (*text == '\0'))
    {
        return -1;
    }
    errno = 0;
    parsed = strtoul(text, &end, 10);
    end = trim(end);
    if ((errno != 0) || (*end != '\0') || (parsed > UINT32_MAX))
    {
        return -1;
    }
    *value = (uint32_t)parsed;
    return 0;
}

static int parse_i32(const char *text, int32_t *value)
{
    char *end;
    long parsed;

    if ((text == NULL) || (value == NULL) || (*text == '\0'))
    {
        return -1;
    }
    errno = 0;
    parsed = strtol(text, &end, 10);
    end = trim(end);
    if ((errno != 0) || (*end != '\0') ||
        (parsed < INT32_MIN) || (parsed > INT32_MAX))
    {
        return -1;
    }
    *value = (int32_t)parsed;
    return 0;
}

void telemetry_config_defaults(TelemetryConfig *config)
{
    if (config != NULL)
    {
        *config = (TelemetryConfig){
            .version = TELEMETRY_CONFIG_VERSION,
            .sample_period_ms = 1000U,
            .temperature_high_centi_c = 3500,
            .humidity_high_centi_percent = 8000U,
            .alarm_enabled = false,
        };
    }
}

TelemetryConfigResult telemetry_config_validate(const TelemetryConfig *config)
{
    if (config == NULL)
    {
        return TELEMETRY_CONFIG_INVALID_ARGUMENT;
    }
    if ((config->version != TELEMETRY_CONFIG_VERSION) ||
        (config->sample_period_ms < TELEMETRY_SAMPLE_PERIOD_MIN_MS) ||
        (config->sample_period_ms > TELEMETRY_SAMPLE_PERIOD_MAX_MS) ||
        (config->temperature_high_centi_c < TELEMETRY_TEMP_HIGH_MIN_CENTI_C) ||
        (config->temperature_high_centi_c > TELEMETRY_TEMP_HIGH_MAX_CENTI_C) ||
        (config->humidity_high_centi_percent >
         TELEMETRY_HUMIDITY_HIGH_MAX_CENTI_PERCENT))
    {
        return TELEMETRY_CONFIG_INVALID_VALUE;
    }
    return TELEMETRY_CONFIG_OK;
}

TelemetryConfigResult telemetry_config_parse_command(const char *command,
                                                      TelemetryConfig *config)
{
    char buffer[192];
    char *token;
    char *cursor;
    TelemetryConfig next;
    bool found = false;

    if ((command == NULL) || (config == NULL))
    {
        return TELEMETRY_CONFIG_INVALID_ARGUMENT;
    }
    if ((strlen(command) >= sizeof(buffer)) ||
        (strncmp(command, "CFG", 3U) != 0))
    {
        return TELEMETRY_CONFIG_INVALID_VALUE;
    }
    next = *config;
    if (telemetry_config_validate(&next) != TELEMETRY_CONFIG_OK)
    {
        return TELEMETRY_CONFIG_INVALID_VALUE;
    }
    (void)strcpy(buffer, command + 3U);
    cursor = trim(buffer);
    if (*cursor == '\0')
    {
        return TELEMETRY_CONFIG_EMPTY;
    }
    token = strtok(cursor, ";");
    while (token != NULL)
    {
        char *pair = trim(token);
        char *separator = strchr(pair, '=');
        char *key;
        char *value;
        uint32_t unsigned_value;
        int32_t signed_value;

        if (separator == NULL)
        {
            return TELEMETRY_CONFIG_INVALID_VALUE;
        }
        *separator = '\0';
        key = trim(pair);
        value = trim(separator + 1U);
        if ((*key == '\0') || (*value == '\0'))
        {
            return TELEMETRY_CONFIG_INVALID_VALUE;
        }
        found = true;
        if (strcmp(key, "period_ms") == 0)
        {
            if (parse_u32(value, &unsigned_value) != 0)
            {
                return TELEMETRY_CONFIG_INVALID_VALUE;
            }
            next.sample_period_ms = unsigned_value;
        }
        else if (strcmp(key, "temp_high") == 0)
        {
            if (parse_i32(value, &signed_value) != 0)
            {
                return TELEMETRY_CONFIG_INVALID_VALUE;
            }
            next.temperature_high_centi_c = signed_value;
        }
        else if (strcmp(key, "humidity_high") == 0)
        {
            if (parse_u32(value, &unsigned_value) != 0)
            {
                return TELEMETRY_CONFIG_INVALID_VALUE;
            }
            next.humidity_high_centi_percent = unsigned_value;
        }
        else if (strcmp(key, "alarm") == 0)
        {
            if (parse_u32(value, &unsigned_value) != 0 ||
                unsigned_value > 1U)
            {
                return TELEMETRY_CONFIG_INVALID_VALUE;
            }
            next.alarm_enabled = unsigned_value != 0U;
        }
        else
        {
            return TELEMETRY_CONFIG_UNKNOWN_KEY;
        }
        token = strtok(NULL, ";");
    }
    if (!found || (telemetry_config_validate(&next) != TELEMETRY_CONFIG_OK))
    {
        return TELEMETRY_CONFIG_INVALID_VALUE;
    }
    *config = next;
    return TELEMETRY_CONFIG_OK;
}
