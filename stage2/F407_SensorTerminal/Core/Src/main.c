#include "main.h"
#include "FreeRTOS.h"
#include "aht20.h"
#include "bmp280.h"
#include "queue.h"
#include "sensor_frame.h"
#include "sensor_extrema.h"
#include "semphr.h"
#include "st7735.h"
#include "stm32f4xx_hal_iwdg.h"
#include "tft_format.h"
#include "telemetry_bridge.h"
#include "telemetry_protocol.h"
#include "task.h"

#include <stdio.h>
#include <string.h>

#define HEARTBEAT_PERIOD_MS 500U
#define SENSOR_PERIOD_MS 1000U
#define HEALTH_PERIOD_MS 5000U
#define SENSOR_QUEUE_LENGTH 4U
#define DISPLAY_SCAN_PERIOD_MS 20U
#define KEY_DEBOUNCE_SCANS 2U
#define TFT_TEXT_COLUMNS 20U
#define TFT_DATA_ROWS 6U
#define TFT_COLOR_LABEL 0x7BEFU
#define TFT_COLOR_PRESSURE 0xAD7FU
#define TASK_PRIORITY_LOW (tskIDLE_PRIORITY + 1U)
#define TASK_PRIORITY_SENSOR (tskIDLE_PRIORITY + 2U)

#define TFT_SCK_PORT GPIOA
#define TFT_SCK_PIN GPIO_PIN_5
#define TFT_MOSI_PORT GPIOA
#define TFT_MOSI_PIN GPIO_PIN_7
#define TFT_CS_PORT GPIOA
#define TFT_CS_PIN GPIO_PIN_4
#define TFT_RST_PORT GPIOB
#define TFT_RST_PIN GPIO_PIN_0
#define TFT_DC_PORT GPIOB
#define TFT_DC_PIN GPIO_PIN_1
#define TFT_BL_PORT GPIOB
#define TFT_BL_PIN GPIO_PIN_12
#define TFT_BL_ACTIVE_HIGH 1U
#define TFT_X_OFFSET 0
#define TFT_Y_OFFSET 0

#define KEY1_PIN GPIO_PIN_6
#define KEY2_PIN GPIO_PIN_7
#define KEY3_PIN GPIO_PIN_8
#define KEY4_PIN GPIO_PIN_9

#ifndef F407_TELEMETRY_BINARY_TX
#define F407_TELEMETRY_BINARY_TX 0
#endif

#ifndef F407_ENABLE_IWDG
#define F407_ENABLE_IWDG 0
#endif

#ifndef F407_TFT_DIAGNOSTIC
#define F407_TFT_DIAGNOSTIC 0
#endif

I2C_HandleTypeDef hi2c2;
UART_HandleTypeDef huart1;
SPI_HandleTypeDef hspi1;
static volatile uint32_t tft_spi_fail;

/* 学习点：队列是任务之间的“传送带”，而不是共享指针。
 * xQueueCreate 的第二个参数决定每次发送时复制多少字节。 */
static QueueHandle_t display_queue;
static QueueHandle_t telemetry_queue;
static TaskHandle_t heartbeat_task_handle;
static TaskHandle_t sensor_task_handle;
static TaskHandle_t display_task_handle;
static TaskHandle_t telemetry_task_handle;
static TaskHandle_t health_task_handle;

/* 设计取舍：I2C 和 UART 都可能被多个执行路径访问，
 * 用互斥锁保护总线/输出的占用范围，避免把锁扩大到整个任务。 */
static SemaphoreHandle_t i2c_mutex;
static volatile uint32_t sensor_sequence;
static volatile uint32_t sensor_ok;
static volatile uint32_t sensor_fail;
static volatile uint32_t display_queue_fail;
static volatile uint32_t telemetry_queue_fail;
static volatile uint32_t display_queue_peak;
static volatile uint32_t telemetry_queue_peak;
static volatile uint32_t heartbeat_task_loops;
static volatile uint32_t sensor_task_loops;
static volatile uint32_t display_task_loops;
static volatile uint32_t telemetry_task_loops;
static volatile uint32_t health_task_loops;
#if F407_TELEMETRY_BINARY_TX
static uint32_t telemetry_binary_fail;
#endif
static uint8_t bmp_i2c_address = BMP280_I2C_ADDR_LOW;
static SemaphoreHandle_t uart_mutex;
static ST7735 display_state;
#if F407_ENABLE_IWDG
static IWDG_HandleTypeDef hiwdg;
#endif

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void uart_write(const char *text);
#if F407_TELEMETRY_BINARY_TX
static void uart_write_bytes(const uint8_t *data, size_t length);
#endif
#if !F407_TELEMETRY_BINARY_TX
static void format_fixed(char *buffer, size_t size, int32_t value);
#endif
static void HeartbeatTask(void *argument);
static void SensorTask(void *argument);
static void DisplayTask(void *argument);
static void TelemetryTask(void *argument);
static void HealthTask(void *argument);
#if F407_ENABLE_IWDG
static void MX_IWDG_Init(void);
#endif
#if F407_TELEMETRY_BINARY_TX
static void telemetry_send_binary(const SensorFrame *frame);
#endif
static int bmp_read(void *context, uint8_t reg, uint8_t *data, uint16_t length);
static int bmp_write(void *context, uint8_t reg, const uint8_t *data,
                     uint16_t length);
static int aht_read(void *context, uint8_t *data, uint16_t length);
static int aht_write(void *context, const uint8_t *data, uint16_t length);
static void aht_delay(void *context, uint32_t milliseconds);
static int tft_write_bytes(void *context, const uint8_t *bytes,
                           uint16_t length);
static void tft_write_cs(void *context, uint8_t level);
static void tft_write_dc(void *context, uint8_t level);
static void tft_write_rst(void *context, uint8_t level);
static void tft_write_bl(void *context, uint8_t level);
static void tft_delay_ms(void *context, uint32_t milliseconds);
static void tft_render_page(const SensorFrame *frame, uint8_t page);
static void tft_copy_row(char destination[TFT_TEXT_COLUMNS + 1U],
                         const char *text);
static void tft_draw_page0_icons(void);
static void tft_draw_cached_text(uint16_t x, uint16_t y, uint16_t max_chars,
                                 uint8_t scale_numerator,
                                 uint8_t scale_denominator,
                                 const char *text, uint16_t color,
                                 char *cache, size_t cache_size,
                                 uint16_t *cache_color, uint8_t *cache_valid);

