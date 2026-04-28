#include "ssl_mpu6050.h"

#include <stdint.h>

#include "mpu6050.h"

static SslMpu6050State g_ssl_mpu6050_state = {0};

void SSL_Mpu6050_Init(void)
{
  MPU6050_Init();
  g_ssl_mpu6050_state.initialized = true;
}

void SSL_Mpu6050_Process(void)
{
  uint32_t index = 0U;

  if ((!g_ssl_mpu6050_state.initialized) || (Read_Quad() != 0))
  {
    return;
  }

  for (index = 0U; index < 3U; ++index)
  {
    g_ssl_mpu6050_state.gyro[index] = gyro[index];
    g_ssl_mpu6050_state.accel[index] = accel[index];
  }

  g_ssl_mpu6050_state.pitch = pitch;
  g_ssl_mpu6050_state.roll = roll;
  g_ssl_mpu6050_state.yaw = yaw;
}

const SslMpu6050State *SSL_Mpu6050_GetState(void)
{
  return &g_ssl_mpu6050_state;
}
