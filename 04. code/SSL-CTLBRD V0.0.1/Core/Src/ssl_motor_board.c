#include "ssl_motor_board.h"

#define SSL_FRAME_HEADER 0x7BU
#define SSL_FRAME_TAIL 0x7DU

static uint8_t SSL_MotorBoard_BuildChecksum(const uint8_t *buffer, size_t length);

void SSL_MotorBoard_InitAll(SslMotorBoard *motors, uint32_t motor_count)
{
  uint32_t index = 0U;

  for (index = 0U; index < motor_count; ++index)
  {
    SSL_Uart_Init(&motors[index].port, false);
  }
}

void SSL_MotorBoard_StopAll(SslMotorBoard *motors, uint32_t motor_count)
{
  uint32_t index = 0U;

  for (index = 0U; index < motor_count; ++index)
  {
    motors[index].target_rpm = 0;
    SSL_MotorBoard_SendTarget(&motors[index]);
  }
}

void SSL_MotorBoard_SendTarget(const SslMotorBoard *motor)
{
  uint8_t frame[6];

  frame[0] = SSL_FRAME_HEADER;
  frame[1] = motor->node_id;
  frame[2] = (uint8_t)(motor->target_rpm & 0xFF);
  frame[3] = (uint8_t)((motor->target_rpm >> 8) & 0xFF);
  frame[4] = SSL_MotorBoard_BuildChecksum(&frame[1], 3U);
  frame[5] = SSL_FRAME_TAIL;

  SSL_Uart_WriteBuffer(motor->port.instance, frame, sizeof(frame));
}

void SSL_MotorBoard_SendAll(const SslMotorBoard *motors, uint32_t motor_count)
{
  uint32_t index = 0U;

  for (index = 0U; index < motor_count; ++index)
  {
    SSL_MotorBoard_SendTarget(&motors[index]);
  }
}

static uint8_t SSL_MotorBoard_BuildChecksum(const uint8_t *buffer, size_t length)
{
  uint8_t checksum = 0U;
  size_t index = 0U;

  for (index = 0U; index < length; ++index)
  {
    checksum = (uint8_t)(checksum + buffer[index]);
  }

  return checksum;
}