typedef struct
{
    uint8_t stable_pressed;
    uint8_t candidate_pressed;
    uint8_t equal_scans;
    uint8_t long_press_handled;
    TickType_t pressed_tick;
} KeyDebounceState;

static KeyDebounceState key_states[4];
static uint8_t display_hold;
static uint8_t pressure_unit_hpa;
static SensorExtrema sensor_extrema;

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C2_Init();
    MX_SPI1_Init();
    MX_USART1_UART_Init();
#if F407_ENABLE_IWDG
    MX_IWDG_Init();
#endif

    uart_write("F407 sensor terminal starting\r\n");
    /* 两条队列各自拥有一份 SensorFrame，显示和遥测可以独立消费。
     * 队列满时当前策略是记录丢弃，而不是让采集任务无限等待。 */
    display_queue = xQueueCreate(SENSOR_QUEUE_LENGTH, sizeof(SensorFrame));
    telemetry_queue = xQueueCreate(SENSOR_QUEUE_LENGTH, sizeof(SensorFrame));
    i2c_mutex = xSemaphoreCreateMutex();
    uart_mutex = xSemaphoreCreateMutex();
    if ((display_queue == NULL) ||
        (telemetry_queue == NULL) || (i2c_mutex == NULL) ||
        (uart_mutex == NULL))
    {
        Error_Handler();
    }

    if (xTaskCreate(HeartbeatTask, "Heartbeat", configMINIMAL_STACK_SIZE,
                    NULL, TASK_PRIORITY_LOW, &heartbeat_task_handle) != pdPASS ||
        xTaskCreate(SensorTask, "Sensor", configMINIMAL_STACK_SIZE + 128U,
                    NULL, TASK_PRIORITY_SENSOR, &sensor_task_handle) != pdPASS ||
        xTaskCreate(DisplayTask, "Display", configMINIMAL_STACK_SIZE + 128U,
                    NULL, TASK_PRIORITY_LOW, &display_task_handle) != pdPASS ||
        xTaskCreate(TelemetryTask, "Telemetry", configMINIMAL_STACK_SIZE + 128U,
                    NULL, TASK_PRIORITY_LOW, &telemetry_task_handle) != pdPASS ||
        xTaskCreate(HealthTask, "Health", configMINIMAL_STACK_SIZE + 128U,
                    NULL, TASK_PRIORITY_LOW, &health_task_handle) != pdPASS)
    {
        Error_Handler();
    }

    vTaskStartScheduler();
    Error_Handler();
}

static void HeartbeatTask(void *argument)
{
    TickType_t last_wake = xTaskGetTickCount();
    (void)argument;
    for (;;)
    {
        heartbeat_task_loops++;
        HAL_GPIO_TogglePin(USER_LED_GPIO_Port, USER_LED_Pin);
#if F407_ENABLE_IWDG
        (void)HAL_IWDG_Refresh(&hiwdg);
#endif
#if !F407_TELEMETRY_BINARY_TX
        uart_write("rtos alive\r\n");
#endif
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS));
    }
}

static void SensorTask(void *argument)
{
    BMP280 bmp = {0};
    AHT20 aht = {0};
    BMP280_Bus bmp_bus = {.read = bmp_read, .write = bmp_write, .context = &hi2c2};
    AHT20_Bus aht_bus = {.read = aht_read, .write = aht_write,
                         .delay_ms = aht_delay, .context = &hi2c2};
    TickType_t last_wake = xTaskGetTickCount();
    BaseType_t bmp_ready = pdFALSE;
    BaseType_t aht_ready = pdFALSE;

    (void)argument;
    /* 学习点：初始化和周期读取都要和其他 I2C 使用者串行化。
     * 获取不到锁时本轮仍会生成一帧无效位信息，便于上层观察故障。 */
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(1000U)) == pdTRUE)
    {
        bmp_ready = bmp280_init(&bmp, &bmp_bus, BMP280_I2C_ADDR_LOW) == 0
                        ? pdTRUE : pdFALSE;
        if (bmp_ready != pdTRUE)
        {
            bmp_i2c_address = BMP280_I2C_ADDR_HIGH;
            bmp_ready = bmp280_init(&bmp, &bmp_bus, BMP280_I2C_ADDR_HIGH) == 0
                            ? pdTRUE : pdFALSE;
        }
        aht_ready = aht20_init(&aht, &aht_bus) == 0 ? pdTRUE : pdFALSE;
        xSemaphoreGive(i2c_mutex);
    }
    uart_write(bmp_ready == pdTRUE ? "bmp280 init ok\r\n" : "bmp280 init fail\r\n");
    uart_write(aht_ready == pdTRUE ? "aht20 init ok\r\n" : "aht20 init fail\r\n");

    for (;;)
    {
        sensor_task_loops++;
        SensorFrame frame = {.sequence = ++sensor_sequence,
                             .timestamp_ms = (uint32_t)xTaskGetTickCount()};
        BMP280_Sample bmp_sample = {0};
        AHT20_Sample aht_sample = {0};

        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(500U)) == pdTRUE)
        {
            /* 故障恢复：掉电模块会丢失测量配置，因此每个采样周期重试一次。
             * 初始化成功后等到下一周期再读取，避免把“刚初始化”误当成有效样本。 */
            if (bmp_ready != pdTRUE)
            {
                bmp_i2c_address = BMP280_I2C_ADDR_LOW;
                bmp_ready = bmp280_init(&bmp, &bmp_bus, bmp_i2c_address) == 0
                                ? pdTRUE : pdFALSE;
                if (bmp_ready != pdTRUE)
                {
                    bmp_i2c_address = BMP280_I2C_ADDR_HIGH;
                    bmp_ready = bmp280_init(&bmp, &bmp_bus, bmp_i2c_address) == 0
                                    ? pdTRUE : pdFALSE;
                }
            }
            else if (bmp280_read_sample(&bmp, &bmp_sample) == 0)
            {
                frame.temperature_centi_c = bmp_sample.temperature_centi_c;
                frame.pressure_pa = bmp_sample.pressure_pa;
                frame.valid |= SENSOR_FRAME_VALID_BMP280;
            }
            else
            {
                bmp_ready = pdFALSE;
            }
            if (aht_ready != pdTRUE)
            {
                aht_ready = aht20_init(&aht, &aht_bus) == 0 ? pdTRUE : pdFALSE;
            }
            else if (aht20_read_sample(&aht, &aht_sample) == 0)
            {
                frame.temperature_centi_c = aht_sample.temperature_centi_c;
                frame.humidity_centi_percent = aht_sample.humidity_centi_percent;
                frame.valid |= SENSOR_FRAME_VALID_AHT20;
            }
            else
            {
                aht_ready = pdFALSE;
            }
            xSemaphoreGive(i2c_mutex);
        }

        /* 证据边界：valid=3 只表示 BMP280 和 AHT20 本次读取都被驱动接受，
         * 不代表传感器经过校准，也不代表系统长期稳定。 */
        if (frame.valid == (SENSOR_FRAME_VALID_BMP280 | SENSOR_FRAME_VALID_AHT20))
        {
            sensor_ok++;
        }
        else
        {
            sensor_fail++;
        }
        /* xQueueSend 会复制 frame 的内容；这里不能传递局部变量地址。
         * 0U 表示不等待，队列压力通过 fail/peak 统计暴露出来。 */
        if (xQueueSend(display_queue, &frame, 0U) != pdPASS)
        {
            display_queue_fail++;
        }
        else
        {
            UBaseType_t depth = uxQueueMessagesWaiting(display_queue);
            if ((uint32_t)depth > display_queue_peak)
            {
                display_queue_peak = (uint32_t)depth;
            }
        }
        if (xQueueSend(telemetry_queue, &frame, 0U) != pdPASS)
        {
            telemetry_queue_fail++;
        }
        else
        {
            UBaseType_t depth = uxQueueMessagesWaiting(telemetry_queue);
            if ((uint32_t)depth > telemetry_queue_peak)
            {
                telemetry_queue_peak = (uint32_t)depth;
            }
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
    }
}

