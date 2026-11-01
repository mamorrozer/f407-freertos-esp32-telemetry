cmake_minimum_required(VERSION 3.22)

project(F407_FreeRTOS_SensorTerminal C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(F407_TELEMETRY_BINARY_TX 0 CACHE STRING "Transmit sensor frames as binary protocol")
set(F407_ENABLE_IWDG 0 CACHE STRING "Enable the optional independent watchdog")
set(F407_TFT_DIAGNOSTIC 0 CACHE STRING "Run the standalone TFT color-cycle diagnostic")

set(MCU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
    -ffunction-sections
    -fdata-sections
    -fno-common
    -Wall
    -Wextra
)

set(CMAKE_EXE_LINKER_FLAGS
    "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -T${CMAKE_SOURCE_DIR}/STM32F407VETX_FLASH.ld --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -Wl,-Map=${PROJECT_NAME}.map -Wl,--print-memory-usage"
)

set(HAL_DIR ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver)
set(CMSIS_DIR ${CMAKE_SOURCE_DIR}/Drivers/CMSIS)
set(FREERTOS_DIR ${CMAKE_SOURCE_DIR}/Middlewares/FreeRTOS_GCC)
set(PROTOCOL_DIR ${CMAKE_SOURCE_DIR}/../../stage4/protocol)
set(COMMON_DIR ${CMAKE_SOURCE_DIR}/../common)

set(SOURCES
    Core/Src/main.c
    ${COMMON_DIR}/telemetry_bridge.c
    ${PROTOCOL_DIR}/telemetry_protocol.c
    Core/Src/bmp280.c
    Core/Src/aht20.c
    Core/Src/st7735.c
    Core/Src/tft_format.c
    Core/Src/sensor_extrema.c
    Core/Src/stm32f4xx_it.c
    Core/Src/stm32f4xx_hal_msp.c
    ${FREERTOS_DIR}/tasks.c
    ${FREERTOS_DIR}/list.c
    ${FREERTOS_DIR}/queue.c
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F/port.c
    ${FREERTOS_DIR}/portable/MemMang/heap_4.c
    ${CMSIS_DIR}/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
    ${CMAKE_SOURCE_DIR}/startup_stm32f407xx_gcc.s
    ${HAL_DIR}/Src/stm32f4xx_hal.c
    ${HAL_DIR}/Src/stm32f4xx_hal_cortex.c
    ${HAL_DIR}/Src/stm32f4xx_hal_dma.c
    ${HAL_DIR}/Src/stm32f4xx_hal_dma_ex.c
    ${HAL_DIR}/Src/stm32f4xx_hal_flash.c
    ${HAL_DIR}/Src/stm32f4xx_hal_flash_ex.c
    ${HAL_DIR}/Src/stm32f4xx_hal_gpio.c
    ${HAL_DIR}/Src/stm32f4xx_hal_i2c.c
    ${HAL_DIR}/Src/stm32f4xx_hal_iwdg.c
    ${HAL_DIR}/Src/stm32f4xx_hal_pwr.c
    ${HAL_DIR}/Src/stm32f4xx_hal_pwr_ex.c
    ${HAL_DIR}/Src/stm32f4xx_hal_rcc.c
    ${HAL_DIR}/Src/stm32f4xx_hal_rcc_ex.c
    ${HAL_DIR}/Src/stm32f4xx_hal_spi.c
    ${HAL_DIR}/Src/stm32f4xx_hal_uart.c
)

add_executable(${PROJECT_NAME}.elf ${SOURCES})

target_compile_options(${PROJECT_NAME}.elf PRIVATE ${MCU_FLAGS})
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    USE_HAL_DRIVER STM32F407xx
    F407_TELEMETRY_BINARY_TX=${F407_TELEMETRY_BINARY_TX}
    F407_ENABLE_IWDG=${F407_ENABLE_IWDG}
    F407_TFT_DIAGNOSTIC=${F407_TFT_DIAGNOSTIC})
target_include_directories(${PROJECT_NAME}.elf PRIVATE
    Core/Inc
    ${COMMON_DIR}
    ${PROTOCOL_DIR}
    ${FREERTOS_DIR}/include
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F
    ${CMSIS_DIR}/Include
    ${CMSIS_DIR}/Device/ST/STM32F4xx/Include
    ${HAL_DIR}/Inc
)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMENT "Creating HEX and BIN images"
)
