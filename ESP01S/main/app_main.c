#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "spi_flash.h"

#include "wifi_module.h"
#include "app_uart.h"

void app_main() {
    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    app_uart_init();
    printf("This is ESP8266 chip with %d CPU cores, WiFi, ", chip_info.cores);
    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded"
                                                         : "external");
    printf("-----------wifi_init_main()-----------\n");
    wifi_init_main();
}