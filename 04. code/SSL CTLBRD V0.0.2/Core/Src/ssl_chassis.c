#include "ssl_chassis.h"

#include "ssl_control_command.h"
#include "ssl_esp32_link.h"
#include "ssl_host_console.h"
#include "ssl_mpu6050.h"
#include "ssl_motor_board.h"
#include <math.h>
#include <stdbool.h>

#define SSL_COMMAND_TIMEOUT_MS 500U
#define SSL_STATUS_PERIOD_MS 100U

static void SSL_ConfigurePulseInputs(void);
static void SSL_ConfigureTimer6(void);
static void SSL_ChassisControlTick(void);
static void SSL_Chassis_HandleCommand(const SslControlCommand *command, bool from_esp32);
static void SSL_StopAllMotors(void);
static float SSL_ApplyHeadingControl(void);
static void SSL_UpdateWheelTargetsFromVelocity(void);
static float SSL_DegreesToRadians(float degrees);
static float SSL_AbsoluteFloat(float value);
static float SSL_ClampFloat(float value, float min_value, float max_value);
static float SSL_NormalizeAngleDeg(float angle_deg);
static int16_t SSL_RoundToInt16(float value);

static const float kWheelRadiusM = 0.05f;
static const float kHalfWheelbaseM = 0.12f;
static const float kHalfTrackM = 0.11f;
static const float kMaxWheelRpm = 450.0f;
static const float kHeadingHoldTranslationThresholdMps = 0.05f;
static const float kHeadingHoldCommandThresholdRadps = 0.08f;
static const float kHeadingHoldKpRadpsPerDeg = 0.04f;
static const float kHeadingHoldMaxCorrectionRadps = 1.20f;
static const float kPi = 3.1415926f;
static const float kWheelAngleDeg[SSL_MOTOR_BOARD_COUNT] = {45.0f, -45.0f, 135.0f, -135.0f};
static const int8_t kWheelDirectionSign[SSL_MOTOR_BOARD_COUNT] = {1, 1, 1, 1};
static const SslEsp32LinkConfig kEsp32LinkConfig = {
    .uart_port =
        {
            .instance = USART6,
            .tx_port = GPIOC,
            .tx_pin = GPIO_PIN_6,
            .rx_port = GPIOC,
            .rx_pin = GPIO_PIN_7,
            .gpio_af = GPIO_AF8_USART6,
            .baudrate = 921600U,
        },
    .enable_pin =
        {
            .port = GPIOB,
            .pin = GPIO_PIN_0,
            .active_high = true,
        },
    .boot_pin =
        {
            .port = GPIOB,
            .pin = GPIO_PIN_1,
            .active_high = true,
        },
};
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

static SslVelocityCommand g_target_velocity = {0.0f, 0.0f, 0.0f};
static volatile uint32_t g_last_command_tick_ms = 0U;
static volatile uint32_t g_last_status_tick_ms = 0U;
static volatile uint32_t g_feedback_pulses[SSL_MOTOR_BOARD_COUNT] = {0U};
static bool g_raw_override_enabled = false;
static bool g_heading_reference_valid = false;
static bool g_heading_hold_active = false;
static float g_heading_target_deg = 0.0f;
static float g_heading_corrected_wz_radps = 0.0f;

void SSL_Chassis_Init(void)
{
  SSL_HostConsole_Init(&kHostPort);
  SSL_Esp32Link_Init(&kEsp32LinkConfig);
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
  SslControlCommand command;

  if (SSL_HostConsole_TryReadCommand(&command))
  {
    SSL_Chassis_HandleCommand(&command, false);
    return;
  }

  if (SSL_Esp32Link_TryReadCommand(&command))
  {
    SSL_Chassis_HandleCommand(&command, true);
    return;
  }

  SSL_Mpu6050_Process();
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
  SslEsp32StatusPayload esp32_status;
  const SslMpu6050State *mpu_state = SSL_Mpu6050_GetState();
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
      esp32_status.wheel_rpm[index] = wheel_rpm[index];
    }

    SSL_HostConsole_ReportStatus(
        &g_target_velocity,
        wheel_rpm,
        SSL_MOTOR_BOARD_COUNT,
        mpu_state->yaw,
        g_heading_target_deg,
        g_heading_corrected_wz_radps,
        g_heading_hold_active);
    esp32_status.velocity = g_target_velocity;
    SSL_Esp32Link_SendStatus(&esp32_status);
  }
}

