# IsItDefcon

A countdown timer for DEF CON events, built on the M5Stack CoreS3 SE with optional LAN Module 13.2 (W5500) Ethernet adapter.

## Features

- Live countdown to the next DEF CON event (DEF CON 34: August 6-9, 2026)
- Automatic event detection and countdown switching
- Network connectivity via WiFi or Ethernet (LAN Module 13.2)
- Web UI accessible from any device on the same network
- NTP time synchronization
- Hacker/terminal-style green-on-black aesthetic
- News feed from DEF CON RSS

## Hardware

- **Main Board**: M5Stack CoreS3 SE (ESP32-S3)
- **Optional**: LAN Module 13.2 (W5500) for Ethernet connectivity
- **Display**: 2.0" IPS LCD (320x240)

## Network Modes

1. **Ethernet Mode**: Plug in the LAN Module and connect an Ethernet cable. DHCP is automatic.
2. **WiFi Mode**: If no Ethernet is detected, the device creates an AP named `DEFCON-SETUP` for WiFi configuration.

## Web Interface

Once connected, access the web UI at the device's IP address (shown on the display). The unified interface shows:

- DEF CON countdown timer
- Event information and links
- WiFi configuration (scan networks, connect, clear credentials)

## API Endpoints

- `GET /` - Main web interface
- `GET /api/check` - JSON countdown data
- `GET /api/news` - JSON news feed
- `GET /scan` - Scan for WiFi networks
- `POST /connect` - Save WiFi credentials
- `GET /clear` - Clear saved WiFi credentials

## Building

### Requirements

- Arduino IDE or arduino-cli
- ESP32 board support package
- Libraries:
  - M5Unified
  - M5GFX
  - M5_Ethernet (for LAN module)

### Compile & Flash

```bash
arduino-cli compile --fqbn esp32:esp32:m5stack_cores3 firmware/isitdefcon/
arduino-cli upload --fqbn esp32:esp32:m5stack_cores3 -p /dev/ttyACM0 firmware/isitdefcon/
```

## Project Structure

```
isitdefcon/
├── firmware/
│   └── isitdefcon/
│       └── isitdefcon.ino    # Main firmware
├── server.js                  # Development server (optional)
├── CLAUDE.md                  # Hardware reference documentation
└── README.md
```

## Credits

Created by DezusAZ (@DezusAZ)

## License

MIT License
