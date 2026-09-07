# ESP32 Weather Station OLED

Firmware for a LilyGO T-Display S3 (ESP32-S3) that displays weather data —
temperature, humidity, pressure, and wind speed — pulled over MQTT from a
Zigbee/anemometer sensor setup, with NTP time sync and OTA firmware updates.

## Hardware

- [LilyGO T-Display S3](boards/lilygo-t-displays3.json) (ESP32-S3, built-in TFT)

## Build

Built with [PlatformIO](https://platformio.org/).

```
pio run
pio run -t upload
```

## Configuration

Copy [`include/connectionDetails.example.h`](include/connectionDetails.example.h)
to `include/connectionDetails.h` and fill in your WiFi, MQTT broker, and OTA
update credentials. This file is gitignored and never committed.

## Features

- MQTT-based temperature/humidity/pressure and wind speed readouts
- NTP-synced clock display
- OTA firmware updates via web server
- Button input handling (OneButton)
