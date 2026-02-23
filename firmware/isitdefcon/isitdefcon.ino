// DEF CON Countdown - M5Stack CoreS3 SE
// Ethernet (LAN Module 13.2) or WiFi

#include <M5Unified.h>
#include <M5GFX.h>
#include <SPI.h>
#include <M5Module_LAN.h>
#include <time.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>

#include "dc_logo.h"


const char* EVENT_NAME = "DEF CON 34";
const char* EVENT_QUESTION = "Is it DEF CON?";
const char* YES_MESSAGE = "Go hack something.";
const char* NO_MESSAGE = "Not yet.";


const int EVENT_START_YEAR = 2026;
const int EVENT_START_MONTH = 8;
const int EVENT_START_DAY = 6;
const int EVENT_END_YEAR = 2026;
const int EVENT_END_MONTH = 8;
const int EVENT_END_DAY = 9;

const char* NEWS_API_HOST = "api.rss2json.com";
const char* NEWS_API_PATH = "/v1/api.json?rss_url=https://www.defcon.org/defconrss.xml";


IPAddress ntpServerIP(162, 159, 200, 1);

uint8_t cs_pin = 1;
uint8_t rst_pin = 0;
uint8_t int_pin = 10;


M5Module_LAN LAN;
EthernetServer server(80);
EthernetUDP udp;

byte mac[] = { 0xDE, 0xFC, 0x0D, 0x34, 0x20, 0x26 };

#define MAX_NEWS 5
String newsItems[MAX_NEWS];
int newsCount = 0;
int currentNews = 0;

unsigned long lastNtpSync = 0;
unsigned long lastRssFetch = 0;
unsigned long lastNewsScroll = 0;
unsigned long lastDisplayUpdate = 0;
bool rssFetchedOnce = false;

const unsigned long NTP_INTERVAL = 3600000;
const unsigned long RSS_INTERVAL = 900000;
const unsigned long NEWS_INTERVAL = 5000;
const unsigned long DISPLAY_INTERVAL = 1000;

bool isEventActive = false;
bool eventPassed = false;
int daysUntil = 0;
int hoursUntil = 0;
int minutesUntil = 0;
int secondsUntil = 0;
bool ethernetConnected = false;
bool timeValid = false;


enum NetworkMode { MODE_NONE, MODE_LAN, MODE_WIFI, MODE_AP };
NetworkMode networkMode = MODE_NONE;

bool wifiConnected = false;
WiFiServer wifiWebServer(80);
bool wifiServerStarted = false;
Preferences prefs;
WebServer apServer(80);
DNSServer dnsServer;
WiFiUDP wifiUdp;

const char* AP_SSID = "DEFCON-SETUP";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

#define MAX_NETWORKS 10
String scannedSSIDs[MAX_NETWORKS];
int scannedRSSI[MAX_NETWORKS];
int networkCount = 0;

// AP management server always running flag
bool apManagementRunning = false;

M5Canvas canvas(&M5.Display);

#define COLOR_BG       0x0000
#define COLOR_YES      0x07E0
#define COLOR_NO       0xF800
#define COLOR_MUTED    0x4208
#define COLOR_ACCENT   0x04DF
#define COLOR_TEXT     0xDEFB


void initEthernet();
bool syncNTP();
bool syncNTPWiFi();
void fetchNews();
void fetchNewsWiFi();
void updateCountdown();
void drawDisplay();
void drawLogo();
void drawStatus();
void drawCountdown();
void drawNews();
void drawFooter();
void handleWebClient();
void handleWiFiWebClient();

NetworkMode showNetworkMenu();
void drawNetworkMenu();

bool loadWiFiCredentials(String &ssid, String &password);
void saveWiFiCredentials(const String &ssid, const String &password);
void clearWiFiCredentials();
bool tryWiFiConnect(const String &ssid, const String &password);
void startAPMode();
void startAPModeAlways();
void handleAPRoot();
void handleAPScan();
void handleAPConnect();
void handleAPClear();
void scanWiFiNetworks();


void setup() {
    auto cfg = M5.config();
    cfg.internal_mic = false;
    cfg.internal_spk = true;
    M5.begin(cfg);

    Serial.begin(115200);
    Serial.println("\n\n=== DEF-CON Countdown Starting ===");

    // Set pins based on board (for LAN module)
    m5::board_t board = M5.getBoard();
    switch (board) {
        case m5::board_t::board_M5Stack:
            cs_pin = 5; rst_pin = 0; int_pin = 35;
            break;
        case m5::board_t::board_M5StackCore2:
            cs_pin = 33; rst_pin = 0; int_pin = 35;
            break;
        case m5::board_t::board_M5StackCoreS3:
        case m5::board_t::board_M5StackCoreS3SE:
        default:
            cs_pin = 1; rst_pin = 0; int_pin = 10;
            break;
    }

    canvas.createSprite(320, 240);
    canvas.setTextWrap(false);

    // Show boot splash
    canvas.fillSprite(COLOR_BG);
    canvas.setTextColor(COLOR_TEXT);
    canvas.setTextSize(2);
    canvas.setCursor(40, 100);
    canvas.println("DEF-CON Countdown");
    canvas.setTextSize(1);
    canvas.setCursor(80, 130);
    canvas.println("Initializing...");
    canvas.pushSprite(0, 0);
    delay(500);

    // Initialize preferences for WiFi credentials
    prefs.begin("defcon", false);

    // Show network selection menu
    networkMode = showNetworkMenu();

    if (networkMode == MODE_LAN) {
        // LAN mode - use Ethernet
        Serial.println("LAN mode selected");
        initEthernet();

        if (ethernetConnected) {
            canvas.fillSprite(COLOR_BG);
            canvas.setTextColor(COLOR_TEXT);
            canvas.setTextSize(2);
            canvas.setCursor(40, 100);
            canvas.println("DEF-CON Countdown");
            canvas.setTextSize(1);
            canvas.setCursor(80, 130);
            canvas.println("Syncing time...");
            canvas.pushSprite(0, 0);

            if (syncNTP()) {
                timeValid = true;
                lastNtpSync = millis();
            }

            server.begin();
            Serial.print("Server at http://");
            Serial.println(Ethernet.localIP());
        }
    } else if (networkMode == MODE_WIFI) {
        // WiFi mode - try saved credentials or start AP
        Serial.println("WiFi mode selected");

        String savedSSID, savedPassword;
        if (loadWiFiCredentials(savedSSID, savedPassword)) {
            Serial.print("Found saved credentials for: ");
            Serial.println(savedSSID);

            canvas.fillSprite(COLOR_BG);
            canvas.setTextColor(COLOR_TEXT);
            canvas.setTextSize(2);
            canvas.setCursor(40, 80);
            canvas.println("Connecting WiFi");
            canvas.setTextSize(1);
            canvas.setCursor(40, 120);
            canvas.print("SSID: ");
            canvas.println(savedSSID);
            canvas.pushSprite(0, 0);

            if (tryWiFiConnect(savedSSID, savedPassword)) {
                wifiConnected = true;
                Serial.print("WiFi connected! IP: ");
                Serial.println(WiFi.localIP());

                canvas.setCursor(40, 140);
                canvas.println("Connected!");
                canvas.setCursor(40, 160);
                canvas.print("IP: ");
                canvas.println(WiFi.localIP());
                canvas.pushSprite(0, 0);
                delay(1000);

                // Sync time over WiFi
                if (syncNTPWiFi()) {
                    timeValid = true;
                    lastNtpSync = millis();
                }

                // Start WiFi web server - MUST be done after WiFi is connected
                wifiWebServer.begin();
                wifiServerStarted = true;
                Serial.print("WiFi web server started at http://");
                Serial.println(WiFi.localIP());
            } else {
                Serial.println("Failed to connect to saved network, starting AP");
                startAPMode();
            }
        } else {
            Serial.println("No saved credentials, starting AP");
            startAPMode();
        }
    }

    delay(500);
    if (networkMode != MODE_AP) {
        drawDisplay();
    }
}


