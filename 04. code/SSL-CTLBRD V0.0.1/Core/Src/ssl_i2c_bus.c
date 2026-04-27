#include "ssl_i2c_bus.h"

static I2C_HandleTypeDef g_ssl_i2c_handle;
static bool g_ssl_i2c_initialized = false;

static void SSL_I2cBus_InitPinsAlternate(void);

void SSL_I2cBus_Init(void)
{
  GPIO_InitTypeDef gpio_init = {0};
  HAL_StatusTypeDef init_status;

  if (g_ssl_i2c_initialized)
  {
    return;
  }

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_I2C1_CLK_ENABLE();

  gpio_init.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  gpio_init.Mode = GPIO_MODE_AF_OD;
  gpio_init.Pull = GPIO_PULLUP;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init.Alternate = GPIO_AF4_I2C1;
  HAL_GPIO_Init(GPIOB, &gpio_init);

  g_ssl_i2c_handle.Instance = I2C1;
  g_ssl_i2c_handle.Init.ClockSpeed = 400000U;
  g_ssl_i2c_handle.Init.DutyCycle = I2C_DUTYCYCLE_2;
  g_ssl_i2c_handle.Init.OwnAddress1 = 0U;
  g_ssl_i2c_handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  g_ssl_i2c_handle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  g_ssl_i2c_handle.Init.OwnAddress2 = 0U;
  g_ssl_i2c_handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  g_ssl_i2c_handle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  init_status = HAL_I2C_Init(&g_ssl_i2c_handle);
  g_ssl_i2c_initialized = (init_status == HAL_OK);
}

HAL_StatusTypeDef SSL_I2cBus_MemWrite(
    uint16_t device_address,
    uint16_t register_address,
    const uint8_t *data,
    uint16_t length,
    uint32_t timeout_ms)
{
  SSL_I2cBus_Init();
  return HAL_I2C_Mem_Write(
      &g_ssl_i2c_handle,
      device_address << 1,
      register_address,
      I2C_MEMADD_SIZE_8BIT,
      (uint8_t *)data,
      length,
      timeout_ms);
}

HAL_StatusTypeDef SSL_I2cBus_MemRead(
    uint16_t device_address,
    uint16_t register_address,
    uint8_t *data,
    uint16_t length,
    uint32_t timeout_ms)
{
  SSL_I2cBus_Init();
  return HAL_I2C_Mem_Read(
      &g_ssl_i2c_handle,
      device_address << 1,
      register_address,
      I2C_MEMADD_SIZE_8BIT,
      data,
      length,
      timeout_ms);
}

bool SSL_I2cBus_IsSdaLow(void)
{
  SSL_I2cBus_Init();
  return (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_RESET);
}

void SSL_I2cBus_SdaUnlock(void)
{
  GPIO_InitTypeDef gpio_init = {0};
  uint32_t cycle = 0U;

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_I2C1_FORCE_RESET();
  __HAL_RCC_I2C1_RELEASE_RESET();

  gpio_init.Pin = GPIO_PIN_6;
  gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
  gpio_init.Pull = GPIO_PULLUP;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio_init);

  gpio_init.Pin = GPIO_PIN_7;
  gpio_init.Mode = GPIO_MODE_INPUT;
  HAL_GPIO_Init(GPIOB, &gpio_init);

  for (cycle = 0U; cycle < 100U; ++cycle)
  {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_Delay(1U);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_Delay(1U);

    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_SET)
    {
      break;
    }
  }

  SSL_I2cBus_InitPinsAlternate();
  g_ssl_i2c_initialized = false;
  SSL_I2cBus_Init();
}

static void SSL_I2cBus_InitPinsAlternate(void)
{
  GPIO_InitTypeDef gpio_init = {0};

  gpio_init.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  gpio_init.Mode = GPIO_MODE_AF_OD;
  gpio_init.Pull = GPIO_PULLUP;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init.Alternate = GPIO_AF4_I2C1;
  HAL_GPIO_Init(GPIOB, &gpio_init);
}
