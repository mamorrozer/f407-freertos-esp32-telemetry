# Architecture

## Data path

1. `SensorTask` samples BMP280 and AHT20 once per second on the F407.
2. It copies each `SensorFrame` into independent display and telemetry queues, so a slow TFT update does not directly block UART delivery.
3. `DisplayTask` owns the ST7735S and handles debounced key input. K1 pauses drawing only; sampling and telemetry continue. K2 changes pressure units, a long K3 press clears session extrema, and K4 changes pages.
4. `TelemetryTask` serializes frames and writes them over USART1. The default build uses readable text lines (`F407_TELEMETRY_BINARY_TX=0`); setting the compile-time option to `1` selects the binary frame format.
5. The ESP32-S3 UART parser validates text or binary frames, normalizes them to JSON, and submits them to MQTT.
6. A bounded RAM FIFO holds frames while network delivery is unavailable and retries them when both Wi-Fi and MQTT are connected.

## FreeRTOS responsibilities on F407

| Task | Responsibility | Synchronization |
|---|---|---|
| Heartbeat | Periodic board liveness indication | `vTaskDelayUntil` |
| Sensor | Periodic I2C acquisition and frame creation | Separate display and telemetry queues |
| Display | TFT rendering and key scanning | Display queue; single-owner peripheral access |
| Telemetry | UART frame transmission | Telemetry queue |
| Health | Periodic task/queue/sensor/heap observations | Read-only snapshots |

The runtime is driven by the FreeRTOS scheduler. Interrupt handlers remain short; application processing is performed in tasks. UART reception on ESP32 uses the ESP-IDF event/task model and the publish/reconnect path is asynchronous with respect to F407 sensor sampling.

The F407 firmware creates an I2C mutex as a bus-access boundary. Current sensor reads are centralized in `SensorTask`, so the present firmware does not demonstrate sustained I2C contention between multiple application tasks. The queue fan-out between sampling, display, and telemetry is the primary demonstrated task-decoupling mechanism.

## Reliability boundaries

- The ESP32 cache is finite and volatile. When full, the oldest/new incoming-frame policy is defined by `stage6/cache/telemetry_cache.c`; inspect the implementation and runtime counters before making stronger delivery claims.
- MQTT QoS 0 has no broker acknowledgement guarantee. ESP MQTT API acceptance is not equivalent to subscriber receipt.
- Sensor validity is carried per frame; invalid readings are not evidence of a valid physical measurement.
- Watchdog, remote configuration, and storage-related modules are optional or standalone unless enabled and linked by the current firmware configuration.

For source-level walkthroughs, start with F407 `Core/Src/main.c`, then `stage4/protocol`, and then ESP32 `main/app_main.c`.
