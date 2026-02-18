/*
 * ESP32 + W5500 NTP Time Sync Example
 * Based on: https://github.com/PuceBaboon/ESP32_W5500_NTP_CLIENT
 *
 * Demonstrates manual UDP NTP time synchronization over Ethernet.
 * The ESP32's built-in configTime() doesn't work reliably with Ethernet,
 * so this uses direct UDP NTP packet exchange.
 */

#include <M5Unified.h>
#include <M5_Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>
#include <time.h>

// Pin definitions for CoreS3
#define CS_PIN   1
#define RST_PIN  0
#define INT_PIN  10

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// NTP configuration
const char* ntpServer = "pool.ntp.org";
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

EthernetUDP udp;

// Timezone offset (seconds) - adjust for your location
// UTC = 0, PST = -8*3600, EST = -5*3600, etc.
const long TIMEZONE_OFFSET = 0;

void setup() {
    M5.begin();
    Serial.begin(115200);

    M5.Display.setTextSize(2);
    M5.Display.println("NTP Sync Demo");

    // Initialize SPI and Ethernet
    SPI.begin();
    Ethernet.init(CS_PIN);

    M5.Display.println("Getting IP...");

    while (Ethernet.begin(mac) == 0) {
        M5.Display.println("DHCP failed...");
        delay(2000);
    }

    M5.Display.print("IP: ");
    M5.Display.println(Ethernet.localIP());

    // Sync time
    M5.Display.println("Syncing NTP...");
    if (syncNTP()) {
        M5.Display.println("NTP sync OK!");
    } else {
        M5.Display.println("NTP sync FAILED");
    }
}

void loop() {
    M5.update();

    static unsigned long lastUpdate = 0;

    // Update display every second
    if (millis() - lastUpdate >= 1000) {
        displayTime();
        lastUpdate = millis();
    }

    // Re-sync NTP every hour
    static unsigned long lastSync = 0;
    if (millis() - lastSync >= 3600000) {
        syncNTP();
        lastSync = millis();
    }
}

bool syncNTP() {
    Serial.println("Starting NTP sync...");

    udp.begin(8888);  // Local port for UDP

    // DNS lookup for NTP server
    IPAddress ntpIP;
    if (Ethernet.hostByName(ntpServer, ntpIP) != 1) {
        Serial.println("DNS lookup failed for NTP server");
        udp.stop();
        return false;
    }

    Serial.print("NTP server IP: ");
    Serial.println(ntpIP);

    // Build NTP request packet
    memset(packetBuffer, 0, NTP_PACKET_SIZE);

    // Initialize values needed to form NTP request
    packetBuffer[0] = 0b11100011;  // LI=3, Version=4, Mode=3 (client)
    packetBuffer[1] = 0;           // Stratum (unspecified)
    packetBuffer[2] = 6;           // Polling interval
    packetBuffer[3] = 0xEC;        // Peer clock precision

    // 8 bytes for Root Delay & Root Dispersion (leave as 0)

    // Reference ID (leave as 0)

    // Reference timestamp (leave as 0)

    // Originate timestamp (leave as 0)

    // Transmit timestamp (set to non-zero for some servers)
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    // Send NTP request
    udp.beginPacket(ntpIP, 123);
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();

    // Wait for response (with timeout)
    unsigned long startTime = millis();
    while (millis() - startTime < 3000) {
        if (udp.parsePacket()) {
            // Read response
            udp.read(packetBuffer, NTP_PACKET_SIZE);

            // Extract timestamp from bytes 40-43 (Transmit Timestamp)
            unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
            unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
            unsigned long secsSince1900 = highWord << 16 | lowWord;

            // Convert NTP time to Unix time
            // NTP starts from 1900, Unix from 1970
            const unsigned long seventyYears = 2208988800UL;
            unsigned long epoch = secsSince1900 - seventyYears;

            // Apply timezone offset
            epoch += TIMEZONE_OFFSET;

            // Set system time
            struct timeval tv;
            tv.tv_sec = epoch;
            tv.tv_usec = 0;
            settimeofday(&tv, NULL);

            Serial.print("NTP sync successful! Epoch: ");
            Serial.println(epoch);

            udp.stop();
            return true;
        }
        delay(10);
    }

    Serial.println("NTP response timeout");
    udp.stop();
    return false;
}

void displayTime() {
    time_t now;
    time(&now);
    struct tm* timeinfo = localtime(&now);

    // Clear time area
    M5.Display.fillRect(0, 100, 320, 50, TFT_BLACK);
    M5.Display.setCursor(10, 100);
    M5.Display.setTextSize(2);

    // Format: YYYY-MM-DD HH:MM:SS
    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d",
        timeinfo->tm_year + 1900,
        timeinfo->tm_mon + 1,
        timeinfo->tm_mday);
    M5.Display.println(timeStr);

    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
        timeinfo->tm_hour,
        timeinfo->tm_min,
        timeinfo->tm_sec);
    M5.Display.setTextSize(3);
    M5.Display.setCursor(50, 130);
    M5.Display.println(timeStr);
}
