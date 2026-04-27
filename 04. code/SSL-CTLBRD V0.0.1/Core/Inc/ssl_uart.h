#ifndef __SSL_UART_H
#define __SSL_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct
{
  USART_TypeDef *instance;
  GPIO_TypeDef *tx_port;
  uint16_t tx_pin;
  GPIO_TypeDef *rx_port;
  uint16_t rx_pin;
  uint8_t gpio_af;
  uint32_t baudrate;
} SslUartPort;

void SSL_Uart_Init(const SslUartPort *port, bool enable_rx_irq);
void SSL_Uart_WriteByte(USART_TypeDef *instance, uint8_t data);
void SSL_Uart_WriteBuffer(USART_TypeDef *instance, const uint8_t *buffer, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* __SSL_UART_H */
