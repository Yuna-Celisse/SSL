#ifndef __SSL_CONTROL_COMMAND_H
#define __SSL_CONTROL_COMMAND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  float vx_mps;
  float vy_mps;
  float wz_radps;
} SslVelocityCommand;

typedef struct
{
  bool has_command;
  bool ping_requested;
  bool stop_requested;
  bool status_requested;
  bool help_requested;
  bool raw_mode;
  int16_t raw_rpm[4];
  SslVelocityCommand velocity;
} SslControlCommand;

#ifdef __cplusplus
}
#endif

#endif /* __SSL_CONTROL_COMMAND_H */
