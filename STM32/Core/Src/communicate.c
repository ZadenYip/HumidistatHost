#include "communicate.h"
#include "aht20.h"
#include "main.h"
#include "stm32f1xx_hal_def.h"
#include "stm32f1xx_hal_uart.h"
#include "usart.h"
#include <stdint.h>
#include <string.h>
#include <sys/_intsup.h>
static void transmit_with_ARQ(UART_HandleTypeDef *huart2,
                              uint8_t *communication_msg, uint16_t msg_size);
static uint8_t is_data_broken(const uint8_t data[], uint16_t length);

typedef enum {
  // 触发AHT20测量
  HEADER_MEASURE = 0x00,
  // 设置ESP01S的WIFI连接信息
  HEADER_SET_WIFI = 0x01,
} CommandType;

typedef enum {
  // SSID长度Header索引
  INDEX_CHECK_SUM = 1,
  INDEX_SSID_LENGTH = 2,
  // 密码长度Header索引
  INDEX_PASSWORD_LENGTH = 3
} CommandIndex;

typedef enum {
  HEADER_ESP01S_SET_WIFI = 0x00,
  // 发送温湿度给ESP01S
  HEADER_ESP01S_RECEIVE_TEMP_AND_HUMI = 0x01
} ESP01SCommandType;

/**
 * @brief 等待 蓝牙模块通过USART3发送的命令
 * 命令格式第一个字节为命令字节，后面跟随参数。
 *
 */
void wait_for_command(void) {
  for (uint16_t rx_length = 0;;) {
    HAL_UARTEx_ReceiveToIdle(&huart3, (uint8_t *)&communication_msg,
                             sizeof(communication_msg), &rx_length,
                             HAL_MAX_DELAY);
    if (is_command_end((uint8_t *)communication_msg, rx_length)) {
      if (is_data_broken((uint8_t *)communication_msg, rx_length)) {
        strcpy(communication_msg, "NAK\r\n");
        HAL_UART_Transmit(&huart3, (uint8_t *)communication_msg,
                          strlen(communication_msg), HAL_MAX_DELAY);
        rx_length = 0;
        continue;
      } else {
        char ack_msg[] = "ACK\r\n";
        HAL_UART_Transmit(&huart3, (uint8_t *)ack_msg, strlen(ack_msg),
                          HAL_MAX_DELAY);
        break;
      }
    } else {
      strcpy(communication_msg, "wrong format\r\n");
      HAL_UART_Transmit(&huart3, (uint8_t *)communication_msg,
                        strlen(communication_msg), HAL_MAX_DELAY);
    }
  }

  if (communication_msg[0] == HEADER_MEASURE) {
    AHT20MeasureTrigger();
  } else if (communication_msg[0] == HEADER_SET_WIFI) {
    SetWIFIConfiguration(communication_msg);
  } else {
    strcpy(communication_msg, "unknown command\r\n");
    HAL_UART_Transmit(&huart3, (uint8_t *)communication_msg,
                      strlen(communication_msg), HAL_MAX_DELAY);
  }
}

void SetWIFIConfiguration(char upper_msg[]) {
  /* 1字节命令标识 + 1字节checksum + 1字节表示SSID字节长度 +
   * 1字节表示SSID密码长度 + SSID最长32字节 + 密码最长63字节 + 1空字符 */
  uint8_t ssid_length = (uint8_t)upper_msg[INDEX_SSID_LENGTH];
  uint8_t password_length = (uint8_t)upper_msg[INDEX_PASSWORD_LENGTH];

  char ssid[33] = {0}; // SSID最长32字节 + 1空字符
  char password[64] = {0};

  uint8_t ssid_start_index = INDEX_PASSWORD_LENGTH + 1;
  strncpy(ssid, &upper_msg[ssid_start_index], ssid_length);
  ssid[ssid_length] = '\0';

  uint8_t password_start_index = ssid_start_index + ssid_length;
  strncpy(password, &upper_msg[password_start_index], password_length);
  password[password_length] = '\0';

  /*
  0x00在ESP01S为设置WIFI配置
  拼接配置数据格式为：
  0x00 checksum ssid_length password_length ssid password \r\n
  */
  communication_msg[0] = 0x00;
  communication_msg[1] = 0x00; // 初始化校验和为0
  communication_msg[2] = ssid_length;
  communication_msg[3] = password_length;
  uint8_t set_index = 4;
  memcpy(&communication_msg[set_index], ssid, ssid_length);
  set_index += ssid_length;
  memcpy(&communication_msg[set_index], password, password_length);
  set_index += password_length;
  communication_msg[set_index] = '\r';
  set_index++;
  communication_msg[set_index] = '\n';
  set_index++;
  communication_msg[1] = get_checksum((uint8_t *)communication_msg, set_index);
  communication_msg[set_index] = '\0';

  transmit_with_ARQ(&huart2, (uint8_t *)communication_msg, set_index);
}

/**
 * @brief 发送温湿度数据到ESP01S
 * 格式如下
 * 0x01 checksum temperature(4 bytes) humidity(4 bytes) \r\n 一共12字节
 * @param temperature 
 * @param humidity 
 */
void transmit_temp_and_humi_to_esp(float temperature, float humidity) {
  // 0x01表示发送温湿度
  communication_msg[0] = HEADER_ESP01S_RECEIVE_TEMP_AND_HUMI;
  // 初始化校验和为0
  communication_msg[1] = 0x00;
  // 温度和湿度各占4字节
  memcpy(&communication_msg[2], &temperature, sizeof(float));
  memcpy(&communication_msg[6], &humidity, sizeof(float));
  communication_msg[10] = '\r';
  communication_msg[11] = '\n';
  communication_msg[12] = '\0';

  communication_msg[1] = get_checksum((uint8_t *)communication_msg, 12);
  transmit_with_ARQ(&huart2, (uint8_t *)communication_msg, 12);
}

static void transmit_with_ARQ(UART_HandleTypeDef *huart2,
                              uint8_t *communication_msg, uint16_t msg_size) {
  static char response_msg[6] = {0};
  do {
    HAL_UART_Transmit(huart2, communication_msg, msg_size, HAL_MAX_DELAY);
    uint16_t rx_length = 0;
    HAL_UARTEx_ReceiveToIdle(huart2, (uint8_t *)response_msg,
                             sizeof(response_msg), &rx_length, 1000);
  } while (strcmp(response_msg, "ACK\r\n") != 0);
}

/**
 * @brief 检查数据是否损坏 要求包含Checksum的字段
 * 长度要除去\r\n
 * 
 */
static uint8_t is_data_broken(const uint8_t data[], uint16_t length) {
  uint8_t checksum = 0;
  uint16_t index = 0;
  while (index < length) {
    checksum += data[index];
    index++;
  }

  if (checksum == 0xFF) {
    return 0; // 数据未损坏
  } else {
    return 1; // 数据损坏
  }
}

uint8_t is_command_end(const uint8_t data[], uint16_t length) {
  if (length < 2) {
    return 0;
  }
  return (data[length - 2] == '\r' && data[length - 1] == '\n');
}

uint8_t get_checksum(const uint8_t data[], uint16_t length) {
  uint8_t checksum = 0;
  for (uint16_t i = 0; i < length; i++) {
    checksum += data[i];
  }
  return ~checksum;
}
