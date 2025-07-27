#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "aht20.h"
#include "i2c.h"
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "communicate.h"

#define BUSY_MSG "Busy"
#define START_MSG "Start"
AHT20 aht20 = {0};

void AHT20MeasureTrigger(void) {
  if (aht20.is_busy) {
    strcpy(communication_msg, BUSY_MSG);
    HAL_UART_Transmit_DMA(&huart3, (uint8_t *)communication_msg, strlen(BUSY_MSG));
    AHT20_DMATxCpltOrWait1MoreTime();
  } else {
    strcpy(communication_msg, START_MSG);
    HAL_UART_Transmit_DMA(&huart3, (uint8_t *)communication_msg, strlen(START_MSG));
    AHT20_SendMeasurement();
  }
}

void AHT20_Init(void) {
  aht20 = (AHT20){0};
  // 上电后等待40ms
  HAL_Delay(40);
  HAL_I2C_Master_Receive(&hi2c1, AHT20_ADDRESS, aht20.rx_tx_buffer, 1, HAL_MAX_DELAY);
  if (!(aht20.rx_tx_buffer[0] & 0x08)) {
    aht20.rx_tx_buffer[0] = 0xBE;
    aht20.rx_tx_buffer[1] = 0x08;
    aht20.rx_tx_buffer[2] = 0x00;
    HAL_I2C_Master_Transmit(&hi2c1, AHT20_ADDRESS, aht20.rx_tx_buffer, 3, HAL_MAX_DELAY);
  }
}

void AHT20_SendMeasurement(void) {
  aht20.rx_tx_buffer[0] = 0xAC;
  aht20.rx_tx_buffer[1] = 0x33;
  aht20.rx_tx_buffer[2] = 0x00;
  aht20.is_busy = 1;
  // 回调函数是AHT20_DMATxCpltOrWait1MoreTime
  HAL_I2C_Master_Transmit_DMA(&hi2c1, AHT20_ADDRESS, aht20.rx_tx_buffer, 3);
  __HAL_DMA_DISABLE_IT(hi2c1.hdmatx, DMA_IT_HT);
}

void AHT20_DMATxCpltOrWait1MoreTime(void) {
  // 重置定时器计数器
  __HAL_TIM_SET_COUNTER(&htim1, 0); 
  // 用定时器等待75ms 然后中断调用AHT20_WaitMeasDone
  HAL_TIM_Base_Start_IT(&htim1);
}

void AHT20_GetMeasurement(void) {
  HAL_I2C_Master_Receive_DMA(&hi2c1, AHT20_ADDRESS, aht20.rx_tx_buffer, 6);
  __HAL_DMA_DISABLE_IT(hi2c1.hdmarx, DMA_IT_HT);
}

void AHT20_WaitMeasDone(void) {
    HAL_TIM_Base_Stop_IT(&htim1);
    AHT20_GetMeasurement();
}

void FloatToStringTwoDecimal(float value, char* str) {
    int integer = (int)value;
    int decimal = (int)((value - integer) * 100);
    sprintf(str, "%3d.%02d", integer, decimal);
}

void AHT20_DMARxCplt(void) {
    uint8_t status = aht20.rx_tx_buffer[0];
    if ((status & 0x80) == 0x00) {
        aht20.origin_humidity = (uint32_t)aht20.rx_tx_buffer[1] << 12 | (uint32_t)aht20.rx_tx_buffer[2] << 4 | ((uint32_t)aht20.rx_tx_buffer[3] >> 4 & 0x0F);
        aht20.origin_temperature = ((uint32_t)aht20.rx_tx_buffer[3] & 0x0F) << 16 | (uint32_t)aht20.rx_tx_buffer[4] << 8 | (uint32_t)aht20.rx_tx_buffer[5];

        float humidity = (float)aht20.origin_humidity / (1 << 20) * 100.0f;
        float temperature = (float)aht20.origin_temperature / (1 << 20) * 200 - 50;
        static char msg[40];
        char temp_str[8], humid_str[8];
        FloatToStringTwoDecimal(temperature, temp_str);
        FloatToStringTwoDecimal(humidity, humid_str);
        sprintf(msg,"温度: %s°C, 湿度: %s%%", temp_str, humid_str);
        HAL_UART_Transmit(&huart3, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
        transmit_temp_and_humi_to_esp(temperature, humidity);
        aht20.is_busy = 0;
    } else {
        AHT20_DMATxCpltOrWait1MoreTime();
    }
}
