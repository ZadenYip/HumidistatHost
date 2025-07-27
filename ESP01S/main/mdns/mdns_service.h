/**
 * @file mdns_service.h
 * @brief mDNS service implementation for Modbus TCP slave
 */
#ifndef MDNS_SERVICE_H
#define MDNS_SERVICE_H

#include <sdkconfig.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * @brief mDNS port for Modbus service
 */
#define MB_MDNS_PORT            (502)

/**
 * @brief Macros for MB ID byte conversion
 */
#define MB_ID_BYTE0(id) ((uint8_t)(id))
#define MB_ID_BYTE1(id) ((uint8_t)(((uint16_t)(id) >> 8) & 0xFF))
#define MB_ID_BYTE2(id) ((uint8_t)(((uint32_t)(id) >> 16) & 0xFF))
#define MB_ID_BYTE3(id) ((uint8_t)(((uint32_t)(id) >> 24) & 0xFF))

#define MB_ID2STR(id) MB_ID_BYTE0(id), MB_ID_BYTE1(id), MB_ID_BYTE2(id), MB_ID_BYTE3(id)

/**
 * @brief Slave address
 */
#if CONFIG_FMB_CONTROLLER_SLAVE_ID_SUPPORT
#define MB_DEVICE_ID (uint32_t)CONFIG_FMB_CONTROLLER_SLAVE_ID
#endif

#define MB_SLAVE_ADDR (CONFIG_MB_SLAVE_ADDR)

/**
 * @brief Instance name for mDNS service
 */
#define MB_MDNS_INSTANCE(pref) pref"mb_slave_tcp"

/**
 * @brief Initialize and start mDNS service for Modbus TCP
 * 
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t start_mdns_service(void);

/**
 * @brief Free mDNS resources
 * 
 */
void stop_mdns_service(void);

#endif /* MDNS_SERVICE_H */