static void DisplayTask(void *argument)
{
    static const uint16_t key_pins[4] = {
        KEY1_PIN, KEY2_PIN, KEY3_PIN, KEY4_PIN};
    ST7735_Bus bus = {.write_sck = NULL,
                      .write_mosi = NULL,
                      .write_cs = tft_write_cs,
                      .write_dc = tft_write_dc,
                      .write_rst = tft_write_rst,
                      .write_bl = tft_write_bl,
                      .delay_ms = tft_delay_ms,
                      .write_bytes = tft_write_bytes,
                      .context = &hspi1};
    SensorFrame latest_frame = {0};
    TickType_t last_scan = xTaskGetTickCount();
    uint8_t have_frame = 0U;
    uint8_t page = 0U;
    uint8_t redraw = 0U;
    BaseType_t display_ready;
    (void)argument;

    display_ready = st7735_init(&display_state, &bus, 0U, TFT_X_OFFSET,
                                TFT_Y_OFFSET) == 0
                        ? pdTRUE : pdFALSE;
    uart_write(display_ready == pdTRUE ? "tft init ok\r\n" : "tft init fail\r\n");
#if F407_TFT_DIAGNOSTIC
    {
        static const uint16_t diagnostic_colors[] = {
            ST7735_RED, ST7735_GREEN, ST7735_BLUE, ST7735_BLACK};
        uint8_t color_index = 0U;

        /* 诊断模式故意不依赖传感器、队列或字体：只验证
         * 初始化、地址窗口和连续 RGB565 像素写入。 */
        for (;;)
        {
            int fill_result = display_ready == pdTRUE
                                   ? st7735_fill(&display_state,
                                                 diagnostic_colors[color_index])
                                   : -1;
            char line[48];
            (void)snprintf(line, sizeof(line),
                           "tft diag color=%u fill=%d\r\n",
                           (unsigned int)color_index, fill_result);
            uart_write(line);
            color_index = (uint8_t)((color_index + 1U) %
                                    (sizeof(diagnostic_colors) /
                                     sizeof(diagnostic_colors[0])));
            vTaskDelay(pdMS_TO_TICKS(1500U));
        }
    }
#endif
#if !F407_TFT_DIAGNOSTIC
    if (display_ready == pdTRUE)
    {
        /* 先画出启动画面，不等待第一帧传感器数据，便于区分
         * “显示链路正常但暂无数据”和“显示链路没有工作”。 */
        tft_render_page(NULL, 0U);
    }
#endif
    for (;;)
    {
        display_task_loops++;
        SensorFrame received_frame;
        /* 该任务只消费显示队列，不直接读取传感器，形成采集与显示的解耦。 */
        if (xQueueReceive(display_queue, &received_frame,
                          pdMS_TO_TICKS(DISPLAY_SCAN_PERIOD_MS)) == pdPASS)
        {
            latest_frame = received_frame;
            have_frame = 1U;
            /* 统计跟着采样帧更新，而不是跟着页面绘制更新：暂停屏幕或
             * 切到其他页时，后台仍会持续记录真实的传感器极值。 */
            sensor_extrema_update(&sensor_extrema, &received_frame);
            redraw = 1U;
        }

        if ((xTaskGetTickCount() - last_scan) >=
            pdMS_TO_TICKS(DISPLAY_SCAN_PERIOD_MS))
        {
            last_scan = xTaskGetTickCount();
            for (uint8_t index = 0U; index < 4U; ++index)
            {
                uint8_t pressed = (HAL_GPIO_ReadPin(GPIOB, key_pins[index]) ==
                                   GPIO_PIN_RESET) ? 1U : 0U;
                KeyDebounceState *state = &key_states[index];
                if (pressed != state->candidate_pressed)
                {
                    state->candidate_pressed = pressed;
                    state->equal_scans = 1U;
                }
                else if (state->equal_scans < KEY_DEBOUNCE_SCANS)
                {
                    state->equal_scans++;
                    if ((state->equal_scans >= KEY_DEBOUNCE_SCANS) &&
                        (state->stable_pressed != pressed))
                    {
                        state->stable_pressed = pressed;
                        if (pressed != 0U)
                        {
                            state->pressed_tick = xTaskGetTickCount();
                            state->long_press_handled = 0U;
                            if (index == 0U)
                            {
                                display_hold = display_hold == 0U ? 1U : 0U;
                            }
                            else if (index == 1U)
                            {
                                pressure_unit_hpa = pressure_unit_hpa == 0U
                                                        ? 1U : 0U;
                            }
                            else if (index == 3U)
                            {
                                page = page == 0U ? 1U : 0U;
                            }
                            redraw = 1U;
                        }
                        else
                        {
                            state->long_press_handled = 0U;
                        }
                    }
                }
                if ((index == 2U) && (state->stable_pressed != 0U) &&
                    (state->long_press_handled == 0U) &&
                    ((TickType_t)(xTaskGetTickCount() - state->pressed_tick) >=
                     pdMS_TO_TICKS(800U)))
                {
                    sensor_extrema_clear(&sensor_extrema);
                    state->long_press_handled = 1U;
                    redraw = 1U;
                }
            }
        }

        if ((redraw != 0U) && (display_ready == pdTRUE))
        {
            if (display_hold == 0U)
            {
                tft_render_page(have_frame != 0U ? &latest_frame : NULL, page);
            }
            redraw = 0U;
        }
    }
}

