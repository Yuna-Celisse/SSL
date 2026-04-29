#ifndef __SSL_HOST_CONSOLE_H
#define __SSL_HOST_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ssl_control_command.h"
#include "ssl_uart.h"
#include <stdbool.h>

void SSL_HostConsole_Init(const SslUartPort *port);
bool SSL_HostConsole_TryReadCommand(SslControlCommand *command);
void SSL_HostConsole_USART1_IRQHandler(void);
void SSL_HostConsole_Write(const char *text);
void SSL_HostConsole_WriteLine(const char *text);
void SSL_HostConsole_ReportStatus(
    const SslVelocityCommand *velocity,
    const int16_t *wheel_rpm,
    uint32_t wheel_count);
void SSL_HostConsole_ReportHelp(void);
void SSL_HostConsole_ReportAck(const char *message);
void SSL_HostConsole_ReportError(const char *message);

#ifdef __cplusplus
}
#endif

#endif /* __SSL_HOST_CONSOLE_H */
