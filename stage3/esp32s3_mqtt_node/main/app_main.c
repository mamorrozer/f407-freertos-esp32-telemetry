#include "driver/uart.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "telemetry_protocol.h"
#include "telemetry_cache.h"
#include "telemetry_config.h"
#include "telemetry_faults.h"
#include "telemetry_delivery.h"
#include "telemetry_nvs.h"
#include "telemetry_stats.h"
#include "telemetry_text.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define UART_PORT UART_NUM_1
#define UART_TX_GPIO 17
#define UART_RX_GPIO 18
#define UART_BAUD_RATE 115200
#define UART_RX_BUFFER_SIZE 2048
#define UART_PARTIAL_FRAME_TIMEOUT_MS 100U
#define LINE_BUFFER_SIZE 256
#define WIFI_CONNECTED_BIT BIT0
#define MQTT_CONNECTED_BIT BIT1
#define MQTT_EVENT_QUEUE_LENGTH 16U
#define MQTT_DELIVERY_ACK_TIMEOUT_MS 30000U

typedef enum
{
    MQTT_APP_EVENT_PUBLISHED,
    MQTT_APP_EVENT_DISCONNECTED,
    MQTT_APP_EVENT_DELETED
} MqttAppEventType;

typedef struct
{
    MqttAppEventType type;
    int message_id;
} MqttAppEvent;

static const char *TAG = "telemetry_node";
static EventGroupHandle_t connection_events;
static esp_mqtt_client_handle_t mqtt_client;
static TelemetryProtocolDecoder binary_decoder;
static TelemetryCache pending_cache;
static SemaphoreHandle_t cache_mutex;
static QueueHandle_t mqtt_event_queue;
static TelemetryRuntimeStats runtime_stats;
static portMUX_TYPE runtime_stats_mux = portMUX_INITIALIZER_UNLOCKED;
static TelemetryDeliveryState delivery_state;
static TelemetryConfig runtime_config;
static TelemetryFaultPlan fault_plan;
static TelemetryFaultCounters fault_counters;

#define STATS_INCREMENT(field)                  \
    do                                          \
    {                                           \
        portENTER_CRITICAL(&runtime_stats_mux); \
        runtime_stats.field++;                  \
        portEXIT_CRITICAL(&runtime_stats_mux);  \
    } while (0)

static TelemetryRuntimeStats runtime_stats_snapshot(void)
{
    TelemetryRuntimeStats snapshot;
    portENTER_CRITICAL(&runtime_stats_mux);
    snapshot = runtime_stats;
    portEXIT_CRITICAL(&runtime_stats_mux);
    return snapshot;
}

static bool network_ready(void)
{
    const EventBits_t required = WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT;
    return (xEventGroupGetBits(connection_events) & required) == required;
}

static void cache_frame_locked(const TelemetryFrame *frame)
{
    TelemetryCacheRecord record = {
        .sequence = frame->sequence,
        .sensor = frame->sensor,
    };
    /* 证据边界：这是运行期 RAM FIFO；断网可暂存，但满时会丢弃，
     * ESP32 复位或断电后也不会保留。它不是 SD 卡或掉电持久化存储。 */
    if (telemetry_cache_push(&pending_cache, &record) != 0)
    {
        STATS_INCREMENT(cache_dropped);
        ESP_LOGW(TAG, "RAM cache full, dropped seq=%lu",
                 (unsigned long)frame->sequence);
    }
    else
    {
        STATS_INCREMENT(cache_pushed);
    }
}

