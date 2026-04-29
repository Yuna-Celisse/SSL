#include "ssl_esp32_link.h"

#include <string.h>

#define SSL_ESP32_RX_BUFFER_SIZE 128U

static const SslEsp32LinkConfig *g_ssl_esp32_config = NULL;
static uint8_t g_ssl_esp32_rx_buffer[SSL_ESP32_RX_BUFFER_SIZE];
static size_t g_ssl_esp32_rx_length = 0U;

static void SSL_Esp32Link_InitControlPin(const SslEsp32ControlPin *pin, bool level);
static void SSL_Esp32Link_WriteControlPin(const SslEsp32ControlPin *pin, bool level);
static void SSL_Esp32Link_PollRx(void);
static void SSL_Esp32Link_SendFrame(const uint8_t *frame, size_t length);

void SSL_Esp32Link_Init(const SslEsp32LinkConfig *config)
{
  g_ssl_esp32_config = config;
  g_ssl_esp32_rx_length = 0U;
  SSL_Esp32Link_InitControlPin(&config->enable_pin, false);
  SSL_Esp32Link_InitControlPin(&config->boot_pin, true);
  SSL_Uart_Init(&config->uart_port, false);
  SSL_Esp32Link_ResetModule();
}

bool SSL_Esp32Link_TryReadCommand(SslControlCommand *command)
{
  SslEsp32Frame frame;
  size_t consumed_length = 0U;
  bool decoded = false;

  SSL_Esp32Link_PollRx();

  if (!SSL_Esp32Protocol_TryDecodeFrame(g_ssl_esp32_rx_buffer, g_ssl_esp32_rx_length, &frame, &consumed_length))
  {
    if ((consumed_length > 0U) && (consumed_length <= g_ssl_esp32_rx_length))
    {
      memmove(
          g_ssl_esp32_rx_buffer,
          &g_ssl_esp32_rx_buffer[consumed_length],
          g_ssl_esp32_rx_length - consumed_length);
      g_ssl_esp32_rx_length -= consumed_length;
    }
    return false;
  }

  memmove(
      g_ssl_esp32_rx_buffer,
      &g_ssl_esp32_rx_buffer[consumed_length],
      g_ssl_esp32_rx_length - consumed_length);
  g_ssl_esp32_rx_length -= consumed_length;

  decoded = SSL_Esp32Protocol_FrameToCommand(&frame, command);
  if (!decoded)
  {
    SSL_Esp32Link_SendError(0x02U);
  }

  return decoded;
}

void SSL_Esp32Link_SendAck(void)
{
  uint8_t frame[SSL_ESP32_MAX_FRAME_SIZE];
  size_t frame_length = 0U;

  if (SSL_Esp32Protocol_EncodeAck(frame, &frame_length))
  {
    SSL_Esp32Link_SendFrame(frame, frame_length);
  }
}

void SSL_Esp32Link_SendError(uint8_t error_code)
{
  uint8_t frame[SSL_ESP32_MAX_FRAME_SIZE];
  size_t frame_length = 0U;

  if (SSL_Esp32Protocol_EncodeError(error_code, frame, &frame_length))
  {
    SSL_Esp32Link_SendFrame(frame, frame_length);
  }
}

void SSL_Esp32Link_SendStatus(const SslEsp32StatusPayload *status)
{
  uint8_t frame[SSL_ESP32_MAX_FRAME_SIZE];
  size_t frame_length = 0U;

  if (SSL_Esp32Protocol_EncodeStatus(status, frame, &frame_length))
  {
    SSL_Esp32Link_SendFrame(frame, frame_length);
  }
}

void SSL_Esp32Link_ResetModule(void)
{
  SSL_Esp32Link_WriteControlPin(&g_ssl_esp32_config->boot_pin, true);
  SSL_Esp32Link_WriteControlPin(&g_ssl_esp32_config->enable_pin, false);
  HAL_Delay(10U);
  SSL_Esp32Link_WriteControlPin(&g_ssl_esp32_config->enable_pin, true);
  HAL_Delay(50U);
}

static void SSL_Esp32Link_InitControlPin(const SslEsp32ControlPin *pin, bool level)
{
  GPIO_InitTypeDef gpio_init = {0};

  if (pin->port == GPIOA)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
  }
  else if (pin->port == GPIOB)
  {
    __HAL_RCC_GPIOB_CLK_ENABLE();
  }
  else if (pin->port == GPIOC)
  {
    __HAL_RCC_GPIOC_CLK_ENABLE();
  }
  else if (pin->port == GPIOD)
  {
    __HAL_RCC_GPIOD_CLK_ENABLE();
  }

  gpio_init.Pin = pin->pin;
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Pull = GPIO_PULLUP;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(pin->port, &gpio_init);
  SSL_Esp32Link_WriteControlPin(pin, level);
}

static void SSL_Esp32Link_WriteControlPin(const SslEsp32ControlPin *pin, bool level)
{
  GPIO_PinState pin_state = GPIO_PIN_RESET;

  if ((level && pin->active_high) || ((!level) && (!pin->active_high)))
  {
    pin_state = GPIO_PIN_SET;
  }

  HAL_GPIO_WritePin(pin->port, pin->pin, pin_state);
}

static void SSL_Esp32Link_PollRx(void)
{
  USART_TypeDef *instance = g_ssl_esp32_config->uart_port.instance;
  volatile uint32_t discarded = 0U;

  if ((instance->SR & (USART_SR_ORE | USART_SR_NE | USART_SR_FE)) != 0U)
  {
    discarded = instance->DR;
    (void)discarded;
    g_ssl_esp32_rx_length = 0U;
  }

  while (((instance->SR & USART_SR_RXNE) != 0U) && (g_ssl_esp32_rx_length < SSL_ESP32_RX_BUFFER_SIZE))
  {
    g_ssl_esp32_rx_buffer[g_ssl_esp32_rx_length] = (uint8_t)(instance->DR & 0xFFU);
    ++g_ssl_esp32_rx_length;
  }

  if (((instance->SR & USART_SR_RXNE) != 0U) && (g_ssl_esp32_rx_length >= SSL_ESP32_RX_BUFFER_SIZE))
  {
    discarded = instance->DR;
    (void)discarded;
    g_ssl_esp32_rx_length = 0U;
  }
}

static void SSL_Esp32Link_SendFrame(const uint8_t *frame, size_t length)
{
  SSL_Uart_WriteBuffer(g_ssl_esp32_config->uart_port.instance, frame, length);
}
