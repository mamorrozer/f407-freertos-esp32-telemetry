# Verification record

This document records test outcomes previously observed on the project hardware. It is not a claim that every build option or failure mode has been exhaustively tested.

## Recorded hardware setup

- F407 sensor terminal with BMP280, AHT20, ST7735S TFT, and four keys.
- ESP32-S3 N16R8 connected to the F407 UART and a Wi-Fi access point.
- MQTTX subscribed to the configured telemetry topic.

## Observed behavior

- TFT showed live temperature, humidity, pressure, validity, and sequence information after display refresh and layout adjustments.
- F407 telemetry arrived at the ESP32; the ESP32 published JSON readings to MQTT.
- A network interruption was observed: reconnect counters increased, frames accumulated in the bounded RAM cache, and publishing resumed after Wi-Fi/MQTT reconnection. One cache overflow was observed during a longer outage, so lossless delivery is not claimed.
- A reset/power-cycle experiment on the F407 produced temporary `valid=0` frames followed by valid readings. The ESP32 continued to publish these states; consumers must interpret the validity field.
- ESP32 health logs showed UART byte/frame counts and MQTT publish counters progressing during normal operation.

## Clean source verification

- STM32F407 default firmware built successfully from this publication tree with CMake, Ninja, and GNU Arm Embedded GCC 13.3.1. The resulting image used approximately 44.9 KiB of flash and 14.9 KiB of RAM in this build. Toolchain warnings about unimplemented newlib syscall stubs and unused parameters remain; the link completed successfully.
- The host suite configured and built from this publication tree, and all 19 registered CTest cases passed. A fresh run on 2026-11-01 with MinGW-w64 GCC 8.1.0 also passed 19/19 cases.
- ESP32-S3 firmware built successfully from this publication tree with ESP-IDF v5.4. The app image is `0xDA220` bytes; the configured 1 MiB application partition had about 15% remaining at build time.

## Limits and reproduction

The historical serial logs are intentionally not included because they contain local environment details and are not needed to understand the reproduction steps. Exact firmware revisions, outage durations, and quantitative pass thresholds should be recorded in a new test run before making reliability or performance claims.

Follow [`BUILD_AND_FLASH.md`](BUILD_AND_FLASH.md), then record firmware revision, board wiring, broker/topic, test start/end time, counters before and after each fault, and received MQTT payloads. Do not include real credentials, public IPs, MAC addresses, or local user paths in the report.
