# DEF CON Countdown

Countdown timer for DEF CON 34 (Aug 6-9, 2026) on M5Stack CoreS3 SE.

## Features

- Live countdown on LCD display
- Web UI with countdown, resources, and WiFi config
- Ethernet (LAN Module 13.2) or WiFi connectivity
- NTP time sync
- News ticker from DEF CON RSS

## Quick Start

**Flash:** Use M5Burner or `esptool.py --chip esp32s3 --port /dev/ttyACM0 write_flash 0x0 firmware.bin`

**Ethernet:** Plug in LAN module, auto DHCP.

**WiFi:** Connect to `DEFCON-SETUP` AP, open browser, configure WiFi.

Device IP shows on bottom of display. Access web UI at `http://<device-ip>`

## Build

```bash
arduino-cli compile --fqbn esp32:esp32:m5stack_cores3 firmware/isitdefcon/
arduino-cli upload --fqbn esp32:esp32:m5stack_cores3 -p /dev/ttyACM0 firmware/isitdefcon/
```

**Libraries:** M5Unified, M5GFX, M5_Ethernet

## Web Server (Optional Dev Tool)

```bash
npm install
node server.js
```

Access at `http://localhost:5000`

## Hardware

- M5Stack CoreS3 SE
- LAN Module 13.2 (W5500) - optional

Created by DezusAZ
