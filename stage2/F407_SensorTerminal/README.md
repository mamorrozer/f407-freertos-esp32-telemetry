# STM32F407 Sensor Terminal

This firmware samples BMP280 and AHT20 sensors, renders readings on an ST7735S TFT, handles four keys, and streams each sample to the ESP32-S3 over USART1.

## Build

Install CMake, Ninja, and `arm-none-eabi-gcc` on `PATH`. From this directory:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-toolchain.cmake
cmake --build build
```

The default `F407_TELEMETRY_BINARY_TX=0` build sends readable text lines. To match the ESP32 binary-frame receiver, configure a separate build directory with `-DF407_TELEMETRY_BINARY_TX=1`:

```powershell
cmake -S . -B build-binary -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-toolchain.cmake -DF407_TELEMETRY_BINARY_TX=1
cmake --build build-binary
```

See the root [build and flash guide](../../docs/BUILD_AND_FLASH.md) for host tests, flashing, UART wiring, and the ESP32 MQTT path.

## Runtime behavior

- `SensorTask` samples BMP280 and AHT20 at 1 Hz and feeds separate display and telemetry queues.
- `DisplayTask` is the sole TFT writer and handles debounced active-low keys: K1 pause/resume drawing, K2 pressure units, long K3 clears extrema, K4 changes pages.
- `TelemetryTask` drains its own queue and sends frames over USART1 at 115200 8N1.
- `HealthTask` periodically reports task, queue, sensor, and heap observations in text mode.
- The I2C2 bus is protected by a FreeRTOS mutex. The TFT is on SPI1.

## Pin summary

| Function | Pins |
|---|---|
| Sensor I2C2 | PB10 SCL, PB11 SDA |
| TFT SPI1 | PA5 SCK, PA7 MOSI, PA4 CS, PB0 reset, PB1 data/command, PB12 backlight |
| Keys | PB6–PB9, active low |
| USART1 | PA9 TX, PA10 RX |

Board and module revisions can differ. Check the board pinout, display voltage, and logic levels before wiring.
