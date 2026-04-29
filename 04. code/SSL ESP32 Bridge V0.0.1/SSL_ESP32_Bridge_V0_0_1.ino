#include <WiFi.h>

#include "wifi_config.h"

static WiFiServer g_ssl_server(SSL_WIFI_TCP_PORT);
static WiFiClient g_ssl_client;
static HardwareSerial g_ssl_uart(1);

static void SSL_Bridge_ConnectWifi(void);
static void SSL_Bridge_AcceptClient(void);
static void SSL_Bridge_ForwardTcpToUart(void);
static void SSL_Bridge_ForwardUartToTcp(void);

void setup(void)
{
  g_ssl_uart.begin(SSL_UART_BAUDRATE, SERIAL_8N1, SSL_UART_RX_PIN, SSL_UART_TX_PIN);
  SSL_Bridge_ConnectWifi();
  g_ssl_server.begin();
  g_ssl_server.setNoDelay(true);
}

void loop(void)
{
  SSL_Bridge_AcceptClient();

  if (g_ssl_client && g_ssl_client.connected())
  {
    SSL_Bridge_ForwardTcpToUart();
    SSL_Bridge_ForwardUartToTcp();
  }
  else
  {
    if (g_ssl_client)
    {
      g_ssl_client.stop();
    }
    delay(20);
  }
}

static void SSL_Bridge_ConnectWifi(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSL_WIFI_SSID, SSL_WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }
}

static void SSL_Bridge_AcceptClient(void)
{
  if (g_ssl_client && g_ssl_client.connected())
  {
    return;
  }

  WiFiClient next_client = g_ssl_server.available();
  if (!next_client)
  {
    return;
  }

  if (g_ssl_client)
  {
    g_ssl_client.stop();
  }

  g_ssl_client = next_client;
  g_ssl_client.setNoDelay(true);
}

static void SSL_Bridge_ForwardTcpToUart(void)
{
  while (g_ssl_client.available() > 0)
  {
    g_ssl_uart.write((uint8_t)g_ssl_client.read());
  }
}

static void SSL_Bridge_ForwardUartToTcp(void)
{
  while (g_ssl_uart.available() > 0)
  {
    g_ssl_client.write((uint8_t)g_ssl_uart.read());
  }
}
