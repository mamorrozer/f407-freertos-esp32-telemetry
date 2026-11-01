idf_component_register(
    SRCS "app_main.c" "telemetry_nvs.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_event esp_netif esp_wifi esp_timer mqtt nvs_flash telemetry_protocol telemetry_text telemetry_cache telemetry_stats telemetry_delivery telemetry_config telemetry_faults
)
