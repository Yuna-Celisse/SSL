#ifndef __SSL_CHASSIS_H
#define __SSL_CHASSIS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void SSL_Chassis_Init(void);
void SSL_Chassis_Process(void);
void SSL_Chassis_EXTI_IRQHandler(uint16_t gpio_pin);
void SSL_Chassis_TIM6_IRQHandler(void);
void SSL_Chassis_USART1_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __SSL_CHASSIS_H */
