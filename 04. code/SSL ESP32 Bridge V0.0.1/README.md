# SSL ESP32 Bridge V0.0.1

`ESP32-C5-WROOM-1U` Wi-Fi to UART transparent bridge for the SSL chassis controller.

## Purpose

- Connect `ESP32-C5-WROOM-1U` to a 5 GHz Wi-Fi router in station mode
- Listen on TCP port `9000`
- Transparently forward TCP bytes to STM32 `USART6`
- Transparently forward STM32 `USART6` bytes back to the TCP client

## Default UART wiring

- `ESP32 TX0 / GPIO11` -> `STM32 PC7 / USART6_RX`
- `ESP32 RX0 / GPIO12` -> `STM32 PC6 / USART6_TX`
- `ESP32 EN` -> `STM32 PB0`
- `ESP32 GPIO28` -> `STM32 PB1`
- `3V3` -> `3V3`
- `GND` -> `GND`

`GPIO28` is the boot strap pin used here for normal boot control. Keep it high for normal startup and pull it low only if you intentionally enter download mode during reset.

## Build environment

This sketch is written for Arduino core on ESP32-C5.

## Configure Wi-Fi

Edit `wifi_config.h`:

- `SSL_WIFI_SSID`
- `SSL_WIFI_PASSWORD`

The bridge uses station mode, so the module can associate with an existing 5 GHz-capable AP.

## Network behavior

- Single TCP client at a time
- TCP port: `9000`
- UART baudrate: `921600`

## Protocol

The bridge is transport-transparent. The application protocol is defined by the STM32 controller and the PC host tool. The ESP32 firmware does not parse commands; it forwards bytes only.