void loop() {
    M5.update();
    unsigned long now = millis();

    // AP mode only (initial setup) has minimal loop
    if (networkMode == MODE_AP) {
        dnsServer.processNextRequest();
        apServer.handleClient();
        delay(10);
        return;
    }

    // LAN mode maintenance
    if (networkMode == MODE_LAN) {
        Ethernet.maintain();
    }

    // NTP sync (works for both LAN and WiFi)
    bool networkActive = (networkMode == MODE_LAN && ethernetConnected) ||
                         (networkMode == MODE_WIFI && wifiConnected);

    if (networkActive && (now - lastNtpSync > NTP_INTERVAL)) {
        if (networkMode == MODE_LAN) {
            syncNTP();
        } else {
            syncNTPWiFi();
        }
        lastNtpSync = now;
    }

    // Fetch news: first time after 5s, then every RSS_INTERVAL (15 min)
    bool newsReady = !rssFetchedOnce ? (now > 5000) : (now - lastRssFetch > RSS_INTERVAL);
    if (networkActive && newsReady) {
        if (networkMode == MODE_LAN) {
            fetchNews();
        } else {
            fetchNewsWiFi();
        }
        lastRssFetch = now;
        rssFetchedOnce = true;
    }

    if (newsCount > 0 && (now - lastNewsScroll > NEWS_INTERVAL)) {
        currentNews = (currentNews + 1) % newsCount;
        drawNews();
        canvas.pushSprite(0, 0);
        lastNewsScroll = now;
    }

    if (now - lastDisplayUpdate >= DISPLAY_INTERVAL) {
        if (timeValid) updateCountdown();
        drawCountdown();
        drawFooter();
        canvas.pushSprite(0, 0);
        lastDisplayUpdate = now;
    }

    // Web server handling
    if (networkMode == MODE_LAN && ethernetConnected) {
        handleWebClient();
    } else if (networkMode == MODE_WIFI && wifiConnected) {
        handleWiFiWebClient();
    }

    if (M5.Touch.getCount() > 0) {
        auto t = M5.Touch.getDetail(0);
        if (t.wasPressed()) {
            Serial.println("Touch - refreshing news...");
            if (networkActive) {
                if (networkMode == MODE_LAN) {
                    fetchNews();
                } else {
                    fetchNewsWiFi();
                }
                lastRssFetch = now;
                rssFetchedOnce = true;
            }
            drawDisplay();
        }
    }

    delay(10);
}

// ETHERNET

void initEthernet() {
    Serial.println("Initializing Ethernet...");

    SPI.begin(SCK, MISO, MOSI, -1);

    LAN.setResetPin(rst_pin);
    LAN.reset();
    LAN.init(cs_pin);

    Serial.println("Getting IP via DHCP...");
    canvas.setCursor(80, 150);
    canvas.println("DHCP...");
    canvas.pushSprite(0, 0);

    int attempts = 0;
    while (LAN.begin(mac) != 1 && attempts < 5) {
        Serial.println("DHCP failed, retrying...");
        attempts++;
        delay(2000);
    }

    if (attempts >= 5) {
        Serial.println("Ethernet FAILED");
        ethernetConnected = false;
        return;
    }

    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        Serial.println("No Ethernet hardware");
        ethernetConnected = false;
        return;
    }

    ethernetConnected = true;
    Serial.print("IP: ");
    Serial.println(Ethernet.localIP());
}

// NTP

bool syncNTP() {
    Serial.println("Syncing NTP...");

    if (udp.begin(8888) == 0) {
        Serial.println("UDP begin failed");
        return false;
    }

    byte packet[48];
    memset(packet, 0, 48);
    packet[0] = 0b11100011;
    packet[1] = 0;
    packet[2] = 6;
    packet[3] = 0xEC;
    packet[12] = 49;
    packet[13] = 0x4E;
    packet[14] = 49;
    packet[15] = 52;

    udp.beginPacket(ntpServerIP, 123);
    udp.write(packet, 48);
    udp.endPacket();

    unsigned long start = millis();
    while (millis() - start < 3000) {
        int size = udp.parsePacket();
        if (size >= 48) {
            udp.read(packet, 48);
            unsigned long hi = word(packet[40], packet[41]);
            unsigned long lo = word(packet[42], packet[43]);
            unsigned long secs1900 = (hi << 16) | lo;
            unsigned long epoch = secs1900 - 2208988800UL;

            struct timeval tv;
            tv.tv_sec = epoch;
            tv.tv_usec = 0;
            settimeofday(&tv, NULL);

            Serial.print("NTP OK: ");
            Serial.println(epoch);
            udp.stop();
            return true;
        }
        delay(10);
    }

    Serial.println("NTP timeout");
    udp.stop();
    return false;
}

// NEWS FETCH - Uses rss2json.com API over plain HTTP (no TLS needed)

void fetchNews() {
    Serial.println("Fetching news via HTTP...");

    // Cloudflare IP for api.rss2json.com (no DNS in M5-Ethernet library)
    // This is a Cloudflare anycast IP - should be stable
    IPAddress apiIP(104, 26, 10, 156);
    Serial.print("API IP: ");
    Serial.println(apiIP);

    // Connect via plain HTTP (port 80)
    EthernetClient client;
    if (!client.connect(apiIP, 80)) {
        Serial.println("HTTP connect failed");
        return;
    }

    // Send HTTP GET request
    client.print("GET ");
    client.print(NEWS_API_PATH);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(NEWS_API_HOST);
    client.println("Connection: close");
    client.println();

    // Wait for response
    unsigned long timeout = millis() + 10000;
    while (!client.available() && millis() < timeout) {
        delay(10);
    }
    if (!client.available()) {
        Serial.println("HTTP timeout");
        client.stop();
        return;
    }

    // Skip HTTP headers (find blank line)
    bool headersEnded = false;
    String headerLine = "";
    while (client.available() && !headersEnded) {
        char c = client.read();
        if (c == '\n') {
            if (headerLine.length() == 0 || headerLine == "\r") {
                headersEnded = true;
            }
            headerLine = "";
        } else if (c != '\r') {
            headerLine += c;
        }
    }

    // Parse JSON response - stream parse for "title" values in items array
    // JSON structure: {"items":[{"title":"..."},{"title":"..."},...]}
    newsCount = 0;
    bool inItems = false;
    bool capturingTitle = false;
    String buffer = "";
    buffer.reserve(256);
    String titleValue = "";
    titleValue.reserve(128);

    timeout = millis() + 15000;
    while (client.available() && millis() < timeout && newsCount < MAX_NEWS) {
        char c = client.read();
        buffer += c;

        // Keep buffer manageable
        if (buffer.length() > 200) {
            buffer.remove(0, 100);
        }

        // Look for "items" array start
        if (!inItems && buffer.endsWith("\"items\":[")) {
            inItems = true;
            buffer = "";
        }

        // Inside items array, look for "title":"
        if (inItems && !capturingTitle && buffer.endsWith("\"title\":\"")) {
            capturingTitle = true;
            titleValue = "";
            buffer = "";
        }

        // Capture title content until closing quote
        if (capturingTitle) {
            if (c == '"' && titleValue.length() > 0) {
                // Check for escaped quote
                if (titleValue.endsWith("\\")) {
                    titleValue.remove(titleValue.length() - 1);
                    titleValue += '"';
                } else {
                    // Title complete
                    titleValue.trim();
                    if (titleValue.length() > 0) {
                        // Decode HTML entities
                        titleValue.replace("&amp;", "&");
                        titleValue.replace("&lt;", "<");
                        titleValue.replace("&gt;", ">");
                        titleValue.replace("&quot;", "\"");
                        titleValue.replace("&#39;", "'");

                        newsItems[newsCount] = titleValue;
                        Serial.print("News[");
                        Serial.print(newsCount);
                        Serial.print("]: ");
                        Serial.println(titleValue);
                        newsCount++;
                    }
                    capturingTitle = false;
                    titleValue = "";
                }
            } else if (c != '"') {
                titleValue += c;
                // Safety limit
                if (titleValue.length() > 150) {
                    capturingTitle = false;
                    titleValue = "";
                }
            }
        }
    }

    client.stop();
    currentNews = 0;
    Serial.print("News fetch done, items: ");
    Serial.println(newsCount);
}

// COUNTDOWN

