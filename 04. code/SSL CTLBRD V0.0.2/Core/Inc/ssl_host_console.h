#ifndef __SSL_HOST_CONSOLE_H
#define __SSL_HOST_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ssl_uart.h"
#include <stdbool.h>

typedef struct
{
  float vx_mps;
  float vy_mps;
  float wz_radps;
} SslHostVelocityCommand;

typedef struct
{
  bool has_command;
  bool stop_requested;
  bool status_requested;
  bool help_requested;
  bool raw_mode;
  int16_t raw_rpm[4];
  SslHostVelocityCommand velocity;
} SslHostCommand;

void SSL_HostConsole_Init(const SslUartPort *port);
bool SSL_HostConsole_TryReadCommand(SslHostCommand *command);
void SSL_HostConsole_USART1_IRQHandler(void);
void SSL_HostConsole_Write(const char *text);
void SSL_HostConsole_WriteLine(const char *text);
void SSL_HostConsole_ReportStatus(
    const SslHostVelocityCommand *velocity,
    const int16_t *wheel_rpm,
    uint32_t wheel_count);
void SSL_HostConsole_ReportHelp(void);
void SSL_HostConsole_ReportAck(const char *message);
void SSL_HostConsole_ReportError(const char *message);

#ifdef __cplusplus
}
#endif

#endif /* __SSL_HOST_CONSOLE_H */
