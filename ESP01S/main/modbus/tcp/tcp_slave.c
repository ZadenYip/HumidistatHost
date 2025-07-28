/* FreeModbus Slave Example ESP32

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "esp_err.h"
#include "esp_wifi.h"
#include "mdns_service.h"
#include "sdkconfig.h"
#include "esp_log.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "mbcontroller.h"
#include "modbus/common/modbus_params.h"      // for modbus parameters structures

#define MB_TCP_PORT_NUMBER      (CONFIG_FMB_TCP_PORT_DEFAULT)

// Defines below are used to define register start address for each type of Modbus registers
#define MB_REG_INPUT_START                  (0x0000)

#define MB_PAR_INFO_GET_TOUT                (50) // Timeout for get parameter info

#define MB_READ_MASK                        (MB_EVENT_INPUT_REG_RD \
                                                | MB_EVENT_HOLDING_REG_RD \
                                                | MB_EVENT_DISCRETE_RD \
                                                | MB_EVENT_COILS_RD)
#define MB_WRITE_MASK                       (MB_EVENT_HOLDING_REG_WR \
                                                | MB_EVENT_COILS_WR)
#define MB_READ_WRITE_MASK                  (MB_READ_MASK | MB_WRITE_MASK)

static const char kTag[] = "MB_SLAVE";
static TaskHandle_t mb_event_task_handler;
static void modbus_event_task(void *pvParameters);

/**
 * @brief Initialize Modbus slave stack
 * This function can be called after the Wi-Fi is connected
 * 
 */
void modbus_init(void)
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
    esp_netif_init();

    ESP_ERROR_CHECK(start_mdns_service());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    // Set UART log level
    esp_log_level_set(kTag, ESP_LOG_INFO);
    void* mbc_slave_handler = NULL;

    // Initialization of Modbus controller
    ESP_ERROR_CHECK(mbc_slave_init_tcp(&mbc_slave_handler)); 
    
    // Modbus register area descriptor structure
    mb_register_area_descriptor_t reg_area; 
    
    mb_communication_info_t comm_info = { 0 };
    comm_info.ip_port = MB_TCP_PORT_NUMBER;
    comm_info.ip_addr_type = MB_IPV4;
    comm_info.ip_mode = MB_MODE_TCP;
    comm_info.ip_addr = NULL;
    void * nif = NULL;
    ESP_ERROR_CHECK(tcpip_adapter_get_netif(TCPIP_ADAPTER_IF_STA, &nif));
    comm_info.ip_netif_ptr = nif;
     // Setup communication parameters and start stack
    ESP_ERROR_CHECK(mbc_slave_setup((void*)&comm_info));

    // The code below initializes Modbus register area descriptors
    // for Modbus Holding Registers, Input Registers, Coils and Discrete Inputs
    // Initialization should be done for each supported Modbus register area according to register map.
    // When external master trying to access the register in the area that is not initialized
    // by mbc_slave_set_descriptor() API call then Modbus stack
    // will send exception response for this register area.

    // Initialization of Input Registers area
    reg_area.type = MB_PARAM_INPUT; // Set type of register area
    reg_area.start_offset = MB_REG_INPUT_START; // Offset of register area in Modbus protocol
    reg_area.address = (void*)&input_reg_params; // Set pointer to storage instance
    reg_area.size = sizeof(input_reg_params); // Set the size of register storage instance
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));

    // Starts of modbus controller and stack
    ESP_ERROR_CHECK(mbc_slave_start());
    xTaskCreate(modbus_event_task, "modbus_event_task", 1024, NULL, 3, &mb_event_task_handler);
}

static void modbus_event_task(void *pvParameters) {
    mb_param_info_t reg_info;
    while (1) {
        mb_event_group_t event = mbc_slave_check_event(MB_READ_WRITE_MASK);
        const char* rw_str = (event & MB_READ_MASK) ? "READ" : "WRITE";
        // Filter events and process them accordingly
        if (event & MB_EVENT_INPUT_REG_RD) {
            // Get parameter information from parameter queue
            ESP_ERROR_CHECK(
                mbc_slave_get_param_info(&reg_info, MB_PAR_INFO_GET_TOUT));
            ESP_LOGI(
                kTag,
                "INPUT %s (%u us), ADDR:%u, TYPE:%u, INST_ADDR:0x%.4x, SIZE:%u",
                rw_str, (uint32_t)reg_info.time_stamp, (uint32_t)reg_info.mb_offset,
                (uint32_t)reg_info.type, (uint32_t)reg_info.address,
                (uint32_t)reg_info.size);
        }
    }
}

void modbus_deinit(void)
{
    ESP_LOGI(kTag, "Deinitializing Modbus slave stack...");
    vTaskDelete(mb_event_task_handler);
    ESP_LOGI(kTag,"Modbus controller destroyed.");
    vTaskDelay(100);
    // Stop Modbus controller
    stop_mdns_service();
    ESP_ERROR_CHECK(mbc_slave_destroy());
    ESP_LOGI(kTag, "Modbus slave stack deinitialized.");
}

void modbus_update_temp_and_humi(float temperature, float humidity)
{
    portENTER_CRITICAL();
    input_reg_params.temperature = temperature;
    input_reg_params.humidity = humidity;
    portEXIT_CRITICAL();
}