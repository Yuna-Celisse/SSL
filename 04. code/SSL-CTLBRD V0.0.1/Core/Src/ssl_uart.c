#include "ssl_uart.h"

static void SSL_EnableGpioClock(GPIO_TypeDef *port);
static void SSL_EnableUartClock(USART_TypeDef *instance);
static uint32_t SSL_GetUartClock(USART_TypeDef *instance);

void SSL_Uart_Init(const SslUartPort *port, bool enable_rx_irq)
{
  GPIO_InitTypeDef gpio_init = {0};
  uint32_t uart_div = 0U;

  SSL_EnableGpioClock(port->tx_port);
  SSL_EnableGpioClock(port->rx_port);
  SSL_EnableUartClock(port->instance);

  gpio_init.Pin = port->tx_pin;
  gpio_init.Mode = GPIO_MODE_AF_PP;
  gpio_init.Pull = GPIO_PULLUP;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init.Alternate = port->gpio_af;
  HAL_GPIO_Init(port->tx_port, &gpio_init);

  gpio_init.Pin = port->rx_pin;
  HAL_GPIO_Init(port->rx_port, &gpio_init);

  port->instance->CR1 = 0U;
  port->instance->CR2 = 0U;
  port->instance->CR3 = 0U;

  uart_div = (SSL_GetUartClock(port->instance) + (port->baudrate / 2U)) / port->baudrate;
  port->instance->BRR = uart_div;
  port->instance->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

  if (enable_rx_irq)
  {
    port->instance->CR1 |= USART_CR1_RXNEIE;
    HAL_NVIC_SetPriority(USART1_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
  }
}

void SSL_Uart_WriteByte(USART_TypeDef *instance, uint8_t data)
{
  while ((instance->SR & USART_SR_TXE) == 0U)
  {
  }

  instance->DR = data;
}

void SSL_Uart_WriteBuffer(USART_TypeDef *instance, const uint8_t *buffer, size_t length)
{
  size_t index = 0U;

  for (index = 0U; index < length; ++index)
  {
    SSL_Uart_WriteByte(instance, buffer[index]);
  }
}

static void SSL_EnableGpioClock(GPIO_TypeDef *port)
{
  if (port == GPIOA)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
  }
  else if (port == GPIOB)
  {
    __HAL_RCC_GPIOB_CLK_ENABLE();
  }
  else if (port == GPIOC)
  {
    __HAL_RCC_GPIOC_CLK_ENABLE();
  }
  else if (port == GPIOD)
  {
    __HAL_RCC_GPIOD_CLK_ENABLE();
  }
  else if (port == GPIOE)
  {
    __HAL_RCC_GPIOE_CLK_ENABLE();
  }
  else if (port == GPIOF)
  {
    __HAL_RCC_GPIOF_CLK_ENABLE();
  }
  else if (port == GPIOG)
  {
    __HAL_RCC_GPIOG_CLK_ENABLE();
  }
}

static void SSL_EnableUartClock(USART_TypeDef *instance)
{
  if (instance == USART1)
  {
    __HAL_RCC_USART1_CLK_ENABLE();
  }
  else if (instance == USART2)
  {
    __HAL_RCC_USART2_CLK_ENABLE();
  }
  else if (instance == USART3)
  {
    __HAL_RCC_USART3_CLK_ENABLE();
  }
  else if (instance == UART4)
  {
    __HAL_RCC_UART4_CLK_ENABLE();
  }
  else if (instance == UART5)
  {
    __HAL_RCC_UART5_CLK_ENABLE();
  }
  else if (instance == USART6)
  {
    __HAL_RCC_USART6_CLK_ENABLE();
  }
}

static uint32_t SSL_GetUartClock(USART_TypeDef *instance)
{
  if ((instance == USART1) || (instance == USART6))
  {
    return HAL_RCC_GetPCLK2Freq();
  }

  return HAL_RCC_GetPCLK1Freq();
}
