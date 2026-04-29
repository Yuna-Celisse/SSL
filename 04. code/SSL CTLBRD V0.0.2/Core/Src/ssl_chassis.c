#include "ssl_chassis.h"

#include "ssl_host_console.h"
#include "ssl_mpu6050.h"
#include "ssl_motor_board.h"
#include <stdbool.h>

#define SSL_COMMAND_TIMEOUT_MS 500U
#define SSL_STATUS_PERIOD_MS 100U

static void SSL_ConfigurePulseInputs(void);
static void SSL_ConfigureTimer6(void);
static void SSL_ChassisControlTick(void);
static void SSL_StopAllMotors(void);
static void SSL_UpdateWheelTargetsFromVelocity(void);
static float SSL_ClampFloat(float value, float min_value, float max_value);
static int16_t SSL_RoundToInt16(float value);

static const float kWheelRadiusM = 0.05f;
static const float kHalfWheelbaseM = 0.12f;
static const float kHalfTrackM = 0.11f;
static const float kMaxWheelRpm = 450.0f;
static const float kSqrt2Inv = 0.70710678f;
static const int8_t kWheelDirectionSign[SSL_MOTOR_BOARD_COUNT] = {1, 1, 1, 1};
static const SslUartPort kHostPort = {
    .instance = USART1,
    .tx_port = GPIOA,
    .tx_pin = GPIO_PIN_9,
    .rx_port = GPIOA,
    .rx_pin = GPIO_PIN_10,
    .gpio_af = GPIO_AF7_USART1,
    .baudrate = 115200U,
};

static SslMotorBoard g_motors[SSL_MOTOR_BOARD_COUNT] = {
    {
        .port =
            {
                .instance = USART2,
                .tx_port = GPIOD,
                .tx_pin = GPIO_PIN_5,
                .rx_port = GPIOD,
                .rx_pin = GPIO_PIN_6,
                .gpio_af = GPIO_AF7_USART2,
                .baudrate = 115200U,
            },
        .node_id = 1U,
        .target_rpm = 0,
    },
    {
        .port =
            {
                .instance = USART3,
                .tx_port = GPIOD,
                .tx_pin = GPIO_PIN_8,
                .rx_port = GPIOD,
                .rx_pin = GPIO_PIN_9,
                .gpio_af = GPIO_AF7_USART3,
                .baudrate = 115200U,
            },
        .node_id = 2U,
        .target_rpm = 0,
    },
    {
        .port =
            {
                .instance = UART4,
                .tx_port = GPIOC,
                .tx_pin = GPIO_PIN_10,
                .rx_port = GPIOC,
                .rx_pin = GPIO_PIN_11,
                .gpio_af = GPIO_AF8_UART4,
                .baudrate = 115200U,
            },
        .node_id = 3U,
        .target_rpm = 0,
    },
    {
        .port =
            {
                .instance = UART5,
                .tx_port = GPIOC,
                .tx_pin = GPIO_PIN_12,
                .rx_port = GPIOD,
                .rx_pin = GPIO_PIN_2,
                .gpio_af = GPIO_AF8_UART5,
                .baudrate = 115200U,
            },
        .node_id = 4U,
        .target_rpm = 0,
    },
};

static SslHostVelocityCommand g_target_velocity = {0.0f, 0.0f, 0.0f};
static volatile uint32_t g_last_command_tick_ms = 0U;
static volatile uint32_t g_last_status_tick_ms = 0U;
static volatile uint32_t g_feedback_pulses[SSL_MOTOR_BOARD_COUNT] = {0U};
static bool g_raw_override_enabled = false;

void SSL_Chassis_Init(void)
{
  SSL_HostConsole_Init(&kHostPort);
  SSL_Mpu6050_Init();
  SSL_MotorBoard_InitAll(g_motors, SSL_MOTOR_BOARD_COUNT);
  SSL_ConfigurePulseInputs();
  SSL_ConfigureTimer6();
  SSL_StopAllMotors();
  SSL_HostConsole_WriteLine("SSL chassis controller ready.");
  SSL_HostConsole_WriteLine("Use HELP for commands.");
}

