#include "wifi/wifi_module.h"
#include <string.h>

#include "esp_err.h"
#include "esp_interface.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "modbus/tcp/tcp_slave.h"

#define MAX_WIFI_RETRY_CONNECT 3
static void check_if_already_connected();
static void connection_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data);
static void ip_assigned_handler(void *arg, esp_event_base_t event_base,
                         int32_t event_id, void *event_data);
static void wifi_reload(const char ssid[], const char password[]);
static void wifi_retry_timer_callback(TimerHandle_t xTimer);
static void wifi_load(const char ssid[], const char password[]);

EventGroupHandle_t wifi_event_group;
static const char kTag[] = "WIFI_MODULE";
static int wifi_retry_num = 0;
static TimerHandle_t wifi_retry_timer = NULL;

wifi_config_t wifi_cfg = {0};

void wifi_init_main() {
    wifi_event_group = xEventGroupCreate();
    esp_netif_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &connection_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &ip_assigned_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_cfg));
    wifi_retry_timer = xTimerCreate("wifi_retry_timer",
                                                pdMS_TO_TICKS(10000), pdFALSE,
                                                NULL, wifi_retry_timer_callback);
    wifi_load((char *)wifi_cfg.sta.ssid, (char *)wifi_cfg.sta.password);
}

static void wifi_load(const char ssid[], const char password[]) {
    memcpy(wifi_cfg.sta.ssid, ssid, strlen(ssid));
    memcpy(wifi_cfg.sta.password, password, strlen(password));

    if (strlen((char *)wifi_cfg.sta.password)) {
        wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }


    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg));
    ESP_LOGI(kTag, "wifi:ssid:%s, password:%s",
             wifi_cfg.sta.ssid, wifi_cfg.sta.password);
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(kTag, "wifi_init finished.");
    check_if_already_connected();
    modbus_init();
}

static void wifi_reload(const char ssid[], const char password[]) {
    modbus_deinit();
    esp_wifi_stop();
    wifi_load(ssid, password);
}

void wifi_set_new_config(const char ssid[], const char password[]) {
    xTimerStop(wifi_retry_timer, 0);
    ESP_LOGI(kTag, "setting new wifi config: ssid:%s, password:%s",
             ssid, password);
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    wifi_reload(ssid, password);
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
}

/**
 * @brief 检测WIFI是否完成连接，并且获取了IP地址
 * 
 */
static void check_if_already_connected() {
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);
    if (WIFI_CONNECTED_BIT & bits) {
        ESP_LOGI(kTag, "Connected to AP SSID:%s", wifi_cfg.sta.ssid);
    } else if (WIFI_FAIL_BIT & bits) {
        ESP_LOGE(kTag, "Failed to connect to SSID:%s, password:%s",
                 wifi_cfg.sta.ssid, wifi_cfg.sta.password);
    } else {
        ESP_LOGE(kTag, "Unexpected event for WIFI connection");
    }
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
}

/**
 * @brief WIFI连接事件处理函数
 * 
 */
static void connection_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data) {
    
    if (event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(kTag, "Station started, attempting to connect to AP");
        esp_wifi_connect();
        return;
    }
    /* 连接失败处理 */
    if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xTimerReset(wifi_retry_timer, 0);
        ESP_LOGI(kTag, "A retry will be attempted in 10 seconds");
    }
}

static void wifi_retry_timer_callback(TimerHandle_t xTimer) {
    ESP_LOGI(kTag, "Retry timer expired, reconnecting to AP");
    esp_wifi_connect();
}

/**
 * @brief IP地址分配事件处理函数，以此标识WIFI连接成功
 * 
 */
static void ip_assigned_handler(void *arg, esp_event_base_t event_base,
                         int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        wifi_retry_num = 0;

        ESP_LOGI(kTag, "got ip:%d.%d.%d.%d", IP2STR(&event->ip_info.ip));
        return;
    }
}