static void tft_draw_scanline_outline(uint16_t center_x, uint16_t top_y,
                                      const uint8_t *widths, size_t row_count,
                                      uint16_t color, uint8_t close_bottom)
{
    uint16_t previous_left = 0U;
    uint16_t previous_right = 0U;

    for (size_t row = 0U; row < row_count; ++row)
    {
        uint16_t width = widths[row];
        uint16_t left = (uint16_t)(center_x - width / 2U);
        uint16_t right = (uint16_t)(left + width - 1U);
        uint16_t y = (uint16_t)(top_y + row);

        if (row == 0U)
        {
            (void)st7735_draw_pixel(&display_state, left, y, color);
            (void)st7735_draw_pixel(&display_state, right, y, color);
        }
        else
        {
            uint16_t left_start = left < previous_left ? left : previous_left;
            uint16_t left_end = left > previous_left ? left : previous_left;
            uint16_t right_start = right < previous_right ? right : previous_right;
            uint16_t right_end = right > previous_right ? right : previous_right;
            (void)st7735_fill_rect(&display_state, left_start, y, left_end, y,
                                   color);
            (void)st7735_fill_rect(&display_state, right_start, y, right_end, y,
                                   color);
        }

        previous_left = left;
        previous_right = right;

        if ((close_bottom != 0U) && (row + 1U == row_count))
        {
            /* Close the teardrop with a short flat chord; the narrowing rows
             * above it provide the rounded lower half at this pixel scale. */
            (void)st7735_fill_rect(&display_state, left, y, right, y, color);
        }
    }
}

static void tft_draw_page0_icons(void)
{
    static const uint8_t drop_widths[] = {
        2U, 4U, 6U, 8U, 10U, 12U, 14U, 16U, 18U, 20U,
        22U, 22U, 22U, 21U, 20U, 18U, 15U, 11U, 7U, 4U};
    static const uint8_t gauge_widths[] = {
        6U, 14U, 20U, 26U, 30U, 34U, 36U, 36U, 34U, 30U};

    /* C-style pixel line art: scanline silhouettes keep the contours rounder
     * than assembling a symbol from a few large rectangles. Draw only when
     * the live-page layout changes, never on each sensor sample. */

    /* Thermometer: slightly shorter stem, rounded cap and fuller bulb. */
    (void)st7735_fill_rect(&display_state, 101U, 33U, 101U, 44U,
                           TFT_COLOR_LABEL);
    (void)st7735_fill_rect(&display_state, 107U, 33U, 107U, 44U,
                           TFT_COLOR_LABEL);
    (void)st7735_fill_rect(&display_state, 103U, 31U, 105U, 31U,
                           TFT_COLOR_LABEL);
    (void)st7735_fill_rect(&display_state, 102U, 32U, 106U, 32U,
                           TFT_COLOR_LABEL);
    (void)st7735_fill_rect(&display_state, 102U, 45U, 106U, 45U,
                           TFT_COLOR_LABEL);
    (void)st7735_fill_rect(&display_state, 100U, 46U, 108U, 46U,
                           TFT_COLOR_LABEL);
    (void)st7735_fill_rect(&display_state, 99U, 47U, 109U, 47U,
                           TFT_COLOR_LABEL);
    (void)st7735_fill_rect(&display_state, 99U, 48U, 109U, 48U,
                           TFT_COLOR_LABEL);
    (void)st7735_fill_rect(&display_state, 100U, 49U, 108U, 49U,
                           TFT_COLOR_LABEL);
    (void)st7735_fill_rect(&display_state, 102U, 50U, 106U, 50U,
                           TFT_COLOR_LABEL);
    (void)st7735_fill_rect(&display_state, 103U, 36U, 105U, 46U,
                           ST7735_YELLOW);
    (void)st7735_fill_rect(&display_state, 101U, 47U, 107U, 48U,
                           ST7735_YELLOW);

    /* Teardrop: pointed crown, gently bowed sides, and a rounded broad base. */
    tft_draw_scanline_outline(107U, 62U, drop_widths,
                              sizeof(drop_widths), ST7735_CYAN, 1U);
    (void)st7735_fill_rect(&display_state, 100U, 75U, 100U, 77U,
                           ST7735_WHITE);
    (void)st7735_fill_rect(&display_state, 101U, 78U, 102U, 78U,
                           ST7735_WHITE);

    /* Barometer: shifted down to visually center it beside the pressure row. */
    tft_draw_scanline_outline(107U, 99U, gauge_widths,
                              sizeof(gauge_widths), TFT_COLOR_PRESSURE, 0U);
    (void)st7735_fill_rect(&display_state, 89U, 109U, 125U, 109U,
                           TFT_COLOR_PRESSURE);
    (void)st7735_fill_rect(&display_state, 92U, 106U, 92U, 108U,
                           TFT_COLOR_PRESSURE);
    (void)st7735_fill_rect(&display_state, 106U, 100U, 106U, 104U,
                           TFT_COLOR_PRESSURE);
    (void)st7735_fill_rect(&display_state, 121U, 106U, 121U, 108U,
                           TFT_COLOR_PRESSURE);
    (void)st7735_fill_rect(&display_state, 99U, 104U, 99U, 106U,
                           TFT_COLOR_PRESSURE);
    (void)st7735_fill_rect(&display_state, 113U, 104U, 113U, 106U,
                           TFT_COLOR_PRESSURE);
    (void)st7735_fill_rect(&display_state, 105U, 106U, 107U, 109U,
                           ST7735_CYAN);
    (void)st7735_draw_pixel(&display_state, 108U, 105U, ST7735_CYAN);
    (void)st7735_draw_pixel(&display_state, 109U, 104U, ST7735_CYAN);
    (void)st7735_draw_pixel(&display_state, 110U, 103U, ST7735_CYAN);
    (void)st7735_draw_pixel(&display_state, 111U, 102U, ST7735_CYAN);
}