static int publish_payload(const char *payload)
{
    uint32_t delay_ms = 0U;

    if (telemetry_faults_apply(&fault_plan, &fault_counters, NULL, 0U,
                               &delay_ms) == 1)
    {
        STATS_INCREMENT(publish_fail);
        ESP_LOGW(TAG, "fault injection dropped publish");
        return -1;
    }
    if (delay_ms != 0U)
    {
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    if (network_ready())
    {
        /* Wi-Fi 提供网络承载，MQTT 在其上完成发布；两者不是二选一。
         * QoS=1 表示请求至少一次投递，后续仍要等待 PUBACK 才能确认本地缓存。 */
        int message_id = esp_mqtt_client_publish(
            mqtt_client, CONFIG_TELEMETRY_MQTT_TOPIC, payload, 0, 1, 0);
        if (message_id >= 0)
        {
            STATS_INCREMENT(publish_accepted);
            return message_id;
        }
    }
    STATS_INCREMENT(publish_fail);
    return -1;
}

static void publish_frame(const TelemetryFrame *frame)
{
    char payload[256];

    portENTER_CRITICAL(&runtime_stats_mux);
    telemetry_stats_observe_sequence(&runtime_stats, frame->sequence);
    portEXIT_CRITICAL(&runtime_stats_mux);

    if (!telemetry_text_format_json(frame, payload, sizeof(payload)))
    {
        STATS_INCREMENT(parse_failures);
        return;
    }

    if (xSemaphoreTake(cache_mutex, pdMS_TO_TICKS(1000U)) != pdTRUE)
    {
        STATS_INCREMENT(publish_fail);
        return;
    }
    cache_frame_locked(frame);
    xSemaphoreGive(cache_mutex);
}

static bool process_next_mqtt_app_event(void)
{
    MqttAppEvent event;
    TelemetryCacheRecord cached;
    uint32_t acknowledged_sequence = 0U;
    TelemetryDeliveryAckResult ack_result;

    if (xQueuePeek(mqtt_event_queue, &event, 0U) != pdPASS)
    {
        return false;
    }
    if (event.type != MQTT_APP_EVENT_PUBLISHED)
    {
        (void)xQueueReceive(mqtt_event_queue, &event, 0U);
        if (event.type == MQTT_APP_EVENT_DISCONNECTED)
        {
            (void)telemetry_delivery_on_disconnect(&delivery_state);
        }
        else if (event.type == MQTT_APP_EVENT_DELETED &&
                 telemetry_delivery_on_deleted(&delivery_state,
                                               event.message_id))
        {
            STATS_INCREMENT(publish_expired);
            ESP_LOGW(TAG, "MQTT outbox expired seq=%lu; will retry",
                     (unsigned long)delivery_state.sequence);
        }
        return true;
    }

    /* 学习点：先 peek 队列中的 ACK，不立即丢掉；缓存锁暂时不可用时保留事件。
     * 只有 message_id 匹配并且缓存头序号一致，才允许 pop。 */
    /* Keep the ACK queued if cache access is temporarily unavailable. */
    if (xSemaphoreTake(cache_mutex, 0U) != pdTRUE)
    {
        return false;
    }
    (void)xQueueReceive(mqtt_event_queue, &event, 0U);
    ack_result = telemetry_delivery_on_puback(&delivery_state,
                                               event.message_id,
                                               &acknowledged_sequence);
    if (ack_result == TELEMETRY_DELIVERY_ACK_MATCHED)
    {
        if ((telemetry_cache_peek(&pending_cache, &cached) == 0) &&
            (cached.sequence == acknowledged_sequence))
        {
            /* 先收到 PUBACK，再删除 FIFO 头；否则网络短暂异常会造成过早丢数据。 */
            (void)telemetry_cache_pop(&pending_cache, &cached);
            STATS_INCREMENT(publish_ok);
            STATS_INCREMENT(cache_flushed);
        }
        else
        {
            ESP_LOGE(TAG,
                     "PUBACK matched seq=%lu but cache head did not; retaining cache",
                     (unsigned long)acknowledged_sequence);
        }
    }
    xSemaphoreGive(cache_mutex);
    return true;
}

static void process_mqtt_app_events(void)
{
    while (process_next_mqtt_app_event())
    {
    }
}

static bool publish_one_cached_frame(void)
{
    TelemetryCacheRecord cached;
    TelemetryFrame frame;
    char payload[256];
    int message_id;

    /* 一次只跟踪一个 in-flight 消息，换取简单且可解释的 FIFO 顺序。
     * 代价是吞吐量受 ACK 往返时间限制。 */
    if (!network_ready() || delivery_state.in_flight ||
        (xSemaphoreTake(cache_mutex, pdMS_TO_TICKS(1000U)) != pdTRUE))
    {
        return false;
    }
    if (telemetry_cache_peek(&pending_cache, &cached) != 0)
    {
        xSemaphoreGive(cache_mutex);
        return false;
    }
    xSemaphoreGive(cache_mutex);

    frame.version = TELEMETRY_FRAME_VERSION;
    frame.type = TELEMETRY_FRAME_TYPE_SENSOR;
    frame.sequence = cached.sequence;
    frame.sensor = cached.sensor;
    if (!telemetry_text_format_json(&frame, payload, sizeof(payload)))
    {
        STATS_INCREMENT(parse_failures);
        return false;
    }

    message_id = publish_payload(payload);
    if (message_id < 0)
    {
        return false;
    }
    if (telemetry_delivery_begin(&delivery_state, message_id,
                                 cached.sequence,
                                 (uint32_t)(esp_timer_get_time() / 1000)) !=
        TELEMETRY_DELIVERY_ACCEPTED)
    {
        ESP_LOGE(TAG, "could not track accepted MQTT message id=%d", message_id);
        return false;
    }
    return true;
}

static void cache_flush_task(void *argument)
{
    (void)argument;
    for (;;)
    {
        process_mqtt_app_events();
        /* 超时只释放 in-flight 状态，不删除 FIFO 头；下一轮会重试同一序号。 */
        if (telemetry_delivery_retry_if_expired(
                &delivery_state,
                (uint32_t)(esp_timer_get_time() / 1000),
                MQTT_DELIVERY_ACK_TIMEOUT_MS))
        {
            ESP_LOGW(TAG,
                     "MQTT delivery ACK timeout; retaining FIFO head for retry");
        }
        if (!network_ready())
        {
            (void)telemetry_delivery_on_disconnect(&delivery_state);
            vTaskDelay(pdMS_TO_TICKS(250U));
            continue;
        }
        if (delivery_state.in_flight)
        {
            vTaskDelay(pdMS_TO_TICKS(50U));
            continue;
        }
        if (!publish_one_cached_frame())
        {
            vTaskDelay(pdMS_TO_TICKS(100U));
        }
    }
}

#if CONFIG_TELEMETRY_REMOTE_CONFIG
static void publish_config_ack(TelemetryConfigResult result)
{
    char payload[192];

    (void)snprintf(payload, sizeof(payload),
                   "result=%d;period_ms=%lu;temp_high=%ld;humidity_high=%lu;alarm=%u",
                   (int)result,
                   (unsigned long)runtime_config.sample_period_ms,
                   (long)runtime_config.temperature_high_centi_c,
                   (unsigned long)runtime_config.humidity_high_centi_percent,
                   runtime_config.alarm_enabled ? 1U : 0U);
    (void)esp_mqtt_client_publish(mqtt_client,
                                  CONFIG_TELEMETRY_CONFIG_ACK_TOPIC,
                                  payload, 0, 1, 0);
}

static void handle_config_command(const esp_mqtt_event_handle_t event)
{
    char command[192];
    TelemetryConfig candidate = runtime_config;
    TelemetryConfigResult result;

    if ((event->data == NULL) || (event->data_len <= 0) ||
        ((size_t)event->data_len >= sizeof(command)))
    {
        publish_config_ack(TELEMETRY_CONFIG_INVALID_VALUE);
        return;
    }
    memcpy(command, event->data, (size_t)event->data_len);
    command[event->data_len] = '\0';
    result = telemetry_config_parse_command(command, &candidate);
    if (result == TELEMETRY_CONFIG_OK)
    {
        esp_err_t save_result = telemetry_nvs_save(&candidate);
        if (save_result != ESP_OK)
        {
            ESP_LOGW(TAG, "configuration save failed: %s",
                     esp_err_to_name(save_result));
            result = TELEMETRY_CONFIG_INVALID_VALUE;
        }
        else
        {
            runtime_config = candidate;
        }
    }
    publish_config_ack(result);
}
#endif

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    (void)handler_args;
    (void)base;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        STATS_INCREMENT(mqtt_connects);
        xEventGroupSetBits(connection_events, MQTT_CONNECTED_BIT);
#if CONFIG_TELEMETRY_REMOTE_CONFIG
        (void)esp_mqtt_client_subscribe(mqtt_client,
                                        CONFIG_TELEMETRY_CONFIG_TOPIC, 1);
#endif
        ESP_LOGI(TAG, "MQTT connected");
        break;
    case MQTT_EVENT_DISCONNECTED:
        STATS_INCREMENT(mqtt_disconnects);
        xEventGroupClearBits(connection_events, MQTT_CONNECTED_BIT);
        {
            const MqttAppEvent app_event = {
                .type = MQTT_APP_EVENT_DISCONNECTED,
                .message_id = -1,
            };
            if (xQueueSend(mqtt_event_queue, &app_event, 0U) != pdPASS)
            {
                ESP_LOGE(TAG,
                         "MQTT event queue full; network state/ACK timeout will recover delivery");
            }
        }
        ESP_LOGW(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_PUBLISHED:
        {
            const MqttAppEvent app_event = {
                .type = MQTT_APP_EVENT_PUBLISHED,
                .message_id = event->msg_id,
            };
            if (xQueueSend(mqtt_event_queue, &app_event, 0U) != pdPASS)
            {
                ESP_LOGE(TAG,
                         "PUBACK queue full; pending delivery will retry after ACK timeout");
            }
        }
        break;
    case MQTT_EVENT_DELETED:
        {
            const MqttAppEvent app_event = {
                .type = MQTT_APP_EVENT_DELETED,
                .message_id = event->msg_id,
            };
            if (xQueueSend(mqtt_event_queue, &app_event, 0U) != pdPASS)
            {
                ESP_LOGE(TAG,
                         "outbox deletion event queue full; pending delivery will retry after ACK timeout");
            }
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "MQTT transport error");
        break;
    case MQTT_EVENT_DATA:
#if CONFIG_TELEMETRY_REMOTE_CONFIG
        if ((event->topic != NULL) &&
            ((size_t)event->topic_len == strlen(CONFIG_TELEMETRY_CONFIG_TOPIC)) &&
            (memcmp(event->topic, CONFIG_TELEMETRY_CONFIG_TOPIC,
                    (size_t)event->topic_len) == 0))
        {
            handle_config_command(event);
        }
#endif
        break;
    default:
        break;
    }
    (void)event;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        STATS_INCREMENT(wifi_reconnects);
        xEventGroupClearBits(connection_events, WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT);
        esp_wifi_connect();
        ESP_LOGW(TAG, "Wi-Fi disconnected, reconnect requested");
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupSetBits(connection_events, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Wi-Fi got IP");
    }
}

