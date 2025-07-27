#ifndef __AHT20_H

#define __AHT20_H
#include <sys/_intsup.h>
#include <stdint.h>
#define AHT20_ADDRESS 0x70

void AHT20MeasureTrigger(void);

typedef struct {
    uint8_t rx_tx_buffer[6];
    uint32_t origin_humidity;
    uint32_t origin_temperature;
    uint8_t is_busy;
} AHT20;

extern AHT20 aht20;

void AHT20_Init(void);
void AHT20_SendMeasurement(void);
void AHT20_DMATxCpltOrWait1MoreTime(void);
void AHT20_GetMeasurement(void);
void AHT20_WaitMeasDone(void);
void FloatToStringTwoDecimal(float value, char* str);
void AHT20_DMARxCplt(void);

#endif /* __AHT20_H */