static void tft_render_page(const SensorFrame *frame, uint8_t page)
{
    static uint8_t rendered_page = 0xFFU;
    static uint8_t row_cache_valid[2];
    static char row_cache[2][TFT_DATA_ROWS][TFT_TEXT_COLUMNS + 1U];
    static uint16_t row_color_cache[2][TFT_DATA_ROWS];
    static const uint16_t row_y[TFT_DATA_ROWS] = {
        28U, 48U, 68U, 88U, 108U, 128U};
    static uint8_t page0_field_cache_valid[5];
    static char temperature_cache[6];
    static char humidity_cache[12];
    static char pressure_cache[12];
    static char status_cache[12];
    static char sequence_cache[20];
    static uint16_t temperature_cache_color;
    static uint16_t humidity_cache_color;
    static uint16_t pressure_cache_color;
    static uint16_t status_cache_color;
    static uint16_t sequence_cache_color;
    static uint8_t last_pressure_unit_hpa = 0xFFU;
    static uint8_t displayed_sequence_valid;
    static uint32_t displayed_sequence;
    static TickType_t sequence_refresh_tick;
    char rows[TFT_DATA_ROWS][TFT_TEXT_COLUMNS + 1U];
    char line[32];
    char value[16];
    uint16_t row_color[TFT_DATA_ROWS] = {
        ST7735_WHITE, ST7735_WHITE, ST7735_WHITE,
        ST7735_YELLOW, ST7735_YELLOW};
    uint8_t page_index = page == 0U ? 0U : 1U;
    uint8_t page_changed = rendered_page != page;

    if ((frame != NULL) &&
        ((displayed_sequence_valid == 0U) || (page_changed != 0U) ||
         ((TickType_t)(xTaskGetTickCount() - sequence_refresh_tick) >=
          pdMS_TO_TICKS(5000U))))
    {
        displayed_sequence = frame->sequence;
        displayed_sequence_valid = 1U;
        sequence_refresh_tick = xTaskGetTickCount();
    }

    if (rendered_page != page)
    {
        (void)st7735_fill(&display_state, ST7735_BLACK);
        (void)st7735_draw_text(&display_state, 4U, 4U, "F407 / TELEMETRY",
                               ST7735_WHITE, ST7735_BLACK);
        (void)st7735_fill_rect(&display_state, 4U, 15U, 123U, 15U,
                               TFT_COLOR_PRESSURE);
        (void)snprintf(line, sizeof(line), "PAGE: %u", (unsigned int)page);
        (void)st7735_draw_text(&display_state, 4U, 140U, line,
                               ST7735_CYAN, ST7735_BLACK);
        if (page == 0U)
        {
            (void)st7735_draw_text(&display_state, 8U, 23U, "TEMPERATURE",
                                   TFT_COLOR_LABEL, ST7735_BLACK);
            (void)st7735_draw_text(&display_state, 74U, 42U, "C",
                                   TFT_COLOR_LABEL, ST7735_BLACK);
            (void)st7735_fill_rect(&display_state, 8U, 57U, 123U, 57U,
                                   TFT_COLOR_PRESSURE);
            (void)st7735_draw_text(&display_state, 8U, 63U, "HUMIDITY %",
                                   TFT_COLOR_LABEL, ST7735_BLACK);
            (void)st7735_draw_text(&display_state, 8U, 90U, "PRESS KPA",
                                   TFT_COLOR_LABEL, ST7735_BLACK);
            (void)st7735_fill_rect(&display_state, 8U, 118U, 123U, 118U,
                                   TFT_COLOR_PRESSURE);
            tft_draw_page0_icons();
            last_pressure_unit_hpa = pressure_unit_hpa;
        }
        rendered_page = page;
        row_cache_valid[page_index] = 0U;
        memset(page0_field_cache_valid, 0, sizeof(page0_field_cache_valid));
    }

    if (page == 0U)
    {
        char temperature_text[6] = "WAIT ";
        char humidity_text[12] = "--.-%";
        char pressure_text[12] = "---.-";
        char status_text[12] = "WAIT";
        char sequence_text[20] = "SEQ: --";
        uint16_t temperature_color = ST7735_YELLOW;
        uint16_t humidity_color = ST7735_CYAN;
        uint16_t pressure_color = TFT_COLOR_PRESSURE;
        uint16_t status_color = ST7735_YELLOW;
        if ((last_pressure_unit_hpa != pressure_unit_hpa) &&
            (rendered_page == 0U))
        {
            (void)st7735_fill_rect(&display_state, 8U, 90U, 70U, 97U,
                                   ST7735_BLACK);
            (void)st7735_draw_text(
                &display_state, 8U, 90U,
                pressure_unit_hpa != 0U ? "PRESS HPA" : "PRESS KPA",
                TFT_COLOR_LABEL, ST7735_BLACK);
            last_pressure_unit_hpa = pressure_unit_hpa;
        }

        if (frame != NULL)
        {
            if ((frame->valid & SENSOR_FRAME_VALID_AHT20) != 0U)
            {
                tft_format_temperature(value, sizeof(value),
                                       frame->temperature_centi_c);
                size_t temperature_length = strlen(value);
                if (temperature_length > 5U)
                {
                    memcpy(temperature_text, "RANGE", 5U);
                }
                else
                {
                    memcpy(temperature_text, value, temperature_length);
                    memset(&temperature_text[temperature_length], ' ',
                           5U - temperature_length);
                }

                uint32_t humidity_tenths =
                    (frame->humidity_centi_percent + 5U) / 10U;
                (void)snprintf(humidity_text, sizeof(humidity_text),
                               "%lu.%01lu%%",
                               (unsigned long)(humidity_tenths / 10U),
                               (unsigned long)(humidity_tenths % 10U));
            }
            else
            {
                (void)snprintf(temperature_text, sizeof(temperature_text),
                               "ERR  ");
                (void)snprintf(humidity_text, sizeof(humidity_text), "ERR");
                temperature_color = ST7735_RED;
                humidity_color = ST7735_RED;
            }

            if ((frame->valid & SENSOR_FRAME_VALID_BMP280) != 0U)
            {
                uint32_t pressure_tenths_kpa =
                    (frame->pressure_pa + 50U) / 100U;
                if (pressure_unit_hpa != 0U)
                {
                    (void)snprintf(pressure_text, sizeof(pressure_text), "%lu",
                                   (unsigned long)((frame->pressure_pa + 50U) /
                                                   100U));
                }
                else
                {
                    (void)snprintf(pressure_text, sizeof(pressure_text),
                                   "%lu.%01lu",
                                   (unsigned long)(pressure_tenths_kpa / 10U),
                                   (unsigned long)(pressure_tenths_kpa % 10U));
                }
            }
            else
            {
                (void)snprintf(pressure_text, sizeof(pressure_text), "ERR");
                pressure_color = ST7735_RED;
            }

            status_color = frame->valid == (SENSOR_FRAME_VALID_BMP280 |
                                             SENSOR_FRAME_VALID_AHT20)
                               ? ST7735_GREEN : ST7735_RED;
            (void)snprintf(status_text, sizeof(status_text), "%s",
                           status_color == ST7735_GREEN ? "VALID" : "ERR");
        }
        if (displayed_sequence_valid != 0U)
        {
            (void)snprintf(sequence_text, sizeof(sequence_text), "SEQ: %lu",
                           (unsigned long)displayed_sequence);
        }

        tft_draw_cached_text(8U, 35U, 5U, 2U, 1U, temperature_text,
                             temperature_color, temperature_cache,
                             sizeof(temperature_cache),
                             &temperature_cache_color,
                             &page0_field_cache_valid[0]);
        tft_draw_cached_text(8U, 73U, 6U, 3U, 2U, humidity_text, humidity_color,
                             humidity_cache, sizeof(humidity_cache),
                             &humidity_cache_color,
                             &page0_field_cache_valid[1]);
        tft_draw_cached_text(8U, 100U, 6U, 3U, 2U, pressure_text, pressure_color,
                             pressure_cache, sizeof(pressure_cache),
                             &pressure_cache_color,
                             &page0_field_cache_valid[2]);
        tft_draw_cached_text(8U, 123U, 5U, 1U, 1U, status_text, status_color,
                             status_cache, sizeof(status_cache),
                             &status_cache_color,
                             &page0_field_cache_valid[3]);
        tft_draw_cached_text(58U, 123U, 11U, 1U, 1U, sequence_text, ST7735_WHITE,
                             sequence_cache, sizeof(sequence_cache),
                             &sequence_cache_color,
                             &page0_field_cache_valid[4]);
        return;
    }

    for (uint8_t row = 0U; row < TFT_DATA_ROWS; ++row)
    {
        memset(rows[row], ' ', TFT_TEXT_COLUMNS);
        rows[row][TFT_TEXT_COLUMNS] = '\0';
    }

    if (page == 1U)
    {
        (void)snprintf(line, sizeof(line), "SENSOR: %s",
                       (frame != NULL) &&
                               (frame->valid == (SENSOR_FRAME_VALID_BMP280 |
                                                 SENSOR_FRAME_VALID_AHT20))
                           ? "OK" : "ERR");
        tft_copy_row(rows[0], line);
        row_color[0] = ((frame != NULL) &&
                        (frame->valid == (SENSOR_FRAME_VALID_BMP280 |
                                          SENSOR_FRAME_VALID_AHT20)))
                           ? ST7735_GREEN : ST7735_RED;
        if (sensor_extrema.temperature_valid != 0U)
        {
            (void)snprintf(line, sizeof(line), "T C %ld/%ld",
                           (long)(sensor_extrema.minimum_temperature_centi_c /
                                  100),
                           (long)(sensor_extrema.maximum_temperature_centi_c /
                                  100));
            tft_copy_row(rows[1], line);
        }
        else
        {
            tft_copy_row(rows[1], "T C: WAITING");
        }
        if (sensor_extrema.humidity_valid != 0U)
        {
            (void)snprintf(line, sizeof(line), "H %% %lu/%lu",
                           (unsigned long)(
                               sensor_extrema.minimum_humidity_centi_percent /
                               100U),
                           (unsigned long)(
                               sensor_extrema.maximum_humidity_centi_percent /
                               100U));
            tft_copy_row(rows[2], line);
        }
        else
        {
            tft_copy_row(rows[2], "H %: WAITING");
        }
        if (sensor_extrema.pressure_valid != 0U)
        {
            if (pressure_unit_hpa != 0U)
            {
                (void)snprintf(line, sizeof(line), "P hPa %lu/%lu",
                               (unsigned long)(
                                   (sensor_extrema.minimum_pressure_pa + 50U) /
                                   100U),
                               (unsigned long)(
                                   (sensor_extrema.maximum_pressure_pa + 50U) /
                                   100U));
            }
            else
            {
                (void)snprintf(line, sizeof(line), "P kPa %lu.%01lu/%lu.%01lu",
                               (unsigned long)((
                                   sensor_extrema.minimum_pressure_pa + 50U) /
                                   1000U),
                               (unsigned long)(((
                                   sensor_extrema.minimum_pressure_pa + 50U) /
                                   100U) % 10U),
                               (unsigned long)((
                                   sensor_extrema.maximum_pressure_pa + 50U) /
                                   1000U),
                               (unsigned long)(((
                                   sensor_extrema.maximum_pressure_pa + 50U) /
                                   100U) % 10U));
            }
            tft_copy_row(rows[3], line);
        }
        else
        {
            tft_copy_row(rows[3], "P: WAITING");
        }
        (void)snprintf(line, sizeof(line), "SEQ: %lu",
                       (unsigned long)displayed_sequence);
        tft_copy_row(rows[4], line);
        (void)snprintf(line, sizeof(line), "FAIL:%lu SPI:%lu",
                       (unsigned long)sensor_fail,
                       (unsigned long)tft_spi_fail);
        tft_copy_row(rows[5], line);
    }
    for (uint8_t row = 0U; row < TFT_DATA_ROWS; ++row)
    {
        if ((row_cache_valid[page_index] == 0U) ||
            (memcmp(rows[row], row_cache[page_index][row],
                    TFT_TEXT_COLUMNS + 1U) != 0) ||
            (row_color[row] != row_color_cache[page_index][row]))
        {
            if (st7735_draw_text(&display_state, 4U, row_y[row], rows[row],
                                 row_color[row], ST7735_BLACK) == 0)
            {
                memcpy(row_cache[page_index][row], rows[row],
                       TFT_TEXT_COLUMNS + 1U);
                row_color_cache[page_index][row] = row_color[row];
            }
        }
    }
    row_cache_valid[page_index] = 1U;
}

