#include "app_uart.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "modbus//tcp/tcp_slave.h"
#include "wifi/wifi_module.h"
#include "esp_log.h"
#include "portmacro.h"
#include "projdefs.h"
#include "uart.h"

#define BUF_SIZE 1024

static void app_uart_receive_event_task(void * pvParameters);
static void reset_frame_buffer(uart_buffer_t *frame_buffer);
static void distribution_command(uart_buffer_t *frame_buffer);
static bool is_data_broken(uint8_t data[], uint16_t len);
static void receive_one_frame_operation(uart_buffer_t *frame_buffer);
static void timer_one_frame_callback(TimerHandle_t x_timer);
static void uart_reassemble_frame(uart_event_t event, uart_buffer_t *frame_buffer);
static bool is_end_of_receive(uint8_t data[], uint16_t len);
static void handle_wifi_command(uart_buffer_t *frame_buffer);
static void handle_receive_temp_and_humid(uart_buffer_t *frame_buffer);

static TimerHandle_t idle_timer;

struct uart_buffer_t {
    uint8_t data[BUF_SIZE];
    uint16_t len;
    uint16_t pos;
    bool full_frame_received;
};
uart_buffer_t frame_buffer = {
    .data = {0}, .len = 0, .pos = 0, .full_frame_received = false
};

static const char kTag[] = "APP_UART";
static QueueHandle_t uart0_queue;

void app_uart_init(void) {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, BUF_SIZE, BUF_SIZE, 100,
                                        &uart0_queue, 0));
    idle_timer = xTimerCreate("idle_uart", pdMS_TO_TICKS(10), pdFALSE, NULL,
                              timer_one_frame_callback);
    if (idle_timer == NULL) {
        ESP_LOGE(kTag, "Failed to create idle timer");
        return;
    } else {
        ESP_LOGI(kTag, "Idle timer created successfully");
    }
    xTaskCreate(app_uart_receive_event_task, "app_uart_receive_event_task",
                2048, NULL, 3, NULL);
}

/**
 * @brief 用于实现类似STM32的UART的IDLE超时功能
 * 
 * @param pvParameters 
 */
static void app_uart_receive_event_task(void *pvParameters) {
    uart_event_t event;
    while (1) {
        if (event.type != UART_DATA) {
            continue;
        }
        /* 若超过1秒没受到则视为接受完一帧  */
        if (xQueueReceive(uart0_queue, (void *)&event, pdMS_TO_TICKS(1000))) {
            xTimerReset(idle_timer, 0);
            uart_reassemble_frame(event, &frame_buffer);
            ESP_LOGI(kTag, "[UART DATA]: %d", event.size);
        } else {
            if (frame_buffer.len == 0) {
                // ESP_LOGI(kTag, "No data received, waiting for next frame");
                continue;
            }
            receive_one_frame_operation(&frame_buffer);
        }
    }
}

static inline void uart_reassemble_frame(uart_event_t event,
                                         uart_buffer_t *frame_buffer) {
    uint16_t received_len = event.size;
    if (frame_buffer->len + event.size > BUF_SIZE) {
        ESP_LOGE(kTag, "Buffer overflow: truncating received framed");
        received_len = BUF_SIZE - frame_buffer->len;
        frame_buffer->full_frame_received = true;
        xTimerStop(idle_timer, 0);
    }
    uart_read_bytes(UART_NUM_0, &frame_buffer->data[frame_buffer->pos],
                    received_len, portMAX_DELAY);
    frame_buffer->len += received_len;
    frame_buffer->pos += received_len;
    for (int i = 0; i < received_len; i++) {
        ESP_LOGI(kTag, "Received byte: 0x%02X", frame_buffer->data[i]);
    }
}

static void receive_one_frame_operation(uart_buffer_t *buffer) {
    ESP_LOGI(kTag, "Full frame received, processing...");
    if (is_data_broken(buffer->data, buffer->len)) {
        ESP_LOGI(kTag, "Received frame is broken, ignoring");
        reset_frame_buffer(buffer);
        return;
    } else {
        static char ack_msg[] = "ACK\r\n";
        uart_write_bytes(UART_NUM_0, ack_msg, strlen(ack_msg));
        return;
    }
    if (!is_end_of_receive(buffer->data, buffer->len)) {
        ESP_LOGI(kTag, "Received frame does not end with CRLF, ignoring");
        reset_frame_buffer(buffer);
        return;
    }

    distribution_command(buffer);
    reset_frame_buffer(buffer);
}

/**
 * @brief 根据第一个参数的值来分发命令
 * 
 * @param frame_buffer 
 */
static void distribution_command(uart_buffer_t *frame_buffer) {
    ESP_LOGI(kTag, "Received complete frame: %.*s", frame_buffer->len,
             frame_buffer->data);
    uint8_t command = frame_buffer->data[0];
    switch (command) {
        case 0x00:
            ESP_LOGI(kTag, "Processing command 0x00");
            handle_wifi_command(frame_buffer);
            break;
        case 0x01:
            ESP_LOGI(kTag, "Processing command 0x01");
            handle_receive_temp_and_humid(frame_buffer);
            // Add your command processing logic here
            break;
        default:
            ESP_LOGW(kTag, "Unknown command: 0x%02X", command);
            break;
    }
}

static void handle_wifi_command(uart_buffer_t *frame_buffer) {
    uint8_t ssid_len =  frame_buffer->data[2];
    uint8_t password_len = frame_buffer->data[3];
    char ssid[32] = {0};
    char password[64] = {0};
    uint8_t set_index = 4;
    memcpy(ssid, &frame_buffer->data[set_index], ssid_len);
    ssid[ssid_len] = '\0';
    set_index += ssid_len;
    memcpy(password, &frame_buffer->data[set_index], password_len);
    password[password_len] = '\0';
    wifi_set_new_config(ssid, password);
}

/**
 * @brief 处理接收到的温湿度数据
 * 格式为：0x01 checksum temperature(4 bytes) humidity(4 bytes) \r\n
 *
 * @param frame_buffer
 */
static void handle_receive_temp_and_humid(uart_buffer_t *frame_buffer) {
    if (frame_buffer->len != 12) {
        ESP_LOGE(kTag, "Invalid frame length for temperature and humidity data: %d",
                 frame_buffer->len);
        return;
    }
    float temperature = 0.0f;
    float humidity = 0.0f;
    memcpy(&temperature, &frame_buffer->data[2], sizeof(float));
    memcpy(&humidity, &frame_buffer->data[6], sizeof(float));
    ESP_LOGI(kTag, "Received temperature: %.2f, humidity: %.2f", temperature,
             humidity);
    modbus_update_temp_and_humi(temperature, humidity);
}

static void reset_frame_buffer(uart_buffer_t *frame_buffer) {
    frame_buffer->len = 0;
    frame_buffer->pos = 0;
    frame_buffer->full_frame_received = false;
}

static bool is_data_broken(uint8_t data[], uint16_t len) {
    uint8_t check_result = 0;
    uint16_t index = 0;
    while (index < len) {
        check_result += data[index];
        index++;
    }

    if (check_result == 0xFF) {
        return false;  // 数据未损坏
    } else {
        return true;  // 数据损坏
    }
}

static void timer_one_frame_callback(TimerHandle_t x_timer) {
    frame_buffer.full_frame_received = true;
    ESP_LOGI(kTag, "Idle timer callback triggered, marking frame as complete");
}

static bool is_end_of_receive(uint8_t data[], uint16_t len) {
    if (len < 3) {
        return false;
    }

    return data[len - 2] == '\r' && data[len - 1] == '\n';
}