void updateCountdown() {
    time_t now;
    time(&now);

    // Target: August 6, 2026 at 5:00 AM Las Vegas time (PDT = UTC-7)
    // 5:00 AM PDT = 12:00 UTC (noon)
    // Use hardcoded UTC epoch for Aug 6, 2026 12:00:00 UTC
    // Calculated: 1785960000 (seconds since Jan 1, 1970)
    const time_t eventTime = 1785960000;  // Aug 6, 2026 12:00 UTC = 5 AM PDT

    // Event end: Aug 10, 2026 07:00 UTC (midnight PDT end of Aug 9)
    const time_t endTime = 1786302000;    // Aug 10, 2026 07:00 UTC

    if (now >= eventTime && now < endTime) {
        isEventActive = true;
        eventPassed = false;
        daysUntil = 0;
        hoursUntil = 0;
        minutesUntil = 0;
        secondsUntil = 0;
    } else if (now >= endTime) {
        isEventActive = false;
        eventPassed = true;
    } else {
        isEventActive = false;
        eventPassed = false;

        long diff = eventTime - now;

        if (diff > 0) {
            daysUntil = diff / 86400;
            diff %= 86400;
            hoursUntil = diff / 3600;
            diff %= 3600;
            minutesUntil = diff / 60;
            secondsUntil = diff % 60;
        }
    }
}

// DISPLAY

void drawDisplay() {
    canvas.fillSprite(COLOR_BG);
    drawLogo();
    drawCountdown();
    drawNews();
    drawFooter();
    canvas.pushSprite(0, 0);
}

void drawLogo() {
    // Logo: 200x120, centered at top
    int x = (320 - DC_LOGO_WIDTH) / 2;
    canvas.pushImage(x, 2, DC_LOGO_WIDTH, DC_LOGO_HEIGHT, dc_logo);
}

void drawCountdown() {
    // Countdown area: y=122 to y=210
    canvas.fillRect(0, 122, 320, 88, COLOR_BG);

    if (isEventActive) {
        // IT'S HAPPENING
        canvas.setTextColor(COLOR_YES);
        canvas.setTextSize(2);
        canvas.setCursor(55, 130);
        canvas.print("IT'S DEF CON TIME!");
        canvas.setTextColor(COLOR_TEXT);
        canvas.setTextSize(1);
        canvas.setCursor(90, 155);
        canvas.print(YES_MESSAGE);
        return;
    }

    if (eventPassed) {
        canvas.setTextColor(COLOR_MUTED);
        canvas.setTextSize(2);
        canvas.setCursor(50, 140);
        canvas.print("See you next year.");
        return;
    }

    if (!timeValid) {
        canvas.setTextColor(COLOR_MUTED);
        canvas.setTextSize(1);
        canvas.setCursor(100, 145);
        canvas.print("Syncing time...");
        return;
    }

    // Event name line
    canvas.setTextColor(COLOR_ACCENT);
    canvas.setTextSize(1);
    char evtLine[40];
    snprintf(evtLine, sizeof(evtLine), "%s  |  Aug %d-%d, %d",
             EVENT_NAME, EVENT_START_DAY, EVENT_END_DAY, EVENT_START_YEAR);
    int evtW = strlen(evtLine) * 6;
    canvas.setCursor((320 - evtW) / 2, 124);
    canvas.print(evtLine);

    // Big day count
    canvas.setTextColor(COLOR_NO);
    canvas.setTextSize(4);
    char dayStr[8];
    snprintf(dayStr, sizeof(dayStr), "%d", daysUntil);
    int dayW = strlen(dayStr) * 24;
    canvas.setCursor((320 - dayW) / 2, 135);
    canvas.print(dayStr);

    // "DAYS" label
    canvas.setTextColor(COLOR_MUTED);
    canvas.setTextSize(1);
    canvas.setCursor((320 - 24) / 2, 172);
    canvas.print("DAYS");

    // HH:MM:SS underneath
    canvas.setTextColor(COLOR_TEXT);
    canvas.setTextSize(2);
    char hms[12];
    snprintf(hms, sizeof(hms), "%02d:%02d:%02d", hoursUntil, minutesUntil, secondsUntil);
    int hmsW = strlen(hms) * 12;
    canvas.setCursor((320 - hmsW) / 2, 182);
    canvas.print(hms);
}

void drawNews() {
    canvas.fillRect(0, 213, 320, 14, COLOR_BG);
    canvas.setTextSize(1);

    if (newsCount == 0) {
        canvas.setTextColor(COLOR_MUTED);
        canvas.setCursor(5, 215);
        bool networkActive = (networkMode == MODE_LAN && ethernetConnected) ||
                             (networkMode == MODE_WIFI && wifiConnected);
        canvas.print(networkActive ? "Loading news..." : "No network");
        return;
    }

    canvas.setTextColor(COLOR_TEXT);
    canvas.setCursor(5, 215);
    String item = newsItems[currentNews];
    // Truncate if too long for display (52 chars at size 1)
    if (item.length() > 52) {
        item = item.substring(0, 49) + "...";
    }
    canvas.print(item);
}

void drawFooter() {
    canvas.fillRect(0, 229, 320, 11, COLOR_BG);
    canvas.setTextColor(COLOR_MUTED);
    canvas.setTextSize(1);
    canvas.setCursor(5, 230);

    if (networkMode == MODE_LAN && ethernetConnected) {
        canvas.print(Ethernet.localIP());
    } else if (networkMode == MODE_WIFI && wifiConnected) {
        canvas.print(WiFi.localIP());
    } else {
        canvas.print("No network");
    }
}

// WEB SERVER

