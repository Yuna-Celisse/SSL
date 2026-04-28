#ifndef __SSL_I2C_BUS_H
#define __SSL_I2C_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "stm32f4xx_hal_i2c.h"
#include <stdbool.h>

void SSL_I2cBus_Init(void);
HAL_StatusTypeDef SSL_I2cBus_MemWrite(
    uint16_t device_address,
    uint16_t register_address,
    const uint8_t *data,
    uint16_t length,
    uint32_t timeout_ms);
HAL_StatusTypeDef SSL_I2cBus_MemRead(
    uint16_t device_address,
    uint16_t register_address,
    uint8_t *data,
    uint16_t length,
    uint32_t timeout_ms);
bool SSL_I2cBus_IsSdaLow(void);
void SSL_I2cBus_SdaUnlock(void);

#ifdef __cplusplus
}
#endif

#endif /* __SSL_I2C_BUS_H */
