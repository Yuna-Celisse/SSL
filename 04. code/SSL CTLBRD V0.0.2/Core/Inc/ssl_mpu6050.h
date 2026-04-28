#ifndef __SSL_MPU6050_H
#define __SSL_MPU6050_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef struct
{
  short gyro[3];
  short accel[3];
  float pitch;
  float roll;
  float yaw;
  bool initialized;
} SslMpu6050State;

void SSL_Mpu6050_Init(void);
void SSL_Mpu6050_Process(void);
const SslMpu6050State *SSL_Mpu6050_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* __SSL_MPU6050_H */