void handleWebClient() {
    EthernetClient client = server.available();
    if (!client) return;

    Serial.println("Client connected");

    String request = "";
    boolean currentLineIsBlank = true;
    unsigned long timeout = millis() + 2000;

    while (client.connected() && millis() < timeout) {
        if (client.available()) {
            char c = client.read();
            request += c;

            if (c == '\n' && currentLineIsBlank) {
                // Request complete, parse path
                String path = "/";
                if (request.startsWith("GET ")) {
                    int end = request.indexOf(" HTTP/");
                    if (end > 4) path = request.substring(4, end);
                }

                Serial.print("Path: ");
                Serial.println(path);

                // Send response based on path
                if (path == "/api/check") {
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: application/json");
                    client.println("Connection: close");
                    client.println();
                    client.print("{\"event\":\"");
                    client.print(EVENT_NAME);
                    client.print("\",\"active\":");
                    client.print(isEventActive ? "true" : "false");
                    client.print(",\"days_until\":");
                    client.print(daysUntil);
                    client.print(",\"hours_until\":");
                    client.print(hoursUntil);
                    client.print(",\"minutes_until\":");
                    client.print(minutesUntil);
                    client.print(",\"seconds_until\":");
                    client.print(secondsUntil);
                    client.print(",\"passed\":");
                    client.print(eventPassed ? "true" : "false");
                    client.println("}");

                } else if (path == "/api/news") {
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: application/json");
                    client.println("Connection: close");
                    client.println();
                    client.print("{\"items\":[");
                    for (int i = 0; i < newsCount; i++) {
                        if (i > 0) client.print(",");
                        client.print("\"");
                        String t = newsItems[i];
                        t.replace("\"", "\\\"");
                        client.print(t);
                        client.print("\"");
                    }
                    client.println("]}");

                } else {
                    // Single self-contained HTML page with inline CSS/JS
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html; charset=utf-8");
                    client.println("Connection: close");
                    client.println();
                    client.println("<!DOCTYPE html><html><head>");
                    client.println("<meta charset=\"UTF-8\">");
                    client.println("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
                    client.println("<title>DEF-CON COUNTDOWN</title>");
                    // Inline CSS
                    client.println("<style>");
                    client.println("*{margin:0;padding:0;box-sizing:border-box}");
                    client.println("body{background:#000;color:#0f0;font-family:'Courier New',monospace;min-height:100vh;padding:1rem}");
                    client.println(".c{max-width:600px;margin:0 auto}");
                    client.println(".hdr{border:1px solid #0f0;padding:.5rem;margin-bottom:1rem;text-align:center}");
                    client.println(".hdr h1{color:#0f0;font-size:1.5rem;text-shadow:0 0 10px #0f0}");
                    client.println(".box{border:1px solid #030;padding:1rem;margin-bottom:1rem;background:#010}");
                    client.println(".label{color:#060;font-size:.8rem;margin-bottom:.5rem}");
                    client.println(".time{font-size:2.5rem;color:#0f0;text-shadow:0 0 8px #0f0;text-align:center;margin:.5rem 0}");
                    client.println(".sub{color:#080;font-size:1rem;text-align:center}");
                    client.println("ul{list-style:none}");
                    client.println("li{padding:.4rem 0;border-bottom:1px solid #020;color:#0a0}");
                    client.println("li:before{content:'> ';color:#060}");
                    client.println("a{color:#0f0;text-decoration:none;display:block;padding:.3rem 0}");
                    client.println("a:hover{text-shadow:0 0 5px #0f0}");
                    client.println("a:before{content:'[ ';color:#060}a:after{content:' ]';color:#060}");
                    client.println(".credit{margin-top:1rem;padding-top:1rem;border-top:1px solid #300;text-align:center}");
                    client.println(".credit span{color:#f00}");
                    client.println(".credit a{color:#f00;display:inline}");
                    client.println(".credit a:before,.credit a:after{color:#600}");
                    client.println(".f{text-align:center;color:#030;font-size:.7rem;margin-top:1rem}");
                    client.println("</style></head><body>");
                    client.println("<div class=\"c\">");
                    // Header
                    client.println("<div class=\"hdr\"><h1>DEF-CON COUNTDOWN</h1></div>");
                    // Countdown box with server-side values
                    client.println("<div class=\"box\">");
                    client.println("<div class=\"label\">$ ./countdown --target defcon34</div>");
                    client.print("<div class=\"time\">");
                    if (isEventActive) {
                        client.print("[ NOW ACTIVE ]");
                    } else if (eventPassed) {
                        client.print("[ COMPLETED ]");
                    } else {
                        client.print(daysUntil);
                        client.print("d ");
                        client.print(hoursUntil);
                        client.print("h ");
                        client.print(minutesUntil);
                        client.print("m ");
                        client.print(secondsUntil);
                        client.print("s");
                    }
                    client.println("</div>");
                    client.print("<div class=\"sub\">");
                    client.print(EVENT_NAME);
                    client.println(" // Aug 6-9, 2026 // Las Vegas</div>");
                    client.println("</div>");
                    // Info box - hardcoded DEF CON info
                    client.println("<div class=\"box\">");
                    client.println("<div class=\"label\">$ cat /etc/defcon/info.txt</div>");
                    client.println("<ul>");
                    client.println("<li>DEF CON 34 - Las Vegas Convention Center</li>");
                    client.println("<li>August 6-9, 2026</li>");
                    client.println("<li>The world's largest hacker convention</li>");
                    client.println("<li>Villages, CTF, Talks, Parties, Hardware Hacking</li>");
                    client.println("<li>Badge pickup starts Thursday</li>");
                    client.println("</ul></div>");
                    // Links box
                    client.println("<div class=\"box\">");
                    client.println("<div class=\"label\">$ cat /etc/defcon/resources.txt</div>");
                    client.println("<a href=\"https://defcon.org\" target=\"_blank\">defcon.org - Official Site</a>");
                    client.println("<a href=\"https://forum.defcon.org\" target=\"_blank\">forum.defcon.org - Community</a>");
                    client.println("<a href=\"https://media.defcon.org\" target=\"_blank\">media.defcon.org - Archives</a>");
                    client.println("<a href=\"https://defcon.org/html/links/dc-ctf.html\" target=\"_blank\">DEF CON CTF</a>");
                    client.println("<a href=\"https://infocondb.org\" target=\"_blank\">infocondb.org - Info Archive</a>");
                    client.println("<a href=\"https://twitter.com/defaborginc\" target=\"_blank\">@defaborginc - Twitter</a>");
                    // Creator credit in red
                    client.println("<div class=\"credit\">");
                    client.println("<span>CREATED BY: DZAZ</span><br>");
                    client.println("<a href=\"https://www.instagram.com/dz_az02/\" target=\"_blank\">Instagram: @dz_az02</a>");
                    client.println("</div>");
                    client.println("</div>");
                    // Footer
                    client.println("<div class=\"f\">// M5Stack CoreS3 SE // ESP32-S3 //</div>");
                    client.println("</div>");
                    // Inline JS for auto-refresh countdown
                    // Target: Aug 6, 2026 at 5:00 AM PDT (Las Vegas) = 12:00 UTC
                    client.println("<script>");
                    client.println("const t=new Date('2026-08-06T05:00:00-07:00').getTime();");
                    client.println("setInterval(()=>{const n=Date.now(),d=t-n;if(d>0){");
                    client.println("const days=Math.floor(d/86400000),hrs=Math.floor((d%86400000)/3600000),");
                    client.println("mins=Math.floor((d%3600000)/60000),secs=Math.floor((d%60000)/1000);");
                    client.println("document.querySelector('.time').textContent=days+'d '+hrs+'h '+mins+'m '+secs+'s';}},1000);");
                    client.println("</script>");
                    client.println("</body></html>");
                }
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
    Serial.println("Client disconnected");
}

// NETWORK MODE SELECTION MENU

void drawNetworkMenu() {
    canvas.fillSprite(COLOR_BG);

    // Title
    canvas.setTextColor(COLOR_ACCENT);
    canvas.setTextSize(2);
    canvas.setCursor(45, 20);
    canvas.println("SELECT NETWORK");

    // Subtitle
    canvas.setTextColor(COLOR_MUTED);
    canvas.setTextSize(1);
    canvas.setCursor(60, 50);
    canvas.println("Touch to select mode");

    // LAN Button (left)
    canvas.drawRect(20, 80, 130, 100, COLOR_YES);
    canvas.fillRect(22, 82, 126, 96, 0x0020);  // Dark green tint
    canvas.setTextColor(COLOR_YES);
    canvas.setTextSize(2);
    canvas.setCursor(55, 110);
    canvas.println("LAN");
    canvas.setTextSize(1);
    canvas.setCursor(35, 145);
    canvas.println("Ethernet cable");

    // WiFi Button (right)
    canvas.drawRect(170, 80, 130, 100, COLOR_ACCENT);
    canvas.fillRect(172, 82, 126, 96, 0x0008);  // Dark blue tint
    canvas.setTextColor(COLOR_ACCENT);
    canvas.setTextSize(2);
    canvas.setCursor(200, 110);
    canvas.println("WiFi");
    canvas.setTextSize(1);
    canvas.setCursor(190, 145);
    canvas.println("Wireless setup");

    // Footer
    canvas.setTextColor(COLOR_MUTED);
    canvas.setTextSize(1);
    canvas.setCursor(80, 220);
    canvas.println("DEF-CON Countdown v2");

    canvas.pushSprite(0, 0);
}

NetworkMode showNetworkMenu() {
    Serial.println("Showing network selection menu...");
    drawNetworkMenu();

    while (true) {
        M5.update();

        if (M5.Touch.getCount() > 0) {
            auto t = M5.Touch.getDetail(0);
            if (t.wasPressed()) {
                int x = t.x;
                int y = t.y;

                Serial.printf("Touch at (%d, %d)\n", x, y);

                // Check LAN button (left: 20-150, 80-180)
                if (x >= 20 && x <= 150 && y >= 80 && y <= 180) {
                    Serial.println("LAN selected");

                    // Visual feedback
                    canvas.fillRect(22, 82, 126, 96, COLOR_YES);
                    canvas.setTextColor(COLOR_BG);
                    canvas.setTextSize(2);
                    canvas.setCursor(55, 110);
                    canvas.println("LAN");
                    canvas.pushSprite(0, 0);
                    delay(300);

                    return MODE_LAN;
                }

                // Check WiFi button (right: 170-300, 80-180)
                if (x >= 170 && x <= 300 && y >= 80 && y <= 180) {
                    Serial.println("WiFi selected");

                    // Visual feedback
                    canvas.fillRect(172, 82, 126, 96, COLOR_ACCENT);
                    canvas.setTextColor(COLOR_BG);
                    canvas.setTextSize(2);
                    canvas.setCursor(200, 110);
                    canvas.println("WiFi");
                    canvas.pushSprite(0, 0);
                    delay(300);

                    return MODE_WIFI;
                }
            }
        }

        delay(10);
    }
}

// WIFI CREDENTIALS (NVS)

bool loadWiFiCredentials(String &ssid, String &password) {
    ssid = prefs.getString("wifi_ssid", "");
    password = prefs.getString("wifi_pass", "");

    if (ssid.length() == 0) {
        return false;
    }
    return true;
}

void saveWiFiCredentials(const String &ssid, const String &password) {
    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_pass", password);
    Serial.println("WiFi credentials saved to NVS");
}

void clearWiFiCredentials() {
    prefs.remove("wifi_ssid");
    prefs.remove("wifi_pass");
    Serial.println("WiFi credentials cleared");
}

// WIFI CONNECTION

bool tryWiFiConnect(const String &ssid, const String &password) {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startTime = millis();
    int dots = 0;

    while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < 15000) {
        delay(500);
        Serial.print(".");
        dots++;

        // Update display with progress
        canvas.fillRect(40, 180, 240, 20, COLOR_BG);
        canvas.setTextColor(COLOR_MUTED);
        canvas.setTextSize(1);
        canvas.setCursor(40, 180);
        String progress = "Connecting";
        for (int i = 0; i < (dots % 4); i++) progress += ".";
        canvas.print(progress);
        canvas.pushSprite(0, 0);
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Connected! IP: ");
        Serial.println(WiFi.localIP());
        return true;
    }

    Serial.println("Connection failed");
    WiFi.disconnect();
    return false;
}

void scanWiFiNetworks() {
    Serial.println("Scanning WiFi networks...");
    networkCount = WiFi.scanNetworks();

    if (networkCount < 0) {
        Serial.println("Scan failed");
        networkCount = 0;
        return;
    }

    if (networkCount > MAX_NETWORKS) {
        networkCount = MAX_NETWORKS;
    }

    for (int i = 0; i < networkCount; i++) {
        scannedSSIDs[i] = WiFi.SSID(i);
        scannedRSSI[i] = WiFi.RSSI(i);
        Serial.printf("  %d: %s (%d dBm)\n", i, scannedSSIDs[i].c_str(), scannedRSSI[i]);
    }

    WiFi.scanDelete();
}

// AP MODE (CAPTIVE PORTAL)

void startAPMode() {
    networkMode = MODE_AP;
    Serial.println("Starting AP mode...");

    // Start AP
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
    WiFi.softAP(AP_SSID);

    Serial.print("AP started: ");
    Serial.println(AP_SSID);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());

    // Scan for networks to show in portal
    scanWiFiNetworks();

    // Setup DNS server for captive portal (redirect all domains to us)
    dnsServer.start(53, "*", AP_IP);

    // Setup web server routes
    apServer.on("/", handleAPRoot);
    apServer.on("/scan", handleAPScan);
    apServer.on("/connect", handleAPConnect);
    apServer.on("/clear", handleAPClear);
    apServer.onNotFound(handleAPRoot);  // Redirect all to root
    apServer.begin();

    // Show setup screen on device
    canvas.fillSprite(COLOR_BG);

    canvas.setTextColor(COLOR_ACCENT);
    canvas.setTextSize(2);
    canvas.setCursor(45, 20);
    canvas.println("WIFI SETUP");

    canvas.setTextColor(COLOR_TEXT);
    canvas.setTextSize(1);
    canvas.setCursor(20, 60);
    canvas.println("1. Connect to WiFi network:");
    canvas.setTextColor(COLOR_YES);
    canvas.setCursor(40, 80);
    canvas.println(AP_SSID);

    canvas.setTextColor(COLOR_TEXT);
    canvas.setCursor(20, 110);
    canvas.println("2. Open browser to:");
    canvas.setTextColor(COLOR_YES);
    canvas.setCursor(40, 130);
    canvas.println("http://192.168.4.1");

    canvas.setTextColor(COLOR_TEXT);
    canvas.setCursor(20, 160);
    canvas.println("3. Select your WiFi network");
    canvas.setCursor(20, 180);
    canvas.println("   and enter password");

    canvas.setTextColor(COLOR_MUTED);
    canvas.setCursor(40, 220);
    canvas.println("Device will reboot after setup");

    canvas.pushSprite(0, 0);
}

void handleAPRoot() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>DEF-CON WiFi Setup</title>";
    html += "<style>";
    html += "body{background:#000;color:#0f0;font-family:monospace;padding:20px;margin:0}";
    html += "h1{color:#0f0;text-shadow:0 0 10px #0f0;text-align:center}";
    html += ".box{border:1px solid #030;padding:15px;margin:15px 0;background:#010}";
    html += ".net{padding:10px;margin:5px 0;border:1px solid #030;cursor:pointer}";
    html += ".net:hover{background:#020;border-color:#0f0}";
    html += ".rssi{color:#060;font-size:0.8em}";
    html += "input{background:#010;border:1px solid #030;color:#0f0;padding:10px;width:100%;margin:10px 0;box-sizing:border-box}";
    html += "input:focus{border-color:#0f0;outline:none}";
    html += "button{background:#020;border:1px solid #0f0;color:#0f0;padding:15px 30px;cursor:pointer;width:100%;font-size:1.1em}";
    html += "button:hover{background:#0f0;color:#000}";
    html += ".selected{background:#020;border-color:#0f0}";
    html += "#status{color:#ff0;padding:10px;text-align:center}";
    html += "</style></head><body>";
    html += "<h1>DEF-CON SETUP</h1>";
    html += "<div class='box'>";
    html += "<p>Select your WiFi network:</p>";
    html += "<div id='networks'>";

    for (int i = 0; i < networkCount; i++) {
        html += "<div class='net' onclick=\"selectNet('";
        html += scannedSSIDs[i];
        html += "')\">";
        html += scannedSSIDs[i];
        html += " <span class='rssi'>(";
        html += String(scannedRSSI[i]);
        html += " dBm)</span></div>";
    }

    if (networkCount == 0) {
        html += "<p>No networks found. <a href='/scan' style='color:#0f0'>Scan again</a></p>";
    }

    html += "</div>";
    html += "<p><a href='/scan' style='color:#060'>[Rescan]</a></p>";
    html += "</div>";

    html += "<div class='box'>";
    html += "<form action='/connect' method='POST'>";
    html += "<label>SSID:</label>";
    html += "<input type='text' name='ssid' id='ssid' placeholder='Network name' required>";
    html += "<label>Password:</label>";
    html += "<div style='position:relative'>";
    html += "<input type='password' name='pass' id='pass' placeholder='WiFi password' style='padding-right:60px'>";
    html += "<span onclick='togglePass()' style='position:absolute;right:10px;top:50%;transform:translateY(-50%);cursor:pointer;color:#060;font-size:0.8em'>[show]</span>";
    html += "</div>";
    html += "<button type='submit'>Connect</button>";
    html += "</form>";
    html += "</div>";

    // Show current status and clear option
    html += "<div class='box'>";
    html += "<p style='color:#060'>Current Status:</p>";
    if (wifiConnected) {
        html += "<p>Connected to WiFi: " + WiFi.SSID() + "</p>";
        html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
    } else {
        String savedSSID = prefs.getString("wifi_ssid", "");
        if (savedSSID.length() > 0) {
            html += "<p style='color:#f80'>Saved network: " + savedSSID + " (not connected)</p>";
        } else {
            html += "<p>No saved credentials</p>";
        }
    }
    html += "<p><a href='/clear' style='color:#f00'>[Clear Saved Credentials]</a></p>";
    html += "</div>";

    html += "<div id='status'></div>";

    html += "<script>";
    html += "function selectNet(name){document.getElementById('ssid').value=name;";
    html += "document.querySelectorAll('.net').forEach(n=>n.classList.remove('selected'));";
    html += "event.target.classList.add('selected');}";
    html += "function togglePass(){var p=document.getElementById('pass');var s=p.previousElementSibling||p.nextElementSibling;";
    html += "if(p.type==='password'){p.type='text';s.textContent='[hide]';}else{p.type='password';s.textContent='[show]';}}";
    html += "</script>";
    html += "</body></html>";

    apServer.send(200, "text/html", html);
}

void handleAPScan() {
    scanWiFiNetworks();
    apServer.sendHeader("Location", "/");
    apServer.send(302, "text/plain", "Redirecting...");
}

void handleAPConnect() {
    String ssid = apServer.arg("ssid");
    String pass = apServer.arg("pass");

    if (ssid.length() == 0) {
        apServer.send(400, "text/html", "<html><body style='background:#000;color:#f00;font-family:monospace;padding:20px'><h1>Error</h1><p>SSID is required</p><a href='/' style='color:#0f0'>Back</a></body></html>");
        return;
    }

    Serial.print("Attempting to connect to: ");
    Serial.println(ssid);

    // Send response first (we're about to reboot)
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>Connecting...</title>";
    html += "<style>body{background:#000;color:#0f0;font-family:monospace;padding:20px;text-align:center}</style>";
    html += "</head><body>";
    html += "<h1>Connecting to WiFi...</h1>";
    html += "<p>SSID: " + ssid + "</p>";
    html += "<p>Saving credentials and rebooting...</p>";
    html += "<p style='color:#ff0'>Device will restart in 3 seconds</p>";
    html += "</body></html>";

    apServer.send(200, "text/html", html);
    delay(100);  // Let response be sent

    // Save credentials
    saveWiFiCredentials(ssid, pass);

    // Show on device screen
    canvas.fillSprite(COLOR_BG);
    canvas.setTextColor(COLOR_YES);
    canvas.setTextSize(2);
    canvas.setCursor(50, 80);
    canvas.println("Credentials");
    canvas.setCursor(80, 110);
    canvas.println("Saved!");
    canvas.setTextColor(COLOR_TEXT);
    canvas.setTextSize(1);
    canvas.setCursor(60, 150);
    canvas.println("Rebooting in 3 sec...");
    canvas.pushSprite(0, 0);

    delay(3000);

    // Reboot
    ESP.restart();
}

void startAPModeAlways() {
    // Start management AP alongside existing WiFi connection
    // This allows configuration access even when connected to a network
    if (apManagementRunning) return;

    Serial.println("Starting management AP (AP_STA mode)...");

    // Must stop any existing WiFi first, then restart in AP_STA mode
    String currentSSID = WiFi.SSID();
    String currentPSK = WiFi.psk();
    bool wasConnected = (WiFi.status() == WL_CONNECTED);

    // Switch to AP_STA mode
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
    WiFi.softAP(AP_SSID);

    // Reconnect to station if we were connected
    if (wasConnected && currentSSID.length() > 0) {
        WiFi.begin(currentSSID.c_str(), currentPSK.c_str());
        // Brief wait for reconnection
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start) < 5000) {
            delay(100);
        }
        wifiConnected = (WiFi.status() == WL_CONNECTED);
    }

    Serial.print("Management AP started: ");
    Serial.println(AP_SSID);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    if (wifiConnected) {
        Serial.print("Station IP: ");
        Serial.println(WiFi.localIP());
    }

    // Setup DNS server for captive portal (redirect all domains to us)
    dnsServer.start(53, "*", AP_IP);

    // Setup web server routes
    apServer.on("/", handleAPRoot);
    apServer.on("/scan", handleAPScan);
    apServer.on("/connect", handleAPConnect);
    apServer.on("/clear", handleAPClear);
    apServer.onNotFound(handleAPRoot);
    apServer.begin();

    apManagementRunning = true;
}

