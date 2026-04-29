# SSL Host WiFi Console V0.0.1

Desktop upper-computer tool for the SSL chassis Wi-Fi link.

## Runtime

- Python 3.10+
- Standard library only

## Start

```powershell
python .\main.py
```

## Features

- Connect to ESP32 bridge over TCP
- Send `VEL`
- Send `RAW`
- Send `STOP`
- Request `STATUS`
- Send `PING`
- Display binary status feedback from STM32

## Default network

- TCP server IP: set in GUI
- TCP port: `9000`

## Protocol

The tool uses the same binary frame protocol as the STM32 `ssl_esp32_protocol` module.