static void SSL_Chassis_HandleCommand(const SslControlCommand *command, bool from_esp32)
{
  int16_t wheel_rpm[SSL_MOTOR_BOARD_COUNT];
  uint32_t index = 0U;

  if (command->help_requested)
  {
    if (!from_esp32)
    {
      SSL_HostConsole_ReportHelp();
    }
    else
    {
      SSL_Esp32Link_SendError(0x11U);
    }
    return;
  }

  if (command->ping_requested)
  {
    if (!from_esp32)
    {
      SSL_HostConsole_ReportAck("OK PING");
    }
    else
    {
      SSL_Esp32Link_SendAck();
    }
    return;
  }

  if (command->status_requested)
  {
    SslEsp32StatusPayload esp32_status;

    for (index = 0U; index < SSL_MOTOR_BOARD_COUNT; ++index)
    {
      wheel_rpm[index] = g_motors[index].target_rpm;
      esp32_status.wheel_rpm[index] = wheel_rpm[index];
    }

    if (!from_esp32)
    {
      const SslMpu6050State *mpu_state = SSL_Mpu6050_GetState();

      SSL_HostConsole_ReportStatus(
          &g_target_velocity,
          wheel_rpm,
          SSL_MOTOR_BOARD_COUNT,
          mpu_state->yaw,
          g_heading_target_deg,
          g_heading_corrected_wz_radps,
          g_heading_hold_active);
    }
    else
    {
      esp32_status.velocity = g_target_velocity;
      SSL_Esp32Link_SendStatus(&esp32_status);
    }
    return;
  }

  if (command->stop_requested)
  {
    SSL_StopAllMotors();
    g_last_command_tick_ms = HAL_GetTick();
    if (!from_esp32)
    {
      SSL_HostConsole_ReportAck("OK STOP");
    }
    else
    {
      SSL_Esp32Link_SendAck();
    }
    return;
  }

  if (command->raw_mode)
  {
    for (index = 0U; index < SSL_MOTOR_BOARD_COUNT; ++index)
    {
      g_motors[index].target_rpm = SSL_RoundToInt16(
          SSL_ClampFloat((float)command->raw_rpm[index], -kMaxWheelRpm, kMaxWheelRpm));
    }

    g_raw_override_enabled = true;
    g_last_command_tick_ms = HAL_GetTick();
    if (!from_esp32)
    {
      SSL_HostConsole_ReportAck("OK RAW");
    }
    else
    {
      SSL_Esp32Link_SendAck();
    }
    return;
  }

  g_target_velocity = command->velocity;
  g_raw_override_enabled = false;
  g_last_command_tick_ms = HAL_GetTick();
  if (!from_esp32)
  {
    SSL_HostConsole_ReportAck("OK VEL");
  }
  else
  {
    SSL_Esp32Link_SendAck();
  }
}

static void SSL_StopAllMotors(void)
{
  const SslMpu6050State *mpu_state = SSL_Mpu6050_GetState();

  g_target_velocity.vx_mps = 0.0f;
  g_target_velocity.vy_mps = 0.0f;
  g_target_velocity.wz_radps = 0.0f;
  g_heading_corrected_wz_radps = 0.0f;
  g_heading_hold_active = false;
  g_heading_reference_valid = mpu_state->initialized;
  if (mpu_state->initialized)
  {
    g_heading_target_deg = mpu_state->yaw;
  }
  g_raw_override_enabled = false;
  SSL_MotorBoard_StopAll(g_motors, SSL_MOTOR_BOARD_COUNT);
}

static float SSL_ApplyHeadingControl(void)
{
  const SslMpu6050State *mpu_state = SSL_Mpu6050_GetState();
  const float translation_speed_mps =
      sqrtf(
          (g_target_velocity.vx_mps * g_target_velocity.vx_mps) +
          (g_target_velocity.vy_mps * g_target_velocity.vy_mps));
  const float commanded_wz_radps = g_target_velocity.wz_radps;
  float heading_error_deg = 0.0f;

  g_heading_corrected_wz_radps = commanded_wz_radps;

  if (!mpu_state->initialized)
  {
    g_heading_hold_active = false;
    g_heading_reference_valid = false;
    return commanded_wz_radps;
  }

  if (SSL_AbsoluteFloat(commanded_wz_radps) >= kHeadingHoldCommandThresholdRadps)
  {
    g_heading_target_deg = mpu_state->yaw;
    g_heading_reference_valid = true;
    g_heading_hold_active = false;
    return commanded_wz_radps;
  }

  if (translation_speed_mps < kHeadingHoldTranslationThresholdMps)
  {
    g_heading_target_deg = mpu_state->yaw;
    g_heading_reference_valid = true;
    g_heading_hold_active = false;
    g_heading_corrected_wz_radps = 0.0f;
    return 0.0f;
  }

  if (!g_heading_reference_valid)
  {
    g_heading_target_deg = mpu_state->yaw;
    g_heading_reference_valid = true;
  }

  heading_error_deg = SSL_NormalizeAngleDeg(g_heading_target_deg - mpu_state->yaw);
  g_heading_corrected_wz_radps = SSL_ClampFloat(
      heading_error_deg * kHeadingHoldKpRadpsPerDeg,
      -kHeadingHoldMaxCorrectionRadps,
      kHeadingHoldMaxCorrectionRadps);
  g_heading_hold_active = true;
  return g_heading_corrected_wz_radps;
}

static void SSL_UpdateWheelTargetsFromVelocity(void)
{
  const float rotation_arm = kHalfWheelbaseM + kHalfTrackM;
  const float wheel_rpm_per_mps = 60.0f / (2.0f * kPi * kWheelRadiusM);
  const float vx = g_target_velocity.vx_mps;
  const float vy = g_target_velocity.vy_mps;
  const float wz = SSL_ApplyHeadingControl();
  float wheel_linear_mps[SSL_MOTOR_BOARD_COUNT];
  float max_abs_wheel_rpm = 0.0f;
  uint32_t index = 0U;

  /* Generic omni-wheel inverse kinematics:
   * wheel_linear = cos(theta) * vx + sin(theta) * vy + wz * (L + W)
   * theta is the wheel drive direction angle measured CCW from +vx.
   */
  for (index = 0U; index < SSL_MOTOR_BOARD_COUNT; ++index)
  {
    const float wheel_angle_rad = SSL_DegreesToRadians(kWheelAngleDeg[index]);

    wheel_linear_mps[index] =
        (cosf(wheel_angle_rad) * vx) +
        (sinf(wheel_angle_rad) * vy) +
        (wz * rotation_arm);
  }

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

static float SSL_DegreesToRadians(float degrees)
{
  return degrees * (kPi / 180.0f);
}

static float SSL_AbsoluteFloat(float value)
{
  return (value >= 0.0f) ? value : -value;
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

static float SSL_NormalizeAngleDeg(float angle_deg)
{
  while (angle_deg > 180.0f)
  {
    angle_deg -= 360.0f;
  }

  while (angle_deg < -180.0f)
  {
    angle_deg += 360.0f;
  }

  return angle_deg;
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
