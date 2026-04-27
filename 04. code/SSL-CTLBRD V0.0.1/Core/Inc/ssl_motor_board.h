#ifndef __SSL_MOTOR_BOARD_H
#define __SSL_MOTOR_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ssl_uart.h"

#define SSL_MOTOR_BOARD_COUNT 4U

typedef struct
{
  SslUartPort port;
  uint8_t node_id;
  int16_t target_rpm;
} SslMotorBoard;

void SSL_MotorBoard_InitAll(SslMotorBoard *motors, uint32_t motor_count);
void SSL_MotorBoard_StopAll(SslMotorBoard *motors, uint32_t motor_count);
void SSL_MotorBoard_SendTarget(const SslMotorBoard *motor);
void SSL_MotorBoard_SendAll(const SslMotorBoard *motors, uint32_t motor_count);

#ifdef __cplusplus
}
#endif

#endif /* __SSL_MOTOR_BOARD_H */
