#include "ssl_host_console.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SSL_MAX_COMMAND_LINE 96U

static const SslUartPort *g_host_port = NULL;
static volatile char g_command_buffer[SSL_MAX_COMMAND_LINE];
static volatile uint8_t g_command_length = 0U;
static volatile bool g_command_ready = false;

static void SSL_HostConsole_CopyReadyCommand(char *line_buffer, size_t line_buffer_size);
static void SSL_HostConsole_ProcessLine(const char *line, SslControlCommand *command);
static void SSL_HostConsole_TrimLine(char *line);

void SSL_HostConsole_Init(const SslUartPort *port)
{
  g_host_port = port;
  g_command_length = 0U;
  g_command_ready = false;
  g_command_buffer[0] = '\0';
  SSL_Uart_Init(port, true);
}

bool SSL_HostConsole_TryReadCommand(SslControlCommand *command)
{
  char line[SSL_MAX_COMMAND_LINE];

  if (!g_command_ready)
  {
    return false;
  }

  memset(command, 0, sizeof(*command));
  SSL_HostConsole_CopyReadyCommand(line, sizeof(line));
  SSL_HostConsole_ProcessLine(line, command);
  return command->has_command;
}

void SSL_HostConsole_USART1_IRQHandler(void)
{
  uint8_t rx_byte = 0U;

  if ((g_host_port == NULL) || ((g_host_port->instance->SR & USART_SR_RXNE) == 0U))
  {
    return;
  }

  if ((g_host_port->instance->SR & USART_SR_ORE) != 0U)
  {
    (void)g_host_port->instance->DR;
  }

  rx_byte = (uint8_t)(g_host_port->instance->DR & 0xFFU);

  if (g_command_ready)
  {
    return;
  }

  if ((rx_byte == '\r') || (rx_byte == '\n'))
  {
    if (g_command_length > 0U)
    {
      g_command_buffer[g_command_length] = '\0';
      g_command_ready = true;
    }
    return;
  }

  if (g_command_length < (SSL_MAX_COMMAND_LINE - 1U))
  {
    g_command_buffer[g_command_length] = (char)rx_byte;
    ++g_command_length;
    g_command_buffer[g_command_length] = '\0';
  }
  else
  {
    g_command_length = 0U;
    g_command_buffer[0] = '\0';
  }
}

void SSL_HostConsole_Write(const char *text)
{
  SSL_Uart_WriteBuffer(g_host_port->instance, (const uint8_t *)text, strlen(text));
}

void SSL_HostConsole_WriteLine(const char *text)
{
  SSL_HostConsole_Write(text);
  SSL_HostConsole_Write("\r\n");
}

void SSL_HostConsole_ReportStatus(
    const SslVelocityCommand *velocity,
    const int16_t *wheel_rpm,
    uint32_t wheel_count,
    float yaw_deg,
    float heading_target_deg,
    float corrected_wz_radps,
    bool heading_hold_active)
{
  char buffer[200];
  const int32_t vx_mmps = (int32_t)(velocity->vx_mps * 1000.0f);
  const int32_t vy_mmps = (int32_t)(velocity->vy_mps * 1000.0f);
  const int32_t wz_mradps = (int32_t)(velocity->wz_radps * 1000.0f);
  const int32_t corrected_wz_mradps = (int32_t)(corrected_wz_radps * 1000.0f);

  if (wheel_count < 4U)
  {
    return;
  }

  (void)snprintf(
      buffer,
      sizeof(buffer),
      "STAT vx=%ldmm/s vy=%ldmm/s wz=%ldmrad/s corrWz=%ldmrad/s yaw=%.1fdeg holdYaw=%.1fdeg hold=%u rpm=[%d,%d,%d,%d]",
      (long)vx_mmps,
      (long)vy_mmps,
      (long)wz_mradps,
      (long)corrected_wz_mradps,
      (double)yaw_deg,
      (double)heading_target_deg,
      heading_hold_active ? 1U : 0U,
      wheel_rpm[0],
      wheel_rpm[1],
      wheel_rpm[2],
      wheel_rpm[3]);
  SSL_HostConsole_WriteLine(buffer);
}

void SSL_HostConsole_ReportHelp(void)
{
  SSL_HostConsole_WriteLine("HELP:");
  SSL_HostConsole_WriteLine("  VEL <vx_mps> <vy_mps> <wz_radps>");
  SSL_HostConsole_WriteLine("  RAW <fl_rpm> <fr_rpm> <rl_rpm> <rr_rpm>");
  SSL_HostConsole_WriteLine("  STOP");
  SSL_HostConsole_WriteLine("  STATUS");
}

void SSL_HostConsole_ReportAck(const char *message)
{
  SSL_HostConsole_WriteLine(message);
}

void SSL_HostConsole_ReportError(const char *message)
{
  SSL_HostConsole_WriteLine(message);
}

static void SSL_HostConsole_CopyReadyCommand(char *line_buffer, size_t line_buffer_size)
{
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  (void)strncpy(line_buffer, (const char *)g_command_buffer, line_buffer_size - 1U);
  line_buffer[line_buffer_size - 1U] = '\0';
  g_command_length = 0U;
  g_command_buffer[0] = '\0';
  g_command_ready = false;
  __set_PRIMASK(primask);

  SSL_HostConsole_TrimLine(line_buffer);
}

static void SSL_HostConsole_ProcessLine(const char *line, SslControlCommand *command)
{
  command->has_command = true;

  if (strncmp(line, "VEL", 3U) == 0)
  {
    char *end_ptr = NULL;
    const char *args = &line[3];
    float values[3];
    uint32_t index = 0U;

    for (index = 0U; index < 3U; ++index)
    {
      values[index] = strtof(args, &end_ptr);
      if (end_ptr == args)
      {
        SSL_HostConsole_ReportError("ERR VEL format: VEL <vx_mps> <vy_mps> <wz_radps>");
        command->has_command = false;
        return;
      }
      args = end_ptr;
    }

    command->velocity.vx_mps = values[0];
    command->velocity.vy_mps = values[1];
    command->velocity.wz_radps = values[2];
  }
  else if (strncmp(line, "RAW", 3U) == 0)
  {
    char *end_ptr = NULL;
    const char *args = &line[3];
    long rpm_value = 0L;
    uint32_t index = 0U;

    command->raw_mode = true;
    for (index = 0U; index < 4U; ++index)
    {
      rpm_value = strtol(args, &end_ptr, 10);
      if (end_ptr == args)
      {
        SSL_HostConsole_ReportError("ERR RAW format: RAW <fl> <fr> <rl> <rr>");
        command->has_command = false;
        return;
      }

      command->raw_rpm[index] = (int16_t)rpm_value;
      args = end_ptr;
    }
  }
  else if (strcmp(line, "STOP") == 0)
  {
    command->stop_requested = true;
  }
  else if (strcmp(line, "STATUS") == 0)
  {
    command->status_requested = true;
  }
  else if (strcmp(line, "HELP") == 0)
  {
    command->help_requested = true;
  }
  else if (line[0] != '\0')
  {
    SSL_HostConsole_ReportError("ERR unknown command, use HELP.");
    command->has_command = false;
  }
}

static void SSL_HostConsole_TrimLine(char *line)
{
  size_t start = 0U;
  size_t end = strlen(line);

  while ((line[start] != '\0') && isspace((unsigned char)line[start]))
  {
    ++start;
  }

  while ((end > start) && isspace((unsigned char)line[end - 1U]))
  {
    --end;
  }

  if (start > 0U)
  {
    memmove(line, &line[start], end - start);
  }

  line[end - start] = '\0';
}