static void tft_copy_row(char destination[TFT_TEXT_COLUMNS + 1U],
                         const char *text)
{
    size_t length = strlen(text);
    if (length > TFT_TEXT_COLUMNS)
    {
        length = TFT_TEXT_COLUMNS;
    }
    memcpy(destination, text, length);
}

static void tft_draw_cached_text(uint16_t x, uint16_t y, uint16_t max_chars,
                                 uint8_t scale_numerator,
                                 uint8_t scale_denominator,
                                 const char *text, uint16_t color,
                                 char *cache, size_t cache_size,
                                 uint16_t *cache_color, uint8_t *cache_valid)
{
    if ((text == NULL) || (cache == NULL) || (cache_size == 0U) ||
        (cache_color == NULL) || (cache_valid == NULL) ||
        (strlen(text) >= cache_size))
    {
        return;
    }
    if ((*cache_valid == 0U) || (strcmp(text, cache) != 0) ||
        (color != *cache_color))
    {
        if (scale_denominator == 0U)
        {
            return;
        }
        uint16_t pixel_width = (uint16_t)(
            (max_chars * 6U * scale_numerator + scale_denominator - 1U) /
            scale_denominator);
        uint16_t pixel_height = (uint16_t)(
            (8U * scale_numerator + scale_denominator - 1U) /
            scale_denominator);
        if ((pixel_width == 0U) ||
            ((uint32_t)x + pixel_width > ST7735_WIDTH) ||
            ((uint32_t)y + pixel_height > ST7735_HEIGHT))
        {
            return;
        }
        char padded[TFT_TEXT_COLUMNS + 1U];
        size_t text_length = strlen(text);
        if (text_length > max_chars)
        {
            text_length = max_chars;
        }
        memset(padded, ' ', max_chars);
        memcpy(padded, text, text_length);
        padded[max_chars] = '\0';
        if (st7735_draw_text_scaled_ratio(
                &display_state, x, y, padded, scale_numerator,
                scale_denominator, color, ST7735_BLACK) != 0)
        {
            return;
        }
        (void)strncpy(cache, text, cache_size - 1U);
        cache[cache_size - 1U] = '\0';
        *cache_color = color;
        *cache_valid = 1U;
    }
}

