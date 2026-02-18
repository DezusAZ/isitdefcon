# IsItDefcon - M5Stack CoreS3 SE Firmware

Standalone DEF CON countdown device that hosts its own web server.

## Hardware Required

- M5Stack CoreS3 SE
- W5500 Ethernet Unit (or compatible)
- Ethernet cable + network with DHCP

## Pin Configuration

The default pin mapping is for **Port A** on the CoreS3 SE:

| Signal | GPIO |
|--------|------|
| SCK    | 1    |
| MISO   | 2    |
| MOSI   | 8    |
| CS     | 9    |

If using a different port, edit the `ETH_*_PIN` defines in the sketch.

## Libraries Required

Install via Arduino Library Manager:

1. **M5CoreS3** - M5Stack CoreS3 library
2. **Ethernet** - Built-in Arduino Ethernet library
3. **ArduinoJson** - JSON parsing (optional, for future extensions)

The sketch also uses ESP32's built-in `WiFiClientSecure` for HTTPS requests.

## Arduino IDE Setup

1. Add ESP32 board support:
   - Preferences > Additional Boards Manager URLs:
   - `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`

2. Install board: **ESP32 by Espressif Systems**

3. Select board: **M5Stack-CoreS3**

4. Install libraries listed above

5. Upload `isitdefcon.ino`

## Customizing for Different Events

Edit these values at the top of `isitdefcon.ino`:

```cpp
const char* EVENT_NAME = "DEF CON 34";
const char* EVENT_QUESTION = "Is it DEF CON?";
const char* YES_MESSAGE = "YES! Go hack something.";
const char* NO_MESSAGE = "NO.";

const int EVENT_START_YEAR = 2026;
const int EVENT_START_MONTH = 8;
const int EVENT_START_DAY = 6;
const int EVENT_END_YEAR = 2026;
const int EVENT_END_MONTH = 8;
const int EVENT_END_DAY = 9;
```

For a different RSS feed (or to disable), edit `RSS_URL`.

## Features

- **LCD Display**: Shows YES/NO answer, countdown timer, scrolling news ticker
- **Web Server**: Visit the device's IP in a browser for the full web UI
- **NTP Sync**: Automatic time synchronization
- **DEF CON RSS**: Fetches live news every 15 minutes
- **API Endpoints**:
  - `GET /` - Web UI
  - `GET /api/check` - JSON status
  - `GET /api/news` - JSON news items

## Network

The device gets an IP via DHCP and displays it on screen. Access the web UI at:

```
http://<device-ip>/
```

## Troubleshooting

- **"Ethernet FAILED"**: Check cable, W5500 unit, and pin configuration
- **No news items**: RSS fetch may have failed; check serial monitor
- **Wrong time**: NTP sync failed; check network connectivity
