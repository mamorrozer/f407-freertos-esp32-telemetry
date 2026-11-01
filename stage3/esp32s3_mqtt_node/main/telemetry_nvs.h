#ifndef TELEMETRY_NVS_H
#define TELEMETRY_NVS_H

#include "esp_err.h"

#include "telemetry_config.h"

esp_err_t telemetry_nvs_load(TelemetryConfig *config);
esp_err_t telemetry_nvs_save(const TelemetryConfig *config);

#endif