void handleAPClear() {
    Serial.println("Clearing WiFi credentials...");
    clearWiFiCredentials();

    String html = "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>Credentials Cleared</title>";
    html += "<style>body{background:#000;color:#0f0;font-family:monospace;padding:20px;text-align:center}</style>";
    html += "</head><body>";
    html += "<h1>Credentials Cleared</h1>";
    html += "<p>WiFi credentials have been removed.</p>";
    html += "<p style='color:#ff0'>Device will restart in 3 seconds...</p>";
    html += "</body></html>";

    apServer.send(200, "text/html", html);
    delay(100);

    // Show on device screen
    canvas.fillSprite(COLOR_BG);
    canvas.setTextColor(COLOR_NO);
    canvas.setTextSize(2);
    canvas.setCursor(50, 80);
    canvas.println("Credentials");
    canvas.setCursor(80, 110);
    canvas.println("Cleared!");
    canvas.setTextColor(COLOR_TEXT);
    canvas.setTextSize(1);
    canvas.setCursor(60, 150);
    canvas.println("Rebooting in 3 sec...");
    canvas.pushSprite(0, 0);

    delay(3000);
    ESP.restart();
}

// WIFI NTP SYNC

bool syncNTPWiFi() {
    Serial.println("Syncing NTP over WiFi...");

    wifiUdp.begin(8888);

    byte packet[48];
    memset(packet, 0, 48);
    packet[0] = 0b11100011;
    packet[1] = 0;
    packet[2] = 6;
    packet[3] = 0xEC;
    packet[12] = 49;
    packet[13] = 0x4E;
    packet[14] = 49;
    packet[15] = 52;

    wifiUdp.beginPacket(ntpServerIP, 123);
    wifiUdp.write(packet, 48);
    wifiUdp.endPacket();

    unsigned long start = millis();
    while (millis() - start < 3000) {
        int size = wifiUdp.parsePacket();
        if (size >= 48) {
            wifiUdp.read(packet, 48);
            unsigned long hi = word(packet[40], packet[41]);
            unsigned long lo = word(packet[42], packet[43]);
            unsigned long secs1900 = (hi << 16) | lo;
            unsigned long epoch = secs1900 - 2208988800UL;

            struct timeval tv;
            tv.tv_sec = epoch;
            tv.tv_usec = 0;
            settimeofday(&tv, NULL);

            Serial.print("NTP WiFi OK: ");
            Serial.println(epoch);
            wifiUdp.stop();
            return true;
        }
        delay(10);
    }

    Serial.println("NTP WiFi timeout");
    wifiUdp.stop();
    return false;
}

