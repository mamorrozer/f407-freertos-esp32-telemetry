cmake_minimum_required(VERSION 3.22)

project(rtos_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wextra -Wpedantic)

include_directories(
    ${CMAKE_SOURCE_DIR}/../stage2/common
    ${CMAKE_SOURCE_DIR}/../stage2/bmp280
    ${CMAKE_SOURCE_DIR}/../stage2/aht20
    ${CMAKE_SOURCE_DIR}/../stage2/F407_SensorTerminal/Core/Inc
    ${CMAKE_SOURCE_DIR}/../stage4/protocol
    ${CMAKE_SOURCE_DIR}/../stage4/fault_injection
    ${CMAKE_SOURCE_DIR}/../stage6/cache
    ${CMAKE_SOURCE_DIR}/../stage7/health
    ${CMAKE_SOURCE_DIR}/../stage7/stats
    ${CMAKE_SOURCE_DIR}/../stage7/config
    ${CMAKE_SOURCE_DIR}/../stage6/storage
    ${CMAKE_SOURCE_DIR}/../stage7/power
    ${CMAKE_SOURCE_DIR}/../stage7/delivery
    ${CMAKE_SOURCE_DIR}/../stage3/esp32s3_mqtt_node/components/telemetry_text/include
    ${CMAKE_SOURCE_DIR}/../stage3/esp32s3_mqtt_node/components/telemetry_protocol/include
)

add_executable(test_bmp280 test_bmp280.c ../stage2/bmp280/bmp280.c)
add_executable(test_aht20 test_aht20.c ../stage2/aht20/aht20.c)
add_executable(test_bmp280_f407 test_bmp280.c
    ../stage2/F407_SensorTerminal/Core/Src/bmp280.c)
add_executable(test_aht20_f407 test_aht20.c
    ../stage2/F407_SensorTerminal/Core/Src/aht20.c)
target_include_directories(test_bmp280_f407 BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/../stage2/F407_SensorTerminal/Core/Inc)
target_include_directories(test_aht20_f407 BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/../stage2/F407_SensorTerminal/Core/Inc)
add_executable(test_st7735 test_st7735.c
    ../stage2/F407_SensorTerminal/Core/Src/st7735.c)
add_executable(test_sensor_extrema test_sensor_extrema.c
    ../stage2/F407_SensorTerminal/Core/Src/sensor_extrema.c)
add_executable(test_telemetry_protocol
    test_telemetry_protocol.c ../stage4/protocol/telemetry_protocol.c)
add_executable(test_f407_telemetry_encoder
    test_f407_telemetry_encoder.c
    ../stage2/common/telemetry_bridge.c
    ../stage4/protocol/telemetry_protocol.c)
add_executable(test_telemetry_pipeline
    test_telemetry_pipeline.c
    ../stage2/common/telemetry_bridge.c
    ../stage4/protocol/telemetry_protocol.c
    ../stage3/esp32s3_mqtt_node/components/telemetry_text/telemetry_text.c
    ../stage7/stats/telemetry_stats.c)
add_executable(test_telemetry_cache
    test_telemetry_cache.c ../stage6/cache/telemetry_cache.c)
add_executable(test_telemetry_text
    test_telemetry_text.c
    ../stage3/esp32s3_mqtt_node/components/telemetry_text/telemetry_text.c)
add_executable(test_telemetry_faults
    test_telemetry_faults.c ../stage4/fault_injection/telemetry_faults.c)
add_executable(test_telemetry_health
    test_telemetry_health.c ../stage7/health/telemetry_health.c)
add_executable(test_telemetry_stats
    test_telemetry_stats.c ../stage7/stats/telemetry_stats.c)
add_executable(test_telemetry_config
    test_telemetry_config.c ../stage7/config/telemetry_config.c)
add_executable(test_telemetry_storage
    test_telemetry_storage.c ../stage6/storage/telemetry_storage.c
    ../stage4/protocol/telemetry_protocol.c)
add_executable(test_telemetry_storage_fatfs
    test_telemetry_storage_fatfs.c
    ../stage6/storage/telemetry_storage.c
    ../stage6/storage/telemetry_storage_fatfs.c
    ../stage4/protocol/telemetry_protocol.c)
add_executable(test_telemetry_power_policy
    test_telemetry_power_policy.c ../stage7/power/telemetry_power_policy.c)
add_executable(test_telemetry_delivery
    test_telemetry_delivery.c ../stage7/delivery/telemetry_delivery.c)

enable_testing()
add_test(NAME bmp280 COMMAND test_bmp280)
add_test(NAME aht20 COMMAND test_aht20)
add_test(NAME bmp280_f407 COMMAND test_bmp280_f407)
add_test(NAME aht20_f407 COMMAND test_aht20_f407)
add_test(NAME st7735 COMMAND test_st7735)
add_test(NAME sensor_extrema COMMAND test_sensor_extrema)
add_test(NAME telemetry_protocol COMMAND test_telemetry_protocol)
add_test(NAME f407_telemetry_encoder COMMAND test_f407_telemetry_encoder)
add_test(NAME telemetry_pipeline COMMAND test_telemetry_pipeline)
add_test(NAME telemetry_cache COMMAND test_telemetry_cache)
add_test(NAME telemetry_text COMMAND test_telemetry_text)
add_test(NAME telemetry_faults COMMAND test_telemetry_faults)
add_test(NAME telemetry_health COMMAND test_telemetry_health)
add_test(NAME telemetry_stats COMMAND test_telemetry_stats)
add_test(NAME telemetry_config COMMAND test_telemetry_config)
add_test(NAME telemetry_storage COMMAND test_telemetry_storage)
add_test(NAME telemetry_storage_fatfs COMMAND test_telemetry_storage_fatfs)
add_test(NAME telemetry_power_policy COMMAND test_telemetry_power_policy)
add_test(NAME telemetry_delivery COMMAND test_telemetry_delivery)
