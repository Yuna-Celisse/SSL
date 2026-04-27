#include "clock.h"

#include "main.h"

void mspm0_delay_ms(unsigned long num_ms)
{
  HAL_Delay((uint32_t)num_ms);
}

void mspm0_get_clock_ms(unsigned long *count)
{
  *count = HAL_GetTick();
}