static void TelemetryTask(void *argument)
{
    SensorFrame frame;
#if !F407_TELEMETRY_BINARY_TX
    char temperature[16];
    char humidity[16];
    char line[160];
#endif
    (void)argument;
    for (;;)
    {
        /* 遥测任务可以阻塞等待数据；慢速 UART 不会让 SensorTask 直接执行发送。
         * 但队列容量有限，所以仍必须观察 telemetry_queue_fail。 */
        if (xQueueReceive(telemetry_queue, &frame, portMAX_DELAY) == pdPASS)
        {
            telemetry_task_loops++;
#if F407_TELEMETRY_BINARY_TX
            telemetry_send_binary(&frame);
#else
            format_fixed(temperature, sizeof(temperature), frame.temperature_centi_c);
            format_fixed(humidity, sizeof(humidity),
                         (int32_t)frame.humidity_centi_percent);
            (void)snprintf(line, sizeof(line),
                           "sensor seq=%lu tick=%lu T=%s H=%s P=%lu valid=0x%02X ok=%lu fail=%lu dqfail=%lu dqpeak=%lu tqfail=%lu tqpeak=%lu stack=%lu\r\n",
                           (unsigned long)frame.sequence,
                           (unsigned long)frame.timestamp_ms, temperature, humidity,
                           (unsigned long)frame.pressure_pa, frame.valid,
                           (unsigned long)sensor_ok, (unsigned long)sensor_fail,
                           (unsigned long)display_queue_fail,
                           (unsigned long)display_queue_peak,
                           (unsigned long)telemetry_queue_fail,
                           (unsigned long)telemetry_queue_peak,
                           (unsigned long)uxTaskGetStackHighWaterMark(NULL));
            uart_write(line);
#endif
        }
    }
}

static void HealthTask(void *argument)
{
    TickType_t last_wake = xTaskGetTickCount();
    (void)argument;

    for (;;)
    {
        TaskHandle_t handles[] = {
            heartbeat_task_handle,
            sensor_task_handle,
            display_task_handle,
            telemetry_task_handle,
            health_task_handle};
        UBaseType_t display_depth = uxQueueMessagesWaiting(display_queue);
        UBaseType_t telemetry_depth = uxQueueMessagesWaiting(telemetry_queue);
        UBaseType_t stack_heartbeat =
            uxTaskGetStackHighWaterMark(handles[0]);
        UBaseType_t stack_sensor = uxTaskGetStackHighWaterMark(handles[1]);
        UBaseType_t stack_display = uxTaskGetStackHighWaterMark(handles[2]);
        UBaseType_t stack_telemetry = uxTaskGetStackHighWaterMark(handles[3]);
        UBaseType_t stack_health = uxTaskGetStackHighWaterMark(handles[4]);
        size_t free_heap = xPortGetFreeHeapSize();

        health_task_loops++;
#if !F407_TELEMETRY_BINARY_TX
        {
            char line[320];
            (void)snprintf(
                line, sizeof(line),
                "rtos health tick=%lu heap=%lu loops=%lu/%lu/%lu/%lu/%lu "
                "stack=%lu/%lu/%lu/%lu/%lu dq=%lu/%lu/%lu tq=%lu/%lu/%lu "
                "sensor=%lu/%lu\r\n",
                (unsigned long)xTaskGetTickCount(),
                (unsigned long)free_heap,
                (unsigned long)heartbeat_task_loops,
                (unsigned long)sensor_task_loops,
                (unsigned long)display_task_loops,
                (unsigned long)telemetry_task_loops,
                (unsigned long)health_task_loops,
                (unsigned long)stack_heartbeat,
                (unsigned long)stack_sensor,
                (unsigned long)stack_display,
                (unsigned long)stack_telemetry,
                (unsigned long)stack_health,
                (unsigned long)display_depth,
                (unsigned long)display_queue_peak,
                (unsigned long)display_queue_fail,
                (unsigned long)telemetry_depth,
                (unsigned long)telemetry_queue_peak,
                (unsigned long)telemetry_queue_fail,
                (unsigned long)sensor_ok,
                (unsigned long)sensor_fail);
            uart_write(line);
        }
#else
        /* Binary telemetry must not share diagnostic text on USART1. Keep the
         * observer active, but mark its silent snapshot as intentionally used. */
        (void)display_depth;
        (void)telemetry_depth;
        (void)stack_heartbeat;
        (void)stack_sensor;
        (void)stack_display;
        (void)stack_telemetry;
        (void)stack_health;
        (void)free_heap;
#endif
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(HEALTH_PERIOD_MS));
    }
}

#if F407_TELEMETRY_BINARY_TX
static void telemetry_send_binary(const SensorFrame *frame)
{
    uint8_t encoded[TELEMETRY_FRAME_MAX_SIZE];
    size_t length = 0U;

    if ((frame != NULL) &&
        (telemetry_bridge_encode_sensor_frame(frame, encoded, sizeof(encoded),
                                              &length) == 0))
    {
        uart_write_bytes(encoded, length);
    }
    else
    {
        telemetry_binary_fail++;
    }
}
#endif