static void wifi_start(void)
{
    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, CONFIG_TELEMETRY_WIFI_SSID,
            sizeof(wifi_config.sta.ssid) - 1U);
    strncpy((char *)wifi_config.sta.password, CONFIG_TELEMETRY_WIFI_PASSWORD,
            sizeof(wifi_config.sta.password) - 1U);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void mqtt_start(void)
{
    const esp_mqtt_client_config_t config = {
        .broker.address.uri = CONFIG_TELEMETRY_MQTT_URI,
    };
    mqtt_client = esp_mqtt_client_init(&config);
    ESP_ERROR_CHECK(mqtt_client != NULL ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                                    mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
}

static void uart_start(void)
{
    const uart_config_t config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_RX_BUFFER_SIZE, 0,
                                        0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_GPIO, UART_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

static void uart_line_task(void *argument)
{
    uint8_t byte;
    char line[LINE_BUFFER_SIZE];
    size_t length = 0U;
    bool line_overflow = false;
    TickType_t last_byte_tick = 0U;
    const TickType_t partial_frame_timeout =
        pdMS_TO_TICKS(UART_PARTIAL_FRAME_TIMEOUT_MS);
    (void)argument;
    telemetry_decoder_init(&binary_decoder);
    for (;;)
    {
        int received = uart_read_bytes(UART_PORT, &byte, 1U, pdMS_TO_TICKS(1000U));
        if (received <= 0)
        {
            continue;
        }
        STATS_INCREMENT(uart_bytes_received);
        {
            TickType_t now = xTaskGetTickCount();
        /* UART 是连续字节流，不保证一次 read 就得到完整帧。
         * 超时清掉半帧，让后续同步字节重新建立边界。 */
        if ((binary_decoder.length != 0U) &&
                ((TickType_t)(now - last_byte_tick) >= partial_frame_timeout))
            {
                telemetry_decoder_reset_partial(&binary_decoder);
                length = 0U;
                line_overflow = false;
                ESP_LOGW(TAG, "UART partial frame timed out; decoder resynchronized");
            }
            last_byte_tick = now;
        }
        if ((binary_decoder.length != 0U) ||
            (byte == TELEMETRY_FRAME_SYNC0))
        {
            TelemetryFrame binary_frame;
            if (binary_decoder.length == 0U)
            {
                /* A binary frame may begin after a noisy/truncated text fragment. */
                length = 0U;
                line_overflow = false;
            }
            TelemetryProtocolResult protocol_result = telemetry_decoder_feed(
                &binary_decoder, byte, &binary_frame);
            if (protocol_result == TELEMETRY_PROTOCOL_FRAME)
            {
                publish_frame(&binary_frame);
                continue;
            }
            if (protocol_result == TELEMETRY_PROTOCOL_CRC_ERROR)
            {
                STATS_INCREMENT(crc_errors);
                length = 0U;
                ESP_LOGW(TAG, "binary CRC error, count=%lu",
                         (unsigned long)runtime_stats_snapshot().crc_errors);
                continue;
            }
            if (protocol_result == TELEMETRY_PROTOCOL_FORMAT_ERROR)
            {
                STATS_INCREMENT(format_errors);
                length = 0U;
                ESP_LOGW(TAG, "binary format error, count=%lu",
                         (unsigned long)runtime_stats_snapshot().format_errors);
                continue;
            }
            continue;
        }
        if (byte == '\n')
        {
            TelemetryFrame text_frame;
            line[length] = '\0';
            if (line_overflow)
            {
                line_overflow = false;
                length = 0U;
                continue;
            }
            if (telemetry_text_parse_sensor_line(line, &text_frame))
            {
                publish_frame(&text_frame);
            }
            else
            {
                STATS_INCREMENT(parse_failures);
                ESP_LOGW(TAG, "ignored line, parse_fail=%lu",
                         (unsigned long)runtime_stats_snapshot().parse_failures);
            }
            length = 0U;
        }
        else if (byte != '\r')
        {
            if (line_overflow)
            {
                continue;
            }
            if (length + 1U < sizeof(line))
            {
                line[length++] = (char)byte;
            }
            else
            {
                length = 0U;
                line_overflow = true;
                STATS_INCREMENT(parse_failures);
                ESP_LOGW(TAG, "line too long, parse_fail=%lu",
                         (unsigned long)runtime_stats_snapshot().parse_failures);
            }
        }
    }
}

#if defined(CONFIG_TELEMETRY_DEMO_PUBLISH) && CONFIG_TELEMETRY_DEMO_PUBLISH
static void demo_publish_task(void *argument)
{
    uint32_t sequence = 0U;
    (void)argument;

    for (;;)
    {
        TelemetryFrame frame = {
            .version = TELEMETRY_FRAME_VERSION,
            .type = TELEMETRY_FRAME_TYPE_SENSOR,
            .sequence = ++sequence,
            .sensor = {
                .timestamp_ms = (uint32_t)xTaskGetTickCount(),
                .temperature_centi_c = 2500,
                .humidity_centi_percent = 6000U,
                .pressure_pa = 100653U,
                .valid = 0x03U,
            },
        };
        char payload[256];
        if (telemetry_text_format_json(&frame, payload, sizeof(payload)))
        {
            publish_frame(&frame);
            TelemetryRuntimeStats snapshot = runtime_stats_snapshot();
            ESP_LOGI(TAG, "demo publish seq=%lu accepted=%lu puback=%lu fail=%lu",
                     (unsigned long)sequence,
                     (unsigned long)snapshot.publish_accepted,
                     (unsigned long)snapshot.publish_ok,
                     (unsigned long)snapshot.publish_fail);
        }
        vTaskDelay(pdMS_TO_TICKS(2000U));
    }
}
#endif

static void health_report_task(void *argument)
{
    (void)argument;
#if CONFIG_TELEMETRY_ENABLE_TASK_WDT
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
#endif
    for (;;)
    {
        TelemetryRuntimeStats snapshot;
        uint32_t cache_count = UINT32_MAX;
        if (xSemaphoreTake(cache_mutex, pdMS_TO_TICKS(100U)) == pdTRUE)
        {
            cache_count = (uint32_t)telemetry_cache_count(&pending_cache);
            snapshot = runtime_stats_snapshot();
            xSemaphoreGive(cache_mutex);
        }
        else
        {
            snapshot = runtime_stats_snapshot();
        }
        /* 健康日志是运行证据：parse/crc/format、缓存丢弃和重连次数
         * 必须结合时间窗口解释，单次打印不能证明长期稳定。 */
        ESP_LOGI(TAG,
                 "health uart_bytes=%lu rx=%lu gaps=%lu parse=%lu crc=%lu format=%lu pub_accept=%lu pub_ok=%lu pub_fail=%lu pub_expired=%lu cache=%u pushed=%lu dropped=%lu flushed=%lu wifi_reconnect=%lu mqtt=%lu/%lu",
                 (unsigned long)snapshot.uart_bytes_received,
                 (unsigned long)snapshot.frames_received,
                 (unsigned long)snapshot.sequence_gaps,
                 (unsigned long)snapshot.parse_failures,
                 (unsigned long)snapshot.crc_errors,
                 (unsigned long)snapshot.format_errors,
                 (unsigned long)snapshot.publish_accepted,
                 (unsigned long)snapshot.publish_ok,
                 (unsigned long)snapshot.publish_fail,
                 (unsigned long)snapshot.publish_expired,
                 (unsigned)cache_count,
                 (unsigned long)snapshot.cache_pushed,
                 (unsigned long)snapshot.cache_dropped,
                 (unsigned long)snapshot.cache_flushed,
                 (unsigned long)snapshot.wifi_reconnects,
                 (unsigned long)snapshot.mqtt_connects,
                 (unsigned long)snapshot.mqtt_disconnects);
#if CONFIG_TELEMETRY_ENABLE_TASK_WDT
        ESP_ERROR_CHECK(esp_task_wdt_reset());
#endif
        vTaskDelay(pdMS_TO_TICKS(5000U));
    }
}

void app_main(void)
{
    esp_err_t nvs_result = nvs_flash_init();
    if ((nvs_result == ESP_ERR_NVS_NO_FREE_PAGES) ||
        (nvs_result == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_result);
    connection_events = xEventGroupCreate();
    ESP_ERROR_CHECK(connection_events != NULL ? ESP_OK : ESP_FAIL);
    telemetry_cache_init(&pending_cache);
    telemetry_stats_init(&runtime_stats);
    telemetry_delivery_init(&delivery_state);
    telemetry_faults_init(&fault_plan, &fault_counters);
#if CONFIG_TELEMETRY_FAULT_INJECTION
    fault_plan.drop_next = CONFIG_TELEMETRY_FAULT_DROP_FIRST;
    fault_plan.delay_ms = CONFIG_TELEMETRY_FAULT_DELAY_MS;
#endif
    telemetry_config_defaults(&runtime_config);
#if CONFIG_TELEMETRY_REMOTE_CONFIG
    if (telemetry_nvs_load(&runtime_config) != ESP_OK)
    {
        (void)telemetry_nvs_save(&runtime_config);
    }
#endif
    cache_mutex = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(cache_mutex != NULL ? ESP_OK : ESP_FAIL);
    mqtt_event_queue = xQueueCreate(MQTT_EVENT_QUEUE_LENGTH,
                                    sizeof(MqttAppEvent));
    ESP_ERROR_CHECK(mqtt_event_queue != NULL ? ESP_OK : ESP_FAIL);
    uart_start();
    wifi_start();
    mqtt_start();
    if (xTaskCreate(uart_line_task, "uart_line", 4096U, NULL, 5U, NULL) != pdPASS)
    {
        ESP_ERROR_CHECK(ESP_FAIL);
    }
    if (xTaskCreate(cache_flush_task, "cache_flush", 3072U, NULL, 4U, NULL) != pdPASS)
    {
        ESP_ERROR_CHECK(ESP_FAIL);
    }
    if (xTaskCreate(health_report_task, "health_report", 3072U, NULL, 2U, NULL) != pdPASS)
    {
        ESP_ERROR_CHECK(ESP_FAIL);
    }
#if defined(CONFIG_TELEMETRY_DEMO_PUBLISH) && CONFIG_TELEMETRY_DEMO_PUBLISH
    if (xTaskCreate(demo_publish_task, "demo_publish", 3072U, NULL, 4U, NULL) != pdPASS)
    {
        ESP_ERROR_CHECK(ESP_FAIL);
    }
#endif
    ESP_LOGI(TAG, "ESP32-S3 MQTT node started; UART1 TX=%d RX=%d",
             UART_TX_GPIO, UART_RX_GPIO);
}
