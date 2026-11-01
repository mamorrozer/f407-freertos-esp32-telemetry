# Build, configure, flash, and observe

## STM32F407

Install CMake, Ninja, and an `arm-none-eabi` GCC toolchain, then ensure `arm-none-eabi-gcc`, `arm-none-eabi-objcopy`, and `arm-none-eabi-size` are on `PATH`.

From `stage2/F407_SensorTerminal` in PowerShell:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-toolchain.cmake
cmake --build build
```

The default build sends readable text lines. To match the ESP32 binary-frame receiver, add `-DF407_TELEMETRY_BINARY_TX=1` and use a separate build directory, for example `-B build-binary`.

The build emits `F407_FreeRTOS_SensorTerminal.hex` and `.bin` under `build/`. To flash with STM32CubeProgrammer CLI, adapt the executable path and ST-LINK connection for your machine:

```powershell
STM32_Programmer_CLI.exe -c port=SWD -w build/F407_FreeRTOS_SensorTerminal.hex -v -rst
```

USART1 uses 115200 8N1. When the binary telemetry option is enabled, connect the F407 UART to ESP32 as described in the root README; diagnostic text is intentionally kept off that stream.

## Host-side test suite

Install CMake, Ninja, and a host GCC compiler. From the repository root:

```powershell
cmake -S host_tests -B host_tests/build -G Ninja
cmake --build host_tests/build
ctest --test-dir host_tests/build --output-on-failure
```

The suite covers sensor drivers, display rendering, protocol parsing/encoding, cache, delivery, configuration, health, and selected integration paths. It does not replace MCU firmware builds or physical hardware checks. The smaller TFT-focused PowerShell runner is also available from `stage2/F407_SensorTerminal`.

## ESP32-S3

Install ESP-IDF v5.4 and its supported toolchain. In an ESP-IDF PowerShell terminal, set the target and configure the project:

```powershell
cd stage3/esp32s3_mqtt_node
idf.py set-target esp32s3
idf.py menuconfig
```

Under `Telemetry MQTT node`, enter the Wi-Fi SSID/password, MQTT broker URI, and topic. These values are stored in the local generated `sdkconfig`, which is ignored by Git. Use a broker reachable from the ESP32 network. For a local broker, the host computer's LAN address is required; `localhost` on the ESP32 refers to the ESP32 itself.

Build and flash (replace `COMx` with the board's actual port):

```powershell
idf.py build
idf.py -p COMx flash monitor
```

Subscribe in MQTTX to the configured topic. Keep the publish topic unique when using a public test broker. Never publish Wi-Fi credentials or private broker credentials in logs or screenshots.

## Two-board UART wiring

| F407 | ESP32-S3 | Notes |
|---|---|---|
| PA9 / USART1 TX | GPIO18 / UART1 RX | Cross TX to RX |
| PA10 / USART1 RX | GPIO17 / UART1 TX | Cross TX to RX |
| GND | GND | Common ground required |

Use 3.3 V UART logic. Disconnect the UART bridge when independently monitoring the F407 through a USB-to-UART adapter on the same pins.
