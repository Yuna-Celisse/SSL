#include "mspm0_i2c.h"

#include "ssl_i2c_bus.h"

#define I2C_TIMEOUT_MS 10U

int mpu6050_i2c_bus_stuck(void)
{
  return SSL_I2cBus_IsSdaLow() ? 1 : 0;
}

void mpu6050_i2c_sda_unlock(void)
{
  SSL_I2cBus_SdaUnlock();
}

int mspm0_i2c_write(unsigned char slave_addr,
                     unsigned char reg_addr,
                     unsigned char length,
                     unsigned char const *data)
{
  return (SSL_I2cBus_MemWrite(slave_addr, reg_addr, data, length, I2C_TIMEOUT_MS) == HAL_OK) ? 0 : -1;
}

int mspm0_i2c_read(unsigned char slave_addr,
                    unsigned char reg_addr,
                    unsigned char length,
                    unsigned char *data)
{
  return (SSL_I2cBus_MemRead(slave_addr, reg_addr, data, length, I2C_TIMEOUT_MS) == HAL_OK) ? 0 : -1;
}
