/*
 * ESP32 HTTPS Fetch with Insecure Mode
 *
 * Demonstrates fetching HTTPS content (like DEF CON RSS) using
 * WiFiClientSecure with certificate verification disabled.
 *
 * NOTE: setInsecure() skips TLS certificate validation.
 * Only use this when security is not a concern.
 */

#include <M5Unified.h>
#include <M5_Ethernet.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPI.h>

// Pin definitions for CoreS3
#define CS_PIN   1
#define RST_PIN  0
#define INT_PIN  10

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// URL to fetch
const char* rssUrl = "https://www.defcon.org/defconrss.xml";

void setup() {
    M5.begin();
    Serial.begin(115200);

    M5.Display.setTextSize(2);
    M5.Display.println("HTTPS Fetch Demo");

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

    delay(1000);

    // Fetch RSS
    M5.Display.println("\nFetching RSS...");
    fetchRSS();
}

void loop() {
    M5.update();

    // Re-fetch every 15 minutes
    static unsigned long lastFetch = 0;
    if (millis() - lastFetch >= 900000) {
        fetchRSS();
        lastFetch = millis();
    }
}

void fetchRSS() {
    Serial.println("Fetching DEF CON RSS...");

    // Create secure client with certificate validation disabled
    WiFiClientSecure client;
    client.setInsecure();  // Skip certificate verification

    HTTPClient http;
    http.begin(client, rssUrl);
    http.setTimeout(15000);  // 15 second timeout

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();

        Serial.println("RSS fetched successfully!");
        Serial.print("Content length: ");
        Serial.println(payload.length());

        // Parse and display news items
        parseAndDisplayRSS(payload);

    } else if (httpCode > 0) {
        Serial.print("HTTP error: ");
        Serial.println(httpCode);

        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setCursor(0, 0);
        M5.Display.print("HTTP Error: ");
        M5.Display.println(httpCode);

    } else {
        Serial.print("Connection failed: ");
        Serial.println(http.errorToString(httpCode));

        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setCursor(0, 0);
        M5.Display.println("Connection failed");
    }

    http.end();
}

void parseAndDisplayRSS(String& xml) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(0, 0);
    M5.Display.println("DEF CON News:");
    M5.Display.println("-------------");

    int itemCount = 0;
    int searchPos = 0;

    while (itemCount < 5) {
        // Find <item> tag
        int itemStart = xml.indexOf("<item>", searchPos);
        if (itemStart == -1) break;

        // Find <title> within item
        int titleStart = xml.indexOf("<title>", itemStart);
        if (titleStart == -1) break;
        titleStart += 7;  // Skip "<title>"

        int titleEnd = xml.indexOf("</title>", titleStart);
        if (titleEnd == -1) break;

        // Extract title
        String title = xml.substring(titleStart, titleEnd);

        // Clean up CDATA if present
        title.replace("<![CDATA[", "");
        title.replace("]]>", "");
        title.trim();

        // Display on screen (truncate if too long)
        if (title.length() > 40) {
            title = title.substring(0, 37) + "...";
        }

        M5.Display.println(title);
        Serial.println(title);

        itemCount++;
        searchPos = titleEnd;
    }

    if (itemCount == 0) {
        M5.Display.println("No items found");
    }
}
