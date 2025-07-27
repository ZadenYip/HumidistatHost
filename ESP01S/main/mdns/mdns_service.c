/**
 * @file mdns_service.c
 * @brief mDNS service implementation for Modbus TCP slave
 */
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "mdns_service.h"

static const char *TAG = "MDNS_SERVICE";

/**
 * @brief Convert MAC from binary format to string
 * 
 * @param mac MAC address in binary format
 * @param pref Prefix string
 * @param mac_str Output buffer for MAC string
 * @return char* Pointer to the output buffer
 */
static inline char* gen_mac_str(const uint8_t* mac, char* pref, char* mac_str)
{
    sprintf(mac_str, "%s%02X%02X%02X%02X%02X%02X", pref, 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return mac_str;
}

/**
 * @brief Generate ID string
 * 
 * @param service_name Service name prefix
 * @param slave_id_str Output buffer for ID string
 * @return char* Pointer to the output buffer
 */
static inline char* gen_id_str(char* service_name, char* slave_id_str)
{
    sprintf(slave_id_str, "%s%02X%02X%02X%02X", service_name, MB_ID2STR(MB_DEVICE_ID));
    return slave_id_str;
}

/**
 * @brief Initialize and start mDNS service for Modbus TCP
 * 
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t start_mdns_service(void)
{
    char temp_str[32] = {0};
    uint8_t sta_mac[6] = {0};
    esp_err_t ret;
    
    ret = esp_read_mac(sta_mac, ESP_MAC_WIFI_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MAC address");
        return ret;
    }

    strncpy(temp_str, "humidistat-esp8266", sizeof(temp_str));
    char* hostname = temp_str;

    // Initialize mDNS
    ret = mdns_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mDNS");
        return ret;
    }
    
    // Set mDNS hostname
    ret = mdns_hostname_set(hostname);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set mDNS hostname");
        return ret;
    }
    ESP_LOGI(TAG, "mDNS hostname set to: [%s]", hostname);

    // Set default mDNS instance name
    ret = mdns_instance_name_set(MB_MDNS_INSTANCE("humidistat"));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set mDNS instance name");
        return ret;
    }

    // Structure with TXT records
    mdns_txt_item_t serviceTxtData[] = {
        {"board", "esp8266"}
    };

    // Initialize service
    ret = mdns_service_add(hostname, "_modbus", "_tcp", MB_MDNS_PORT, serviceTxtData, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add mDNS service");
        return ret;
    }
    
    // Add MAC key string text item
    ret = mdns_service_txt_item_set("_modbus", "_tcp", "mac", gen_mac_str(sta_mac, "\0", temp_str));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set MAC TXT record");
        return ret;
    }
    
    // Add slave ID key TXT item
    ret = mdns_service_txt_item_set("_modbus", "_tcp", "mb_id", gen_id_str("\0", temp_str));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set MB_ID TXT record");
        return ret;
    }
    
    ESP_LOGI(TAG, "mDNS service started successfully");
    return ESP_OK;
}

/**
 * @brief Free mDNS resources
 * 
 */
void stop_mdns_service(void)
{
    mdns_free();
    ESP_LOGI(TAG, "mDNS service stopped");
}