// WIFI NEWS FETCH

void fetchNewsWiFi() {
    Serial.println("Fetching news via WiFi HTTP...");

    WiFiClient wifiClient;

    // Connect via plain HTTP
    if (!wifiClient.connect("api.rss2json.com", 80)) {
        Serial.println("WiFi HTTP connect failed");
        return;
    }

    // Send HTTP GET request
    wifiClient.print("GET ");
    wifiClient.print(NEWS_API_PATH);
    wifiClient.println(" HTTP/1.1");
    wifiClient.print("Host: ");
    wifiClient.println(NEWS_API_HOST);
    wifiClient.println("Connection: close");
    wifiClient.println();

    // Wait for response
    unsigned long timeout = millis() + 10000;
    while (!wifiClient.available() && millis() < timeout) {
        delay(10);
    }
    if (!wifiClient.available()) {
        Serial.println("WiFi HTTP timeout");
        wifiClient.stop();
        return;
    }

    // Skip HTTP headers
    bool headersEnded = false;
    String headerLine = "";
    while (wifiClient.available() && !headersEnded) {
        char c = wifiClient.read();
        if (c == '\n') {
            if (headerLine.length() == 0 || headerLine == "\r") {
                headersEnded = true;
            }
            headerLine = "";
        } else if (c != '\r') {
            headerLine += c;
        }
    }

    // Parse JSON - same logic as Ethernet version
    newsCount = 0;
    bool inItems = false;
    bool capturingTitle = false;
    String buffer = "";
    buffer.reserve(256);
    String titleValue = "";
    titleValue.reserve(128);

    timeout = millis() + 15000;
    while (wifiClient.available() && millis() < timeout && newsCount < MAX_NEWS) {
        char c = wifiClient.read();
        buffer += c;

        if (buffer.length() > 200) {
            buffer.remove(0, 100);
        }

        if (!inItems && buffer.endsWith("\"items\":[")) {
            inItems = true;
            buffer = "";
        }

        if (inItems && !capturingTitle && buffer.endsWith("\"title\":\"")) {
            capturingTitle = true;
            titleValue = "";
            buffer = "";
        }

        if (capturingTitle) {
            if (c == '"' && titleValue.length() > 0) {
                if (titleValue.endsWith("\\")) {
                    titleValue.remove(titleValue.length() - 1);
                    titleValue += '"';
                } else {
                    titleValue.trim();
                    if (titleValue.length() > 0) {
                        titleValue.replace("&amp;", "&");
                        titleValue.replace("&lt;", "<");
                        titleValue.replace("&gt;", ">");
                        titleValue.replace("&quot;", "\"");
                        titleValue.replace("&#39;", "'");

                        newsItems[newsCount] = titleValue;
                        Serial.print("News[");
                        Serial.print(newsCount);
                        Serial.print("]: ");
                        Serial.println(titleValue);
                        newsCount++;
                    }
                    capturingTitle = false;
                    titleValue = "";
                }
            } else if (c != '"') {
                titleValue += c;
                if (titleValue.length() > 150) {
                    capturingTitle = false;
                    titleValue = "";
                }
            }
        }
    }

    wifiClient.stop();
    currentNews = 0;
    Serial.print("WiFi news fetch done, items: ");
    Serial.println(newsCount);
}