void SSL_Chassis_Process(void)
{
  SslHostCommand command;

  if (!SSL_HostConsole_TryReadCommand(&command))
  {
    SSL_Mpu6050_Process();
    return;
  }

  if (command.help_requested)
  {
    SSL_HostConsole_ReportHelp();
    return;
  }

  if (command.status_requested)
  {
    int16_t wheel_rpm[SSL_MOTOR_BOARD_COUNT];
    uint32_t index = 0U;

    for (index = 0U; index < SSL_MOTOR_BOARD_COUNT; ++index)
    {
      wheel_rpm[index] = g_motors[index].target_rpm;
    }

    SSL_HostConsole_ReportStatus(&g_target_velocity, wheel_rpm, SSL_MOTOR_BOARD_COUNT);
    return;
  }

  if (command.stop_requested)
  {
    SSL_StopAllMotors();
    g_last_command_tick_ms = HAL_GetTick();
    SSL_HostConsole_ReportAck("OK STOP");
    return;
  }

  if (command.raw_mode)
  {
    uint32_t index = 0U;

    for (index = 0U; index < SSL_MOTOR_BOARD_COUNT; ++index)
    {
      g_motors[index].target_rpm = SSL_RoundToInt16(
          SSL_ClampFloat((float)command.raw_rpm[index], -kMaxWheelRpm, kMaxWheelRpm));
    }

    g_raw_override_enabled = true;
    g_last_command_tick_ms = HAL_GetTick();
    SSL_HostConsole_ReportAck("OK RAW");
    return;
  }

  g_target_velocity = command.velocity;
  g_raw_override_enabled = false;
  g_last_command_tick_ms = HAL_GetTick();
  SSL_HostConsole_ReportAck("OK VEL");
}

void SSL_Chassis_USART1_IRQHandler(void)
{
  SSL_HostConsole_USART1_IRQHandler();
}

void SSL_Chassis_EXTI_IRQHandler(uint16_t gpio_pin)
{
  if (__HAL_GPIO_EXTI_GET_IT(gpio_pin) != RESET)
  {
    __HAL_GPIO_EXTI_CLEAR_IT(gpio_pin);

    if (gpio_pin == GPIO_PIN_0)
    {
      ++g_feedback_pulses[0];
    }
    else if (gpio_pin == GPIO_PIN_1)
    {
      ++g_feedback_pulses[1];
    }
    else if (gpio_pin == GPIO_PIN_2)
    {
      ++g_feedback_pulses[2];
    }
    else if (gpio_pin == GPIO_PIN_3)
    {
      ++g_feedback_pulses[3];
    }
  }
}

void SSL_Chassis_TIM6_IRQHandler(void)
{
  if ((TIM6->SR & TIM_SR_UIF) != 0U)
  {
    TIM6->SR &= (uint16_t)(~TIM_SR_UIF);
    SSL_ChassisControlTick();
  }
}

static void SSL_ConfigurePulseInputs(void)
{
  HAL_NVIC_SetPriority(EXTI0_IRQn, 2U, 0U);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
  HAL_NVIC_SetPriority(EXTI1_IRQn, 2U, 0U);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);
  HAL_NVIC_SetPriority(EXTI2_IRQn, 2U, 0U);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);
  HAL_NVIC_SetPriority(EXTI3_IRQn, 2U, 0U);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);
}

static void SSL_ConfigureTimer6(void)
{
  __HAL_RCC_TIM6_CLK_ENABLE();

  TIM6->PSC = 7200U - 1U;
  TIM6->ARR = 100U - 1U;
  TIM6->EGR = TIM_EGR_UG;
  TIM6->SR = 0U;
  TIM6->DIER = TIM_DIER_UIE;
  TIM6->CR1 = TIM_CR1_CEN;

  HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 1U, 1U);
  HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
}

