# IsItDefcon Project - Hardware Reference

This document contains all hardware specifications, pin configurations, and reference code for the M5Stack CoreS3 SE with LAN Module 13.2 (W5500) Ethernet adapter.

---

## Table of Contents

1. [Hardware Overview](#hardware-overview)
2. [CoreS3 SE Specifications](#cores3-se-specifications)
3. [GPIO Pin Mapping](#gpio-pin-mapping)
4. [LAN Module 13.2 (W5500)](#lan-module-132-w5500)
5. [SPI Bus Configuration](#spi-bus-configuration)
6. [Known Issues & Solutions](#known-issues--solutions)
7. [Library Dependencies](#library-dependencies)
8. [Reference Code Examples](#reference-code-examples)
9. [NTP Time Synchronization](#ntp-time-synchronization)
10. [Web Server Implementation](#web-server-implementation)
11. [HTTPS Requests](#https-requests)
12. [Display & Graphics](#display--graphics)
13. [Resources & Links](#resources--links)

---

## Hardware Overview

### Target Hardware
- **Main Board**: M5Stack CoreS3 SE (SKU: K128-SE)
- **Ethernet Module**: LAN Module 13.2 (W5500)
- **Connection**: Module stacks via M-Bus connector

### Key Differences: CoreS3 SE vs CoreS3
The CoreS3 SE is a lightweight version. These features are **NOT available** on CoreS3 SE:
- Camera (GC0308)
- Proximity sensor (LTR-553ALS)
- IMU (BMI270)
- Magnetometer (BMM150)
- Battery (500mAh) - SE has no battery

Code using these peripherals will not work on CoreS3 SE.

---

## CoreS3 SE Specifications

### Processor & Memory
| Component | Specification |
|-----------|---------------|
| SoC | ESP32-S3 (Xtensa LX7 dual-core @ 240MHz) |
| Flash | 16MB |
| PSRAM | 8MB |
| SRAM | 512KB on-chip |
| Wi-Fi | 2.4GHz 802.11b/g/n |
| Bluetooth | BLE 5.0 |

### Display
| Component | Specification |
|-----------|---------------|
| Type | 2.0" IPS LCD (capacitive touch) |
| Resolution | 320 x 240 pixels |
| LCD Controller | ILI9342C (SPI) |
| Touch Controller | FT6336U (I2C) |
| Touch Resolution | 320 x 280 |

### Audio
| Component | Specification |
|-----------|---------------|
| Speaker | 1W |
| Amplifier | AW88298 (16-bit I2S) |
| Audio Codec | ES7210 |
| Microphones | Dual built-in |

### Power Management
| Component | Specification |
|-----------|---------------|
| PMIC | AXP2101 |
| IO Expander | AW9523B |
| RTC | BM8563 (PCF8563 compatible) |
| USB | Type-C with OTG/CDC |
| Charging Current | 5V @ 198mA |

### Physical
| Property | Value |
|----------|-------|
| Dimensions | 54.0 x 54.0 x 15.5mm |
| Weight | 37.8g |
| Operating Temp | 0-40C |

---

## GPIO Pin Mapping

### Grove Ports (External)

| Port | Color | GPIO Pins | Function | Notes |
|------|-------|-----------|----------|-------|
| Port A | Red | GPIO1 (SCL), GPIO2 (SDA) | I2C | External I2C bus |
| Port B | Black | GPIO8, GPIO9 | Free GPIO | Can be used for SPI CS |
| Port C | Blue | GPIO17 (RX), GPIO18 (TX) | UART/GPIO | Serial communication |

### Display SPI Bus

| Signal | GPIO | Notes |
|--------|------|-------|
| SCLK | GPIO36 | Shared with SD card |
| MOSI | GPIO37 | Shared with SD card |
| MISO | GPIO35 | Also used as D/C in 3-wire mode |
| LCD CS | GPIO3 | Display chip select |
| SD CS | GPIO4 | SD card chip select |

**WARNING**: GPIO35, GPIO36, GPIO37 are used by PSRAM on ESP32-S3R8 variants. Sharing the SPI bus between display and external modules requires careful management.

### Internal I2C Bus

| Signal | GPIO | Devices |
|--------|------|---------|
| SDA | GPIO12 | AXP2101, AW9523B, BM8563, FT6336U |
| SCL | GPIO11 | Same devices |

### Audio (I2S)

| Signal | GPIO | Notes |
|--------|------|-------|
| MCLK | GPIO0 | Master clock |
| BCLK | GPIO34 | Bit clock |
| LRCK | GPIO33 | Left/Right clock |
| DATA IN | GPIO14 | Microphone input |
| DATA OUT | GPIO13 | Speaker output |

**IMPORTANT**: GPIO14 is used for microphone input. This conflicts with W5500 interrupt pin on some configurations.

### Touch

| Signal | GPIO |
|--------|------|
| Touch IRQ | GPIO21 |
| Touch Reset | Via AW9523B (pin 0) |

---

## LAN Module 13.2 (W5500)

### Specifications
| Property | Value |
|----------|-------|
| Ethernet Controller | W5500 (hardware TCP/IP) |
| Interface | SPI |
| Network Port | RJ45 (10/100Mbps) |
| Input Voltage | 9-24V DC (DC-JACK) |
| Protocols | TCP, UDP, IPv4, ICMP, ARP, IGMP, PPPoE |
| Dimensions | 54 x 54 x 13.2mm |

### Pin Configuration for CoreS3

Based on M5Module-LAN-13.2 library examples:

| Signal | GPIO | Notes |
|--------|------|-------|
| SPI CS | GPIO1 | Via Port A or M-Bus |
| SPI SCLK | (shared) | Uses internal SPI bus |
| SPI MOSI | (shared) | Uses internal SPI bus |
| SPI MISO | (shared) | Uses internal SPI bus |
| Reset | GPIO0 | Module reset pin |
| Interrupt | GPIO10 | Status interrupt |

**Alternative configuration for M-Bus connection:**

| Signal | GPIO | Notes |
|--------|------|-------|
| CS | GPIO9 | Port B GPIO |
| RST | GPIO7 | Reset |
| INT | GPIO14 | **Conflicts with mic!** |

### Pin Configuration Comparison (Different Boards)

| Board | CS | RST | INT |
|-------|------|------|------|
| M5Stack Basic | GPIO5 | GPIO0 | GPIO35 |
| M5Stack Core2 | GPIO33 | GPIO0 | GPIO35 |
| **M5Stack CoreS3** | GPIO1 | GPIO0 | GPIO10 |

---

## SPI Bus Configuration

### Critical Notes

1. **Display and SD share SPI bus**: LCD and SD card use the same SPI bus (GPIO36, GPIO37, GPIO35)
2. **W5500 should NOT share display SPI**: The W5500 needs its own SPI initialization or careful bus sharing
3. **SPI Clock Speed Issue**: Default SPI clock may be too fast. Reduce to **8MHz** if experiencing issues

### Recommended W5500 SPI Setup

```cpp
// For M5Stack CoreS3 with LAN Module 13.2
#define W5500_CS_PIN    1   // Port A or designated GPIO
#define W5500_RST_PIN   0
#define W5500_INT_PIN   10

// Initialize SPI for W5500 (separate from display SPI if possible)
SPI.begin();  // Uses default SPI pins

// Or explicit pin configuration for custom setup:
// SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
```

### SPI Clock Speed Fix

If experiencing WebServer timeouts or connection issues, modify the Ethernet library:

In `libdeps/M5-Ethernet/src/utility/w5100.h`, change lines 21-22:
```cpp
// Change from default (14MHz+) to 8MHz
SPISettings(8000000, MSBFIRST, SPI_MODE0)
```

---

## Known Issues & Solutions

### Issue 1: GPIO14 Conflict (Microphone vs W5500 Interrupt)

**Problem**: W5500 interrupt pin conflicts with I2S microphone DATA_IN

**Solution**: Disable microphone in M5Unified config:
```cpp
auto cfg = M5.config();
cfg.output_power = false;
cfg.internal_mic = false;
M5.begin(cfg);
```

Or use W5500 in **polling mode** (no interrupt pin).

### Issue 2: SPI Bus Contention (Display + W5500)

**Problem**: Both display and W5500 try to use the same SPI bus

**Solutions**:
1. Use different CS pins and share the bus carefully
2. Initialize M5 display first, then reconfigure SPI for W5500
3. Use explicit SPI bus management with `SPI.beginTransaction()` / `SPI.endTransaction()`

### Issue 3: DHCP Not Working / Random IP

**Problem**: W5500 gets wrong IP or 0.0.0.0

**Solutions**:
1. Check physical connections (especially breadboard issues)
2. Reduce SPI clock speed to 8MHz
3. Add delay after reset: `delay(500)` after `LAN.reset()`
4. Try static IP first to isolate issues

### Issue 4: WebServer Stops Responding After ~15 Minutes

**Problem**: `client.available()` stays false after connection

**Solution**: Reduce SPI clock speed to 8MHz (see above)

### Issue 5: ESP32 NTP Library Incompatibility with Ethernet

**Problem**: `configTime()` / `esp_sntp` doesn't work with Ethernet

**Solution**: Use manual UDP NTP implementation instead of ESP32's built-in NTP. See NTP section below.

---

## Library Dependencies

### Required Libraries

| Library | Purpose | Install Via |
|---------|---------|-------------|
| M5Unified | Main M5Stack API | Arduino Library Manager |
| M5GFX | Graphics/Display | Arduino Library Manager |
| M5_Ethernet | W5500 Ethernet | Arduino Library Manager |
| ArduinoHttpClient | HTTP client | Arduino Library Manager |

### Arduino IDE Board Configuration

1. **Board Manager URL**: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. **Board**: ESP32S3 Dev Module or M5Stack-CoreS3
3. **Settings**:
   - Flash Size: 16MB
   - PSRAM: OPI PSRAM
   - USB CDC On Boot: Enabled (for serial monitor)
   - USB Mode: Hardware CDC and JTAG

### PlatformIO Configuration

```ini
[env:m5stack-cores3]
platform = espressif32
board = m5stack-cores3
framework = arduino
monitor_speed = 115200
lib_deps =
    m5stack/M5Unified
    m5stack/M5GFX
    m5stack/M5_Ethernet
    arduino-libraries/ArduinoHttpClient
build_flags =
    -DARDUINO_M5STACK_CORES3
    -DBOARD_HAS_PSRAM
```

---

## Reference Code Examples

### Basic M5Unified Initialization

```cpp
#include <M5Unified.h>

void setup() {
    auto cfg = M5.config();

    // For CoreS3 SE with Ethernet (disable conflicting peripherals)
    cfg.internal_mic = false;  // Disable mic if using GPIO14 for W5500 INT

    M5.begin(cfg);

    M5.Display.setTextSize(2);
    M5.Display.println("CoreS3 SE Ready");
}

void loop() {
    M5.update();  // Required for touch/button handling
}
```

### W5500 Ethernet Initialization (CoreS3)

```cpp
#include <M5Unified.h>
#include <M5_Ethernet.h>
#include <SPI.h>

// Pin definitions for CoreS3 with LAN Module 13.2
#define W5500_CS   1
#define W5500_RST  0
#define W5500_INT  10

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

void setup() {
    auto cfg = M5.config();
    cfg.internal_mic = false;  // Avoid GPIO14 conflict
    M5.begin(cfg);

    // Initialize SPI
    SPI.begin();

    // Configure W5500
    Ethernet.init(W5500_CS);

    // Optional: Reset the module
    pinMode(W5500_RST, OUTPUT);
    digitalWrite(W5500_RST, LOW);
    delay(100);
    digitalWrite(W5500_RST, HIGH);
    delay(500);

    // Start Ethernet with DHCP
    M5.Display.println("Getting IP...");

    while (Ethernet.begin(mac) == 0) {
        M5.Display.println("DHCP failed, retrying...");
        delay(2000);
    }

    M5.Display.print("IP: ");
    M5.Display.println(Ethernet.localIP());
}
```

### Simple WebServer

```cpp
#include <M5Unified.h>
#include <M5_Ethernet.h>
#include <SPI.h>

EthernetServer server(80);

void setup() {
    // ... Ethernet init (see above) ...
    server.begin();
}

void loop() {
    EthernetClient client = server.available();

    if (client) {
        boolean currentLineIsBlank = true;
        String request = "";

        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                request += c;

                if (c == '\n' && currentLineIsBlank) {
                    // Send HTTP response
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println("Connection: close");
                    client.println();
                    client.println("<!DOCTYPE HTML>");
                    client.println("<html><body>");
                    client.println("<h1>Hello from CoreS3 SE!</h1>");
                    client.println("</body></html>");
                    break;
                }

                if (c == '\n') {
                    currentLineIsBlank = true;
                } else if (c != '\r') {
                    currentLineIsBlank = false;
                }
            }
        }

        delay(1);
        client.stop();
    }
}
```

---

## NTP Time Synchronization

### Manual UDP NTP (Works with Ethernet)

The ESP32's built-in `configTime()` function does NOT work reliably with Ethernet. Use manual UDP NTP instead:

```cpp
#include <EthernetUdp.h>

EthernetUDP udp;
const char* ntpServer = "pool.ntp.org";
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

void syncNTP() {
    udp.begin(8888);  // Local port

    // Resolve NTP server
    IPAddress ntpIP;
    if (!Ethernet.hostByName(ntpServer, ntpIP)) {
        Serial.println("DNS lookup failed");
        return;
    }

    // Build NTP request packet
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;  // LI, Version, Mode
    packetBuffer[1] = 0;           // Stratum
    packetBuffer[2] = 6;           // Polling interval
    packetBuffer[3] = 0xEC;        // Precision
    // Reference timestamps (8 bytes)
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    // Send request
    udp.beginPacket(ntpIP, 123);
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();

    // Wait for response
    delay(1000);

    if (udp.parsePacket()) {
        udp.read(packetBuffer, NTP_PACKET_SIZE);

        // Extract time from response (bytes 40-43)
        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
        unsigned long secsSince1900 = highWord << 16 | lowWord;

        // Convert to Unix time (seconds since 1970)
        const unsigned long seventyYears = 2208988800UL;
        unsigned long epoch = secsSince1900 - seventyYears;

        // Set system time
        struct timeval tv;
        tv.tv_sec = epoch;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);

        Serial.print("NTP sync OK, epoch: ");
        Serial.println(epoch);
    }

    udp.stop();
}

// Get current time
void printTime() {
    time_t now;
    time(&now);
    struct tm* timeinfo = localtime(&now);

    Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n",
        timeinfo->tm_year + 1900,
        timeinfo->tm_mon + 1,
        timeinfo->tm_mday,
        timeinfo->tm_hour,
        timeinfo->tm_min,
        timeinfo->tm_sec);
}
```

---

## Web Server Implementation

### Serving Multiple Files

```cpp
void handleWebClient() {
    EthernetClient client = server.available();
    if (!client) return;

    String request = "";
    unsigned long timeout = millis() + 1000;

    while (client.connected() && millis() < timeout) {
        if (client.available()) {
            char c = client.read();
            request += c;
            if (request.endsWith("\r\n\r\n")) break;
        }
    }

    // Parse request path
    String path = "/";
    if (request.startsWith("GET ")) {
        int pathEnd = request.indexOf(" HTTP/");
        if (pathEnd > 4) {
            path = request.substring(4, pathEnd);
        }
    }

    // Route to handlers
    if (path == "/api/status") {
        sendJSON(client);
    } else if (path == "/style.css") {
        sendCSS(client);
    } else {
        sendHTML(client);
    }

    delay(1);
    client.stop();
}

void sendHeaders(EthernetClient& client, const char* contentType) {
    client.println("HTTP/1.1 200 OK");
    client.print("Content-Type: ");
    client.println(contentType);
    client.println("Connection: close");
    client.println();
}
```

---

## HTTPS Requests

### Using WiFiClientSecure (Insecure Mode)

The ESP32's `WiFiClientSecure` works even when using Ethernet for the underlying transport:

```cpp
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

void fetchHTTPS() {
    WiFiClientSecure client;
    client.setInsecure();  // Skip certificate verification

    HTTPClient http;
    http.begin(client, "https://www.defcon.org/defconrss.xml");
    http.setTimeout(10000);

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        // Process payload...
    }

    http.end();
}
```

**Note**: `setInsecure()` disables TLS certificate verification. Acceptable for this project where all parties are trusted.

---

## Display & Graphics

### M5GFX Basic Usage

```cpp
#include <M5Unified.h>

void drawUI() {
    M5.Display.fillScreen(TFT_BLACK);

    // Text
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 10);
    M5.Display.println("Hello World");

    // Shapes
    M5.Display.drawRect(0, 0, 320, 240, TFT_WHITE);
    M5.Display.fillRect(100, 100, 50, 30, TFT_GREEN);

    // Colors
    // TFT_BLACK, TFT_WHITE, TFT_RED, TFT_GREEN, TFT_BLUE
    // TFT_CYAN, TFT_MAGENTA, TFT_YELLOW, TFT_ORANGE
}
```

### Display Dimensions
- Width: 320 pixels
- Height: 240 pixels
- Rotation: Landscape by default

---

## Resources & Links

### Official Documentation
- [CoreS3 SE Product Page](https://shop.m5stack.com/products/m5stack-cores3-se-iot-controller-w-o-battery-bottom)
- [CoreS3 SE Docs](https://docs.m5stack.com/en/products/sku/K128-SE)
- [LAN Module 13.2 Docs](https://docs.m5stack.com/en/module/LAN%20Module%2013.2)
- [Arduino Programming Guide](https://docs.m5stack.com/en/arduino/m5cores3_se/program)

### GitHub Repositories
- [M5Unified Library](https://github.com/m5stack/M5Unified) - Main M5Stack library
- [M5GFX Library](https://github.com/m5stack/M5GFX) - Graphics library
- [M5Module-LAN-13.2](https://github.com/m5stack/M5Module-LAN-13.2) - LAN module examples
- [M5-Ethernet](https://github.com/m5stack/M5-Ethernet) - Ethernet library
- [CoreS3-UserDemo](https://github.com/m5stack/CoreS3-UserDemo) - Demo firmware
- [scarolan/cores3se-arduino](https://github.com/scarolan/cores3se-arduino) - Community examples

### Community Resources
- [M5Stack Community Forum](https://community.m5stack.com/)
- [CoreS3 + W5500 Issue Thread](https://community.m5stack.com/topic/6700/cores3-and-w5500-fail)
- [WebServer Errors Thread](https://community.m5stack.com/topic/6200/webserver-errors-core-basic-lan-13-2-w5500)

### Third-Party Libraries
- [ArduinoHttpClient](https://github.com/arduino-libraries/ArduinoHttpClient) - HTTP client
- [ESP32_W5500_NTP_CLIENT](https://github.com/PuceBaboon/ESP32_W5500_NTP_CLIENT) - NTP example

---

## DEF CON Event Data

### DEF CON 34 (2026)
- **Dates**: August 6-9, 2026
- **Location**: Las Vegas Convention Center
- **RSS Feed**: https://www.defcon.org/defconrss.xml

### DEF CON 35 (2027)
- **Dates**: August 5-8, 2027
- **Location**: Las Vegas Convention Center

### DEF CON Singapore (2026)
- **Dates**: April 28-30, 2026
- **Location**: Marina Bay Sands

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2026-02-17 | 1.0 | Initial documentation |