// WIFI WEB SERVER

void handleWiFiWebClient() {
    // Start server if not already
    if (!wifiServerStarted) {
        wifiWebServer.begin();
        wifiServerStarted = true;
        Serial.print("WiFi web server started at http://");
        Serial.println(WiFi.localIP());
    }

    WiFiClient client = wifiWebServer.available();
    if (!client) return;

    Serial.println("WiFi client connected");

    String request = "";
    boolean currentLineIsBlank = true;
    unsigned long timeout = millis() + 2000;

    while (client.connected() && millis() < timeout) {
        if (client.available()) {
            char c = client.read();
            request += c;

            if (c == '\n' && currentLineIsBlank) {
                String path = "/";
                if (request.startsWith("GET ")) {
                    int end = request.indexOf(" HTTP/");
                    if (end > 4) path = request.substring(4, end);
                }

                Serial.print("WiFi Path: ");
                Serial.println(path);

                if (path == "/api/check") {
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: application/json");
                    client.println("Connection: close");
                    client.println();
                    client.print("{\"event\":\"");
                    client.print(EVENT_NAME);
                    client.print("\",\"active\":");
                    client.print(isEventActive ? "true" : "false");
                    client.print(",\"days_until\":");
                    client.print(daysUntil);
                    client.print(",\"hours_until\":");
                    client.print(hoursUntil);
                    client.print(",\"minutes_until\":");
                    client.print(minutesUntil);
                    client.print(",\"seconds_until\":");
                    client.print(secondsUntil);
                    client.print(",\"passed\":");
                    client.print(eventPassed ? "true" : "false");
                    client.println("}");

                } else if (path == "/api/news") {
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: application/json");
                    client.println("Connection: close");
                    client.println();
                    client.print("{\"items\":[");
                    for (int i = 0; i < newsCount; i++) {
                        if (i > 0) client.print(",");
                        client.print("\"");
                        String t = newsItems[i];
                        t.replace("\"", "\\\"");
                        client.print(t);
                        client.print("\"");
                    }
                    client.println("]}");

                } else if (path == "/scan") {
                    // Scan for WiFi networks and redirect back
                    scanWiFiNetworks();
                    client.println("HTTP/1.1 302 Found");
                    client.println("Location: /#settings");
                    client.println("Connection: close");
                    client.println();

                } else if (path == "/clear") {
                    // Clear credentials and reboot
                    clearWiFiCredentials();
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println("Connection: close");
                    client.println();
                    client.println("<!DOCTYPE html><html><head>");
                    client.println("<meta name='viewport' content='width=device-width,initial-scale=1'>");
                    client.println("<style>body{background:#000;color:#0f0;font-family:monospace;padding:20px;text-align:center}</style>");
                    client.println("</head><body>");
                    client.println("<h1>Credentials Cleared</h1>");
                    client.println("<p>Rebooting in 3 seconds...</p>");
                    client.println("</body></html>");
                    client.stop();
                    delay(100);
                    canvas.fillSprite(COLOR_BG);
                    canvas.setTextColor(COLOR_NO);
                    canvas.setTextSize(2);
                    canvas.setCursor(50, 100);
                    canvas.println("WiFi Cleared");
                    canvas.setTextSize(1);
                    canvas.setCursor(60, 140);
                    canvas.println("Rebooting...");
                    canvas.pushSprite(0, 0);
                    delay(3000);
                    ESP.restart();

                } else if (path.startsWith("/connect?") || request.indexOf("POST /connect") >= 0) {
                    // Parse WiFi credentials from POST or GET
                    String ssid = "";
                    String pass = "";

                    // Try to find in URL params first (GET)
                    int ssidStart = path.indexOf("ssid=");
                    if (ssidStart >= 0) {
                        ssidStart += 5;
                        int ssidEnd = path.indexOf("&", ssidStart);
                        if (ssidEnd < 0) ssidEnd = path.length();
                        ssid = path.substring(ssidStart, ssidEnd);
                        ssid.replace("+", " ");
                        ssid.replace("%20", " ");
                    }
                    int passStart = path.indexOf("pass=");
                    if (passStart >= 0) {
                        passStart += 5;
                        int passEnd = path.indexOf("&", passStart);
                        if (passEnd < 0) passEnd = path.length();
                        pass = path.substring(passStart, passEnd);
                        pass.replace("+", " ");
                        pass.replace("%20", " ");
                    }

                    // If POST, read body
                    if (request.indexOf("POST") >= 0) {
                        String body = "";
                        while (client.available()) {
                            body += (char)client.read();
                        }
                        ssidStart = body.indexOf("ssid=");
                        if (ssidStart >= 0) {
                            ssidStart += 5;
                            int ssidEnd = body.indexOf("&", ssidStart);
                            if (ssidEnd < 0) ssidEnd = body.length();
                            ssid = body.substring(ssidStart, ssidEnd);
                            ssid.replace("+", " ");
                            ssid.replace("%20", " ");
                        }
                        passStart = body.indexOf("pass=");
                        if (passStart >= 0) {
                            passStart += 5;
                            int passEnd = body.indexOf("&", passStart);
                            if (passEnd < 0) passEnd = body.length();
                            pass = body.substring(passStart, passEnd);
                            pass.replace("+", " ");
                            pass.replace("%20", " ");
                        }
                    }

                    if (ssid.length() > 0) {
                        saveWiFiCredentials(ssid, pass);
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println("Connection: close");
                        client.println();
                        client.println("<!DOCTYPE html><html><head>");
                        client.println("<meta name='viewport' content='width=device-width,initial-scale=1'>");
                        client.println("<style>body{background:#000;color:#0f0;font-family:monospace;padding:20px;text-align:center}</style>");
                        client.println("</head><body>");
                        client.println("<h1>Credentials Saved</h1>");
                        client.print("<p>SSID: ");
                        client.print(ssid);
                        client.println("</p>");
                        client.println("<p>Rebooting in 3 seconds...</p>");
                        client.println("</body></html>");
                        client.stop();
                        delay(100);
                        canvas.fillSprite(COLOR_BG);
                        canvas.setTextColor(COLOR_YES);
                        canvas.setTextSize(2);
                        canvas.setCursor(50, 100);
                        canvas.println("WiFi Saved");
                        canvas.setTextSize(1);
                        canvas.setCursor(60, 140);
                        canvas.println("Rebooting...");
                        canvas.pushSprite(0, 0);
                        delay(3000);
                        ESP.restart();
                    } else {
                        client.println("HTTP/1.1 400 Bad Request");
                        client.println("Content-Type: text/html");
                        client.println("Connection: close");
                        client.println();
                        client.println("<html><body>SSID required</body></html>");
                    }

                } else {
                    // Unified HTML page with countdown AND WiFi settings
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html; charset=utf-8");
                    client.println("Connection: close");
                    client.println();
                    client.println("<!DOCTYPE html><html><head>");
                    client.println("<meta charset=\"UTF-8\">");
                    client.println("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
                    client.println("<title>DEF-CON COUNTDOWN</title>");
                    client.println("<style>");
                    client.println("*{margin:0;padding:0;box-sizing:border-box}");
                    client.println("body{background:#000;color:#0f0;font-family:'Courier New',monospace;min-height:100vh;padding:1rem}");
                    client.println(".c{max-width:600px;margin:0 auto}");
                    client.println(".hdr{border:1px solid #0f0;padding:.5rem;margin-bottom:1rem;text-align:center}");
                    client.println(".hdr h1{color:#0f0;font-size:1.5rem;text-shadow:0 0 10px #0f0}");
                    client.println(".box{border:1px solid #030;padding:1rem;margin-bottom:1rem;background:#010}");
                    client.println(".label{color:#060;font-size:.8rem;margin-bottom:.5rem}");
                    client.println(".time{font-size:2.5rem;color:#0f0;text-shadow:0 0 8px #0f0;text-align:center;margin:.5rem 0}");
                    client.println(".sub{color:#080;font-size:1rem;text-align:center}");
                    client.println("ul{list-style:none}");
                    client.println("li{padding:.4rem 0;border-bottom:1px solid #020;color:#0a0}");
                    client.println("li:before{content:'> ';color:#060}");
                    client.println("a{color:#0f0;text-decoration:none;display:block;padding:.3rem 0}");
                    client.println("a:hover{text-shadow:0 0 5px #0f0}");
                    client.println("a:before{content:'[ ';color:#060}a:after{content:' ]';color:#060}");
                    client.println(".credit{margin-top:1rem;padding-top:1rem;border-top:1px solid #300;text-align:center}");
                    client.println(".credit span{color:#f00}");
                    client.println(".credit a{color:#f00;display:inline}");
                    client.println(".credit a:before,.credit a:after{color:#600}");
                    client.println(".f{text-align:center;color:#030;font-size:.7rem;margin-top:1rem}");
                    // WiFi settings styles
                    client.println(".net{padding:8px;margin:5px 0;border:1px solid #030;cursor:pointer}");
                    client.println(".net:hover{background:#020;border-color:#0f0}");
                    client.println(".rssi{color:#060;font-size:0.8em}");
                    client.println("input{background:#010;border:1px solid #030;color:#0f0;padding:8px;width:100%;margin:5px 0}");
                    client.println("input:focus{border-color:#0f0;outline:none}");
                    client.println("button{background:#020;border:1px solid #0f0;color:#0f0;padding:10px 20px;cursor:pointer;width:100%;margin-top:10px}");
                    client.println("button:hover{background:#0f0;color:#000}");
                    client.println(".warn{color:#f00}");
                    client.println("</style></head><body>");
                    client.println("<div class=\"c\">");
                    // Header
                    client.println("<div class=\"hdr\"><h1>DEF-CON COUNTDOWN</h1></div>");
                    // Countdown box
                    client.println("<div class=\"box\">");
                    client.println("<div class=\"label\">$ ./countdown --target defcon34</div>");
                    client.print("<div class=\"time\">");
                    if (isEventActive) {
                        client.print("[ NOW ACTIVE ]");
                    } else if (eventPassed) {
                        client.print("[ COMPLETED ]");
                    } else {
                        client.print(daysUntil);
                        client.print("d ");
                        client.print(hoursUntil);
                        client.print("h ");
                        client.print(minutesUntil);
                        client.print("m ");
                        client.print(secondsUntil);
                        client.print("s");
                    }
                    client.println("</div>");
                    client.print("<div class=\"sub\">");
                    client.print(EVENT_NAME);
                    client.println(" // Aug 6-9, 2026 // Las Vegas</div>");
                    client.println("</div>");
                    // Info box
                    client.println("<div class=\"box\">");
                    client.println("<div class=\"label\">$ cat /etc/defcon/info.txt</div>");
                    client.println("<ul>");
                    client.println("<li>DEF CON 34 - Las Vegas Convention Center</li>");
                    client.println("<li>August 6-9, 2026</li>");
                    client.println("<li>The world's largest hacker convention</li>");
                    client.println("<li>Villages, CTF, Talks, Parties, Hardware Hacking</li>");
                    client.println("<li>Badge pickup starts Thursday</li>");
                    client.println("</ul></div>");
                    // Links box
                    client.println("<div class=\"box\">");
                    client.println("<div class=\"label\">$ cat /etc/defcon/resources.txt</div>");
                    client.println("<a href=\"https://defcon.org\" target=\"_blank\">defcon.org - Official Site</a>");
                    client.println("<a href=\"https://forum.defcon.org\" target=\"_blank\">forum.defcon.org - Community</a>");
                    client.println("<a href=\"https://media.defcon.org\" target=\"_blank\">media.defcon.org - Archives</a>");
                    client.println("<a href=\"https://defcon.org/html/links/dc-ctf.html\" target=\"_blank\">DEF CON CTF</a>");
                    client.println("<a href=\"https://infocondb.org\" target=\"_blank\">infocondb.org - Info Archive</a>");
                    client.println("<div class=\"credit\">");
                    client.println("<span>CREATED BY: DZAZ</span><br>");
                    client.println("<a href=\"https://www.instagram.com/dz_az02/\" target=\"_blank\">Instagram: @dz_az02</a>");
                    client.println("</div>");
                    client.println("</div>");

                    // WiFi Settings box
                    client.println("<div class=\"box\" id=\"settings\">");
                    client.println("<div class=\"label\">$ ./wifi-config --manage</div>");
                    client.print("<p>Connected to: <strong>");
                    client.print(WiFi.SSID());
                    client.print("</strong> (");
                    client.print(WiFi.localIP());
                    client.println(")</p>");
                    client.println("<p style=\"margin-top:10px\">Change WiFi Network:</p>");
                    // Show scanned networks
                    if (networkCount > 0) {
                        for (int i = 0; i < networkCount; i++) {
                            client.print("<div class=\"net\" onclick=\"document.getElementById('ssid').value='");
                            client.print(scannedSSIDs[i]);
                            client.print("'\">");
                            client.print(scannedSSIDs[i]);
                            client.print(" <span class=\"rssi\">(");
                            client.print(scannedRSSI[i]);
                            client.println(" dBm)</span></div>");
                        }
                    } else {
                        client.println("<p style=\"color:#060\">No networks scanned yet</p>");
                    }
                    client.println("<a href=\"/scan\">Scan for Networks</a>");
                    // WiFi form
                    client.println("<form action=\"/connect\" method=\"POST\" style=\"margin-top:15px\">");
                    client.println("<input type=\"text\" name=\"ssid\" id=\"ssid\" placeholder=\"WiFi SSID\" required>");
                    client.println("<div style=\"position:relative\">");
                    client.println("<input type=\"password\" name=\"pass\" id=\"pass\" placeholder=\"Password\" style=\"padding-right:50px\">");
                    client.println("<span onclick=\"var p=document.getElementById('pass');p.type=p.type==='password'?'text':'password';this.textContent=p.type==='password'?'show':'hide'\" style=\"position:absolute;right:10px;top:50%;transform:translateY(-50%);cursor:pointer;color:#060;font-size:0.8em\">show</span>");
                    client.println("</div>");
                    client.println("<button type=\"submit\">Save &amp; Reboot</button>");
                    client.println("</form>");
                    client.println("<p style=\"margin-top:15px\"><a href=\"/clear\" class=\"warn\" onclick=\"return confirm('Clear WiFi credentials?')\">Clear Saved Credentials</a></p>");
                    client.println("</div>");

                    // Footer
                    client.println("<div class=\"f\">// M5Stack CoreS3 SE // ESP32-S3 // WiFi //</div>");
                    client.println("</div>");
                    // JS for countdown
                    client.println("<script>");
                    client.println("const t=new Date('2026-08-06T05:00:00-07:00').getTime();");
                    client.println("setInterval(()=>{const n=Date.now(),d=t-n;if(d>0){");
                    client.println("const days=Math.floor(d/86400000),hrs=Math.floor((d%86400000)/3600000),");
                    client.println("mins=Math.floor((d%3600000)/60000),secs=Math.floor((d%60000)/1000);");
                    client.println("document.querySelector('.time').textContent=days+'d '+hrs+'h '+mins+'m '+secs+'s';}},1000);");
                    client.println("</script>");
                    client.println("</body></html>");
                }
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
    Serial.println("WiFi client disconnected");
}
