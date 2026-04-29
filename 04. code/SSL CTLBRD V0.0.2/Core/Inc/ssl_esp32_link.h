#ifndef __SSL_ESP32_LINK_H
#define __SSL_ESP32_LINK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ssl_control_command.h"
#include "ssl_esp32_protocol.h"
#include "ssl_uart.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
  bool active_high;
} SslEsp32ControlPin;

typedef struct
{
  SslUartPort uart_port;
  SslEsp32ControlPin enable_pin;
  SslEsp32ControlPin boot_pin;
} SslEsp32LinkConfig;

void SSL_Esp32Link_Init(const SslEsp32LinkConfig *config);
bool SSL_Esp32Link_TryReadCommand(SslControlCommand *command);
void SSL_Esp32Link_SendAck(void);
void SSL_Esp32Link_SendError(uint8_t error_code);
void SSL_Esp32Link_SendStatus(const SslEsp32StatusPayload *status);
void SSL_Esp32Link_ResetModule(void);

#ifdef __cplusplus
}
#endif

#endif /* __SSL_ESP32_LINK_H */
