#ifndef __COMMUNICATE_H
#define __COMMUNICATE_H
#include "stm32f1xx.h"
#include <stdint.h>


uint8_t is_command_end(const uint8_t data[], uint16_t length);
uint8_t get_checksum(const uint8_t data[], uint16_t length);
void wait_for_command(void);
/**
 * @brief 把ssid和password发送到ESP01S
 * 发送格式如下
 * 0x00|ssid_length|password_length|password
 * 
 * @param communication_msg 
 */
void SetWIFIConfiguration(char communication_msg[]);
void transmit_temp_and_humi_to_esp(float temperature, float humidity);
#endif /* __COMMUNICATE_H */