#if !F407_TELEMETRY_BINARY_TX
static void format_fixed(char *buffer, size_t size, int32_t value)
{
    int64_t magnitude = value < 0 ? -(int64_t)value : (int64_t)value;
    int64_t whole = magnitude / 100;
    uint32_t fraction = (uint32_t)(magnitude % 100);
    (void)snprintf(buffer, size, "%s%ld.%02lu", value < 0 ? "-" : "",
                   (long)whole, (unsigned long)fraction);
}
#endif

static int bmp_read(void *context, uint8_t reg, uint8_t *data, uint16_t length)
{
    I2C_HandleTypeDef *bus = (I2C_HandleTypeDef *)context;
    return HAL_I2C_Mem_Read(bus, (uint16_t)(bmp_i2c_address << 1), reg,
                            I2C_MEMADD_SIZE_8BIT, data, length, 100U) == HAL_OK
               ? 0 : -1;
}

static int bmp_write(void *context, uint8_t reg, const uint8_t *data,
                     uint16_t length)
{
    I2C_HandleTypeDef *bus = (I2C_HandleTypeDef *)context;
    return HAL_I2C_Mem_Write(bus, (uint16_t)(bmp_i2c_address << 1), reg,
                             I2C_MEMADD_SIZE_8BIT, (uint8_t *)data, length, 100U) == HAL_OK
               ? 0 : -1;
}

static int aht_read(void *context, uint8_t *data, uint16_t length)
{
    I2C_HandleTypeDef *bus = (I2C_HandleTypeDef *)context;
    return HAL_I2C_Master_Receive(bus, (uint16_t)(AHT20_I2C_ADDRESS << 1),
                                  data, length, 100U) == HAL_OK ? 0 : -1;
}

static int aht_write(void *context, const uint8_t *data, uint16_t length)
{
    I2C_HandleTypeDef *bus = (I2C_HandleTypeDef *)context;
    return HAL_I2C_Master_Transmit(bus, (uint16_t)(AHT20_I2C_ADDRESS << 1),
                                   (uint8_t *)data, length, 100U) == HAL_OK ? 0 : -1;
}

static void aht_delay(void *context, uint32_t milliseconds)
{
    (void)context;
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

static int tft_write_bytes(void *context, const uint8_t *bytes,
                           uint16_t length)
{
    SPI_HandleTypeDef *spi = (SPI_HandleTypeDef *)context;
    if ((spi == NULL) || (bytes == NULL) || (length == 0U) ||
        (HAL_SPI_Transmit(spi, (uint8_t *)bytes, length, 100U) != HAL_OK))
    {
        tft_spi_fail++;
        return -1;
    }
    return 0;
}

static void tft_write_cs(void *context, uint8_t level)
{
    (void)context;
    HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN,
                      level != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void tft_write_dc(void *context, uint8_t level)
{
    (void)context;
    HAL_GPIO_WritePin(TFT_DC_PORT, TFT_DC_PIN,
                      level != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void tft_write_rst(void *context, uint8_t level)
{
    (void)context;
    HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN,
                      level != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void tft_write_bl(void *context, uint8_t level)
{
    (void)context;
#if TFT_BL_ACTIVE_HIGH
    HAL_GPIO_WritePin(TFT_BL_PORT, TFT_BL_PIN,
                      level != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
    HAL_GPIO_WritePin(TFT_BL_PORT, TFT_BL_PIN,
                      level != 0U ? GPIO_PIN_RESET : GPIO_PIN_SET);
#endif
}

static void tft_delay_ms(void *context, uint32_t milliseconds)
{
    (void)context;
    HAL_Delay(milliseconds);
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clocks = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();
    clocks.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clocks.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clocks.APB1CLKDivider = RCC_HCLK_DIV1;
    clocks.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_0) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin = TFT_CS_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = TFT_RST_PIN | TFT_DC_PIN | TFT_BL_PIN | USER_LED_Pin;
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = KEY1_PIN | KEY2_PIN | KEY3_PIN | KEY4_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);

    HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TFT_DC_PORT, TFT_DC_PIN, GPIO_PIN_SET);
#if TFT_BL_ACTIVE_HIGH
    HAL_GPIO_WritePin(TFT_BL_PORT, TFT_BL_PIN, GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(TFT_BL_PORT, TFT_BL_PIN, GPIO_PIN_RESET);
#endif
    HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, GPIO_PIN_RESET);
}

static void MX_SPI1_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10U;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_I2C2_Init(void)
{
    hi2c2.Instance = I2C2;
    hi2c2.Init.ClockSpeed = 100000U;
    hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c2.Init.OwnAddress1 = 0U;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0U;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c2) != HAL_OK) Error_Handler();
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200U;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
}

static void uart_write(const char *text)
{
    BaseType_t locked = pdTRUE;
    if ((uart_mutex != NULL) &&
        (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED))
    {
        locked = xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(1000U));
    }
    if (locked == pdTRUE)
    {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)text,
                                (uint16_t)strlen(text), HAL_MAX_DELAY);
        if ((uart_mutex != NULL) &&
            (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED))
        {
            xSemaphoreGive(uart_mutex);
        }
    }
}

#if F407_TELEMETRY_BINARY_TX
static void uart_write_bytes(const uint8_t *data, size_t length)
{
    BaseType_t locked = pdTRUE;
    if ((data == NULL) || (length == 0U) || (length > UINT16_MAX))
    {
        telemetry_binary_fail++;
        return;
    }
    if ((uart_mutex != NULL) &&
        (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED))
    {
        locked = xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(1000U));
    }
    if (locked == pdTRUE)
    {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)data, (uint16_t)length,
                                HAL_MAX_DELAY);
        if ((uart_mutex != NULL) &&
            (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED))
        {
            xSemaphoreGive(uart_mutex);
        }
    }
    else
    {
        telemetry_binary_fail++;
    }
}
#endif

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
        HAL_GPIO_TogglePin(USER_LED_GPIO_Port, USER_LED_Pin);
        for (volatile uint32_t i = 0U; i < 160000U; ++i) __NOP();
    }
}

void vApplicationMallocFailedHook(void)
{
    Error_Handler();
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task;
    (void)task_name;
    Error_Handler();
}

#if F407_ENABLE_IWDG
static void MX_IWDG_Init(void)
{
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
    hiwdg.Init.Reload = 2500U;
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
    {
        Error_Handler();
    }
}
#endif