static void SSL_ChassisControlTick(void)
{
  int16_t wheel_rpm[SSL_MOTOR_BOARD_COUNT];
  const uint32_t now_ms = HAL_GetTick();
  uint32_t index = 0U;

  if ((now_ms - g_last_command_tick_ms) > SSL_COMMAND_TIMEOUT_MS)
  {
    SSL_StopAllMotors();
  }
  else if (!g_raw_override_enabled)
  {
    SSL_UpdateWheelTargetsFromVelocity();
  }

  SSL_MotorBoard_SendAll(g_motors, SSL_MOTOR_BOARD_COUNT);

  if ((now_ms - g_last_status_tick_ms) >= SSL_STATUS_PERIOD_MS)
  {
    g_last_status_tick_ms = now_ms;

    for (index = 0U; index < SSL_MOTOR_BOARD_COUNT; ++index)
    {
      wheel_rpm[index] = g_motors[index].target_rpm;
    }

    SSL_HostConsole_ReportStatus(&g_target_velocity, wheel_rpm, SSL_MOTOR_BOARD_COUNT);
  }
}

static void SSL_StopAllMotors(void)
{
  g_target_velocity.vx_mps = 0.0f;
  g_target_velocity.vy_mps = 0.0f;
  g_target_velocity.wz_radps = 0.0f;
  g_raw_override_enabled = false;
  SSL_MotorBoard_StopAll(g_motors, SSL_MOTOR_BOARD_COUNT);
}

static void SSL_UpdateWheelTargetsFromVelocity(void)
{
  const float rotation_arm = kHalfWheelbaseM + kHalfTrackM;
  const float wheel_rpm_per_mps = 60.0f / (2.0f * 3.1415926f * kWheelRadiusM);
  const float vx = g_target_velocity.vx_mps;
  const float vy = g_target_velocity.vy_mps;
  const float wz = g_target_velocity.wz_radps;
  float wheel_linear_mps[SSL_MOTOR_BOARD_COUNT];
  float max_abs_wheel_rpm = 0.0f;
  uint32_t index = 0U;

  /* Four omni-wheel layout:
   * front_left  =  45 deg
   * rear_left   = 135 deg
   * rear_right  = -135 deg
   * front_right = -45 deg
   */
  wheel_linear_mps[0] = (kSqrt2Inv * (vx + vy)) + (wz * rotation_arm);
  wheel_linear_mps[1] = (kSqrt2Inv * (vx - vy)) + (wz * rotation_arm);
  wheel_linear_mps[2] = (kSqrt2Inv * (-vx + vy)) + (wz * rotation_arm);
  wheel_linear_mps[3] = (kSqrt2Inv * (-vx - vy)) + (wz * rotation_arm);

  for (index = 0U; index < SSL_MOTOR_BOARD_COUNT; ++index)
  {
    const float wheel_rpm = wheel_linear_mps[index] * wheel_rpm_per_mps;
    const float abs_wheel_rpm = (wheel_rpm >= 0.0f) ? wheel_rpm : -wheel_rpm;

    if (abs_wheel_rpm > max_abs_wheel_rpm)
    {
      max_abs_wheel_rpm = abs_wheel_rpm;
    }
  }

  for (index = 0U; index < SSL_MOTOR_BOARD_COUNT; ++index)
  {
    float wheel_rpm = wheel_linear_mps[index] * wheel_rpm_per_mps;

    if (max_abs_wheel_rpm > kMaxWheelRpm)
    {
      wheel_rpm *= (kMaxWheelRpm / max_abs_wheel_rpm);
    }

    wheel_rpm *= (float)kWheelDirectionSign[index];

    g_motors[index].target_rpm = SSL_RoundToInt16(
        SSL_ClampFloat(wheel_rpm, -kMaxWheelRpm, kMaxWheelRpm));
  }
}

static float SSL_ClampFloat(float value, float min_value, float max_value)
{
  if (value < min_value)
  {
    return min_value;
  }

  if (value > max_value)
  {
    return max_value;
  }

  return value;
}

static int16_t SSL_RoundToInt16(float value)
{
  if (value >= 0.0f)
  {
    value += 0.5f;
  }
  else
  {
    value -= 0.5f;
  }

  if (value > 32767.0f)
  {
    return 32767;
  }

  if (value < -32768.0f)
  {
    return -32768;
  }

  return (int16_t)value;
}
