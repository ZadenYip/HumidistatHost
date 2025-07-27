#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#include <stdint.h>
#include "FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event_base.h"
extern EventGroupHandle_t wifi_event_group;

/**
 * @brief 刚启动时初始化WIFI
 * 
 */
void wifi_init_main(void);
void wifi_set_new_config(const char ssid[], const char password[]);
#endif // WIFI_HANDLER_H