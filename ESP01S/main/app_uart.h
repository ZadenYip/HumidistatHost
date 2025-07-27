#ifndef __UART_H__
#define __UART_H__
#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "timers.h"
#include "uart.h"

typedef struct uart_buffer_t uart_buffer_t;

void app_uart_init(void);
#endif /* __UART_H__ */

