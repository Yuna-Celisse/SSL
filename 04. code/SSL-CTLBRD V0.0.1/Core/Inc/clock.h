#ifndef __MPU6050_COMPAT_CLOCK_H
#define __MPU6050_COMPAT_CLOCK_H

void mspm0_delay_ms(unsigned long num_ms);
void mspm0_get_clock_ms(unsigned long *count);

#endif /* __MPU6050_COMPAT_CLOCK_H */
