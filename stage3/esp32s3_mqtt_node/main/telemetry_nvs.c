#include "telemetry_nvs.h"

#include "nvs.h"

#include <string.h>

#define TELEMETRY_NVS_NAMESPACE "telemetry"
#define TELEMETRY_NVS_KEY "config"

esp_err_t telemetry_nvs_load(TelemetryConfig *config)
{
    nvs_handle_t handle;
    TelemetryConfig stored;
    size_t length = sizeof(stored);
    esp_err_t result;

    if (config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    result = nvs_open(TELEMETRY_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (result != ESP_OK)
    {
        return result;
    }
    result = nvs_get_blob(handle, TELEMETRY_NVS_KEY, &stored, &length);
    nvs_close(handle);
    if ((result != ESP_OK) || (length != sizeof(stored)) ||
        (telemetry_config_validate(&stored) != TELEMETRY_CONFIG_OK))
    {
        return result == ESP_OK ? ESP_ERR_INVALID_STATE : result;
    }
    *config = stored;
    return ESP_OK;
}

esp_err_t telemetry_nvs_save(const TelemetryConfig *config)
{
    nvs_handle_t handle;
    esp_err_t result;

    if ((config == NULL) ||
        (telemetry_config_validate(config) != TELEMETRY_CONFIG_OK))
    {
        return ESP_ERR_INVALID_ARG;
    }
    result = nvs_open(TELEMETRY_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (result != ESP_OK)
    {
        return result;
    }
    result = nvs_set_blob(handle, TELEMETRY_NVS_KEY, config, sizeof(*config));
    if (result == ESP_OK)
    {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    return result;
}
