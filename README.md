# ESP32 Weather Station OLED

Firmware for a LilyGO T-Display S3 (ESP32-S3) that displays weather data:
temperature, humidity, pressure, and wind speed, pulled over MQTT from a
Zigbee/anemometer sensor setup, with NTP time sync and OTA firmware updates.

## Hardware

- [LilyGO T-Display S3](boards/lilygo-t-displays3.json) (ESP32-S3, "basic"
  non-touch variant), 170x320 ST7789 parallel IPS LCD, 16MB flash.
- LCD power gate on GPIO15: must be driven HIGH at boot or the panel stays
  unpowered even though `tft.init()` reports success.
- LCD backlight on GPIO38, driven by LEDC PWM (channel 0, 10kHz, 8-bit) so
  brightness can be adjusted in steps rather than just on/off.
- Two physical buttons:
  - **Boot** (GPIO0): click cycles through 5 preset brightness levels.
  - **IO14**: long-press triggers a reboot (`ESP.restart()`).

## Build

Built with [PlatformIO](https://platformio.org/).

```
pio run
pio run -t upload
```

The upload port is hardcoded in `platformio.ini` (`upload_port = COM11`);
change it to match whatever COM port the board enumerates as on your machine.

## Configuration

Copy [`include/connectionDetails.example.h`](include/connectionDetails.example.h)
to `include/connectionDetails.h` and fill in your WiFi, MQTT broker, and OTA
update credentials. This file is gitignored and never committed.

Runtime settings (screen orientation, brightness, MQTT topic override) are
also persisted to `/config.ini` on the device's SPIFFS filesystem, and are
read back on every boot via `loadCustomParamsSPIFFS()`. These can be changed
at runtime through the web UI (see below) without reflashing.

## Features

- MQTT-based temperature/humidity/pressure and wind speed readouts, parsed
  as JSON from a configurable topic (default: a Zigbee2MQTT temperature/
  humidity/pressure sensor topic, plus a separate anemometer wind speed
  topic).
- Background FreeRTOS tasks keep WiFi and MQTT connections alive
  independently of the main render loop, retrying automatically on
  disconnect.
- NTP-synced clock display, with basic BST (British Summer Time) handling
  so displayed local time adjusts automatically across the DST boundary.
- A small built-in web server (on port 80) that shows live status (WiFi
  SSID/signal strength, last MQTT message, IP/MAC address, firmware
  version) and exposes a settings form for screen flip, brightness, and
  MQTT topic, plus `/reset` and `/resetSettings` endpoints.
- OTA firmware updates via the same web server (`/update`, POST a compiled
  `.bin`).
- Button input handling (via the [OneButton](include/lib/OneButton) library)
  for brightness cycling and reboot, described under Hardware above.
- On-screen rolling debug log (`DisplayOut`) shown during startup, so boot
  progress is visible on the LCD even before WiFi/MQTT come up.
