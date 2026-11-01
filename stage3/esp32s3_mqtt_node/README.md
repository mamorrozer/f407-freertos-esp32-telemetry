# ESP32-S3 MQTT Node

Target: ESP32-S3 N16R8, ESP-IDF v5.4.

Responsibilities:

- UART1: `GPIO17=TX`, `GPIO18=RX`, 115200 8N1.
- Wi-Fi Station with reconnect on disconnect.
- MQTT connection with reconnect event tracking.
- Accepts F407 readable sensor lines and binary CRC16 frames.
- Publishes normalized JSON to the configured MQTT topic.
- Buffers frames in a RAM FIFO while the network is unavailable, then retries
  them in FIFO order after Wi-Fi and MQTT are both ready.

Configure credentials locally with `idf.py menuconfig`, under `Telemetry MQTT
node`. Do not commit real credentials. Enable `Publish fixed demo samples
without UART input` in the same menu when testing the ESP32 without sensors.
Disable it before the two-board UART test to avoid mixing synthetic and F407
messages.

The text parser is a separately testable component. It accepts the F407 line
format, checks field boundaries, numeric bases, fixed-point limits and JSON
buffer capacity, then shares the same JSON formatter with binary frames.

The offline queue is intentionally RAM-only. It protects data during a network
outage while the ESP32 remains running; reboot or power loss clears it. The
publish return value means the MQTT client accepted the request, not that a
subscriber has already observed the message.

Optional operability features are disabled by default. `TELEMETRY_REMOTE_CONFIG`
validates and stores ESP32-local configuration in NVS; it does not change the
F407 sampling period. `TELEMETRY_ENABLE_TASK_WDT` only registers the health
task until per-task liveness points and timeout measurements are available.

The repository also contains host-testable protocol, fault-injection, runtime
counter, storage-record, and delivery modules. The storage boundary is not
linked into this application and does not claim SD-card or power-loss
validation. See the root build guide for current setup steps. A successful
firmware build does not prove Wi-Fi, MQTT, or UART wiring; those require
physical testing.
