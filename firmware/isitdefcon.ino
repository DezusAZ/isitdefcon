/*
 * IsItDefcon - Standalone DEF CON Countdown for M5Stack CoreS3 SE
 *
 * Features:
 * - Real-time countdown to DEF CON on the LCD
 * - Fetches live news from defcon.org RSS feed
 * - Hosts a web server for network visitors
 * - Works with any W5500-based Ethernet adapter
 *
 * Hardware: M5Stack CoreS3 SE + Ethernet Unit (W5500)
 *
 * Libraries required (install via Arduino Library Manager):
 * - M5CoreS3 (or M5Unified)
 * - Ethernet (built-in)
 * - NTPClient
 * - ArduinoJson
 */

#include <M5CoreS3.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <WiFiClientSecure.h>  // For HTTPS (uses ESP32's WiFiClientSecure even over Ethernet)
#include <HTTPClient.h>
#include <time.h>

// ============================================================================
// CONFIGURATION - Edit these values for your event
// ============================================================================

// Event configuration
const char* EVENT_NAME = "DEF CON 34";
const char* EVENT_QUESTION = "Is it DEF CON?";
const char* YES_MESSAGE = "YES! Go hack something.";
const char* NO_MESSAGE = "NO.";

// DEF CON 34 dates (UTC) - August 6-9, 2026
const int EVENT_START_YEAR = 2026;
const int EVENT_START_MONTH = 8;
const int EVENT_START_DAY = 6;
const int EVENT_END_YEAR = 2026;
const int EVENT_END_MONTH = 8;
const int EVENT_END_DAY = 9;

// DEF CON RSS feed URL
const char* RSS_URL = "https://www.defcon.org/defconrss.xml";

// NTP server
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 0;  // UTC
const int DAYLIGHT_OFFSET_SEC = 0;

// Ethernet W5500 pins for CoreS3 SE (Port A: GPIO1=SCK, GPIO2=MISO, GPIO8=MOSI, GPIO9=CS)
// Adjust these if using a different port
#define ETH_CS_PIN    9
#define ETH_SCK_PIN   1
#define ETH_MISO_PIN  2
#define ETH_MOSI_PIN  8

// MAC address (change last byte if you have multiple devices)
byte mac[] = { 0xDE, 0xFC, 0x0D, 0xEF, 0xC0, 0x01 };

// ============================================================================
// GLOBALS
// ============================================================================

EthernetServer server(80);
EthernetUDP udp;

// News items from RSS
#define MAX_NEWS_ITEMS 5
String newsItems[MAX_NEWS_ITEMS];
int newsCount = 0;
int currentNewsIndex = 0;
unsigned long lastNewsScroll = 0;
const unsigned long NEWS_SCROLL_INTERVAL = 5000;  // 5 seconds per item

// Timing
unsigned long lastNTPSync = 0;
unsigned long lastRSSFetch = 0;
const unsigned long NTP_SYNC_INTERVAL = 3600000;   // 1 hour
const unsigned long RSS_FETCH_INTERVAL = 900000;   // 15 minutes

// Display state
bool isEventActive = false;
int daysUntil = 0;
int hoursUntil = 0;
int minutesUntil = 0;
int secondsUntil = 0;
bool eventPassed = false;

// Colors
#define COLOR_BG       0x0000  // Black
#define COLOR_YES      0x07E0  // Green
#define COLOR_NO       0xF800  // Red
#define COLOR_MUTED    0x4208  // Dark gray
#define COLOR_ACCENT   0x04BF  // Cyan
#define COLOR_TEXT     0xDEFB  // Light gray

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  M5.begin();
  Serial.begin(115200);

  // Initialize display
  M5.Lcd.fillScreen(COLOR_BG);
  M5.Lcd.setTextColor(COLOR_TEXT, COLOR_BG);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 100);
  M5.Lcd.println("Initializing...");

  // Initialize SPI for W5500
  SPI.begin(ETH_SCK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, ETH_CS_PIN);
  Ethernet.init(ETH_CS_PIN);

  // Start Ethernet with DHCP
  M5.Lcd.setCursor(10, 120);
  M5.Lcd.println("Getting IP via DHCP...");

  if (Ethernet.begin(mac) == 0) {
    M5.Lcd.fillScreen(COLOR_BG);
    M5.Lcd.setTextColor(COLOR_NO, COLOR_BG);
    M5.Lcd.setCursor(10, 100);
    M5.Lcd.println("Ethernet FAILED");
    M5.Lcd.setCursor(10, 130);
    M5.Lcd.setTextColor(COLOR_MUTED, COLOR_BG);
    M5.Lcd.println("Check cable & adapter");
    while (1) delay(1000);
  }

  // Show IP address
  M5.Lcd.fillScreen(COLOR_BG);
  M5.Lcd.setTextColor(COLOR_ACCENT, COLOR_BG);
  M5.Lcd.setCursor(10, 100);
  M5.Lcd.print("IP: ");
  M5.Lcd.println(Ethernet.localIP());
  delay(2000);

  // Sync time via NTP
  syncNTP();

  // Fetch initial RSS
  fetchRSS();

  // Start web server
  server.begin();

  // Draw initial display
  drawDisplay();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  M5.update();
  unsigned long now = millis();

  // Maintain Ethernet
  Ethernet.maintain();

  // Periodic NTP sync
  if (now - lastNTPSync > NTP_SYNC_INTERVAL) {
    syncNTP();
    lastNTPSync = now;
  }

  // Periodic RSS fetch
  if (now - lastRSSFetch > RSS_FETCH_INTERVAL) {
    fetchRSS();
    lastRSSFetch = now;
  }

  // Scroll news ticker
  if (now - lastNewsScroll > NEWS_SCROLL_INTERVAL && newsCount > 0) {
    currentNewsIndex = (currentNewsIndex + 1) % newsCount;
    drawNewsTicker();
    lastNewsScroll = now;
  }

  // Update countdown every second
  static unsigned long lastCountdownUpdate = 0;
  if (now - lastCountdownUpdate >= 1000) {
    updateCountdown();
    drawCountdown();
    lastCountdownUpdate = now;
  }

  // Handle web clients
  handleWebClient();
}

// ============================================================================
// NTP TIME SYNC
// ============================================================================

void syncNTP() {
  Serial.println("Syncing NTP...");

  // Use ESP32's configTime (works even with Ethernet as time source)
  // We'll do a manual NTP request over Ethernet

  udp.begin(8888);

  IPAddress ntpIP;
  if (!Ethernet.hostByName(NTP_SERVER, ntpIP)) {
    Serial.println("DNS lookup failed for NTP");
    return;
  }

  // NTP packet
  byte packetBuffer[48];
  memset(packetBuffer, 0, 48);
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  udp.beginPacket(ntpIP, 123);
  udp.write(packetBuffer, 48);
  udp.endPacket();

  delay(1000);

  if (udp.parsePacket()) {
    udp.read(packetBuffer, 48);

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    // Unix time starts Jan 1 1970
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears;

    // Set system time
    struct timeval tv;
    tv.tv_sec = epoch;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    Serial.print("NTP sync OK, epoch: ");
    Serial.println(epoch);
  } else {
    Serial.println("No NTP response");
  }

  udp.stop();
}

// ============================================================================
// COUNTDOWN LOGIC
// ============================================================================

void updateCountdown() {
  time_t now;
  time(&now);
  struct tm* timeinfo = gmtime(&now);

  int year = timeinfo->tm_year + 1900;
  int month = timeinfo->tm_mon + 1;
  int day = timeinfo->tm_mday;
  int hour = timeinfo->tm_hour;
  int minute = timeinfo->tm_min;
  int second = timeinfo->tm_sec;

  // Build date integers for comparison (YYYYMMDD)
  int today = year * 10000 + month * 100 + day;
  int startDate = EVENT_START_YEAR * 10000 + EVENT_START_MONTH * 100 + EVENT_START_DAY;
  int endDate = EVENT_END_YEAR * 10000 + EVENT_END_MONTH * 100 + EVENT_END_DAY;

  if (today >= startDate && today <= endDate) {
    isEventActive = true;
    eventPassed = false;
    daysUntil = 0;
  } else if (today > endDate) {
    isEventActive = false;
    eventPassed = true;
    daysUntil = 0;
  } else {
    isEventActive = false;
    eventPassed = false;

    // Calculate time until event start
    struct tm eventStart = {0};
    eventStart.tm_year = EVENT_START_YEAR - 1900;
    eventStart.tm_mon = EVENT_START_MONTH - 1;
    eventStart.tm_mday = EVENT_START_DAY;
    eventStart.tm_hour = 0;
    eventStart.tm_min = 0;
    eventStart.tm_sec = 0;

    time_t eventTime = mktime(&eventStart);
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

// ============================================================================
// RSS FETCHING
// ============================================================================

void fetchRSS() {
  Serial.println("Fetching RSS...");

  WiFiClientSecure client;
  client.setInsecure();  // Skip certificate verification

  HTTPClient http;
  http.begin(client, RSS_URL);
  http.setTimeout(10000);

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    parseRSS(payload);
    Serial.print("RSS fetched, items: ");
    Serial.println(newsCount);
  } else {
    Serial.print("RSS fetch failed: ");
    Serial.println(httpCode);
  }

  http.end();
}

void parseRSS(String& xml) {
  // Simple XML parsing for RSS <title> tags within <item>
  newsCount = 0;

  int itemStart = 0;
  while (newsCount < MAX_NEWS_ITEMS) {
    itemStart = xml.indexOf("<item>", itemStart);
    if (itemStart == -1) break;

    int titleStart = xml.indexOf("<title>", itemStart);
    if (titleStart == -1) break;
    titleStart += 7;

    int titleEnd = xml.indexOf("</title>", titleStart);
    if (titleEnd == -1) break;

    String title = xml.substring(titleStart, titleEnd);

    // Clean up CDATA if present
    title.replace("<![CDATA[", "");
    title.replace("]]>", "");
    title.trim();

    if (title.length() > 0) {
      newsItems[newsCount] = title;
      newsCount++;
    }

    itemStart = titleEnd;
  }

  currentNewsIndex = 0;
}

// ============================================================================
// DISPLAY RENDERING
// ============================================================================

void drawDisplay() {
  M5.Lcd.fillScreen(COLOR_BG);
  drawQuestion();
  drawCountdown();
  drawNewsTicker();
  drawFooter();
}

void drawQuestion() {
  M5.Lcd.setTextColor(COLOR_MUTED, COLOR_BG);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.println(EVENT_QUESTION);
}

void drawCountdown() {
  // Clear countdown area
  M5.Lcd.fillRect(0, 30, 320, 120, COLOR_BG);

  if (isEventActive) {
    // YES - Green, big
    M5.Lcd.setTextColor(COLOR_YES, COLOR_BG);
    M5.Lcd.setTextSize(5);
    M5.Lcd.setCursor(100, 50);
    M5.Lcd.println("YES.");

    M5.Lcd.setTextColor(COLOR_MUTED, COLOR_BG);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(50, 110);
    M5.Lcd.println(YES_MESSAGE);

  } else if (eventPassed) {
    // Already happened
    M5.Lcd.setTextColor(COLOR_MUTED, COLOR_BG);
    M5.Lcd.setTextSize(4);
    M5.Lcd.setCursor(100, 50);
    M5.Lcd.println("NO.");

    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(60, 110);
    M5.Lcd.println("It already happened.");

  } else {
    // Countdown
    M5.Lcd.setTextColor(COLOR_NO, COLOR_BG);
    M5.Lcd.setTextSize(4);
    M5.Lcd.setCursor(100, 40);
    M5.Lcd.println("NO.");

    // Days/time until
    M5.Lcd.setTextColor(COLOR_ACCENT, COLOR_BG);
    M5.Lcd.setTextSize(2);

    char countdown[32];
    if (daysUntil > 0) {
      snprintf(countdown, sizeof(countdown), "%d days", daysUntil);
      M5.Lcd.setCursor(90, 85);
    } else {
      snprintf(countdown, sizeof(countdown), "%02d:%02d:%02d", hoursUntil, minutesUntil, secondsUntil);
      M5.Lcd.setCursor(80, 85);
    }
    M5.Lcd.println(countdown);

    // Event dates
    M5.Lcd.setTextColor(COLOR_MUTED, COLOR_BG);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(60, 115);
    char dates[48];
    snprintf(dates, sizeof(dates), "Aug %d-%d, %d - Las Vegas",
             EVENT_START_DAY, EVENT_END_DAY, EVENT_START_YEAR);
    M5.Lcd.println(dates);
  }
}

void drawNewsTicker() {
  // Clear ticker area
  M5.Lcd.fillRect(0, 160, 320, 50, COLOR_BG);

  if (newsCount == 0) {
    M5.Lcd.setTextColor(COLOR_MUTED, COLOR_BG);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(10, 170);
    M5.Lcd.println("Loading news...");
    return;
  }

  // Draw current news item
  M5.Lcd.setTextColor(COLOR_TEXT, COLOR_BG);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(5, 165);

  String item = newsItems[currentNewsIndex];
  // Truncate if too long for display
  if (item.length() > 50) {
    item = item.substring(0, 47) + "...";
  }
  M5.Lcd.println(item);

  // News indicator dots
  M5.Lcd.setCursor(140, 195);
  for (int i = 0; i < newsCount; i++) {
    if (i == currentNewsIndex) {
      M5.Lcd.print("*");
    } else {
      M5.Lcd.print(".");
    }
  }
}

void drawFooter() {
  // IP address at bottom
  M5.Lcd.setTextColor(COLOR_MUTED, COLOR_BG);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(5, 230);
  M5.Lcd.print("http://");
  M5.Lcd.print(Ethernet.localIP());
}

// ============================================================================
// WEB SERVER
// ============================================================================

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

  if (path == "/api/check") {
    sendAPICheck(client);
  } else if (path == "/api/events") {
    sendAPIEvents(client);
  } else if (path == "/api/news") {
    sendAPINews(client);
  } else if (path == "/style.css") {
    sendCSS(client);
  } else if (path == "/app.js") {
    sendJS(client);
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
  client.println("Access-Control-Allow-Origin: *");
  client.println("Connection: close");
  client.println();
}

void sendAPICheck(EthernetClient& client) {
  sendHeaders(client, "application/json");

  client.print("{");
  client.print("\"event\":\""); client.print(EVENT_NAME); client.print("\",");
  client.print("\"active\":"); client.print(isEventActive ? "true" : "false"); client.print(",");
  client.print("\"days_until\":"); client.print(daysUntil); client.print(",");
  client.print("\"hours_until\":"); client.print(hoursUntil); client.print(",");
  client.print("\"minutes_until\":"); client.print(minutesUntil); client.print(",");
  client.print("\"seconds_until\":"); client.print(secondsUntil); client.print(",");
  client.print("\"passed\":"); client.print(eventPassed ? "true" : "false"); client.print(",");
  client.print("\"start\":\"");
  client.print(EVENT_START_YEAR); client.print("-");
  if (EVENT_START_MONTH < 10) client.print("0");
  client.print(EVENT_START_MONTH); client.print("-");
  if (EVENT_START_DAY < 10) client.print("0");
  client.print(EVENT_START_DAY); client.print("\",");
  client.print("\"end\":\"");
  client.print(EVENT_END_YEAR); client.print("-");
  if (EVENT_END_MONTH < 10) client.print("0");
  client.print(EVENT_END_MONTH); client.print("-");
  if (EVENT_END_DAY < 10) client.print("0");
  client.print(EVENT_END_DAY); client.print("\",");
  client.print("\"question\":\""); client.print(EVENT_QUESTION); client.print("\",");
  client.print("\"yes_message\":\""); client.print(YES_MESSAGE); client.print("\",");
  client.print("\"no_message\":\""); client.print(NO_MESSAGE); client.print("\"");
  client.print("}");
}

void sendAPIEvents(EthernetClient& client) {
  sendHeaders(client, "application/json");

  client.print("{\"events\":[{");
  client.print("\"id\":\"defcon34\",");
  client.print("\"name\":\""); client.print(EVENT_NAME); client.print("\"");
  client.print("}],\"default_event\":\"defcon34\"}");
}

void sendAPINews(EthernetClient& client) {
  sendHeaders(client, "application/json");

  client.print("{\"items\":[");
  for (int i = 0; i < newsCount; i++) {
    if (i > 0) client.print(",");
    client.print("\"");
    // Escape quotes in title
    String title = newsItems[i];
    title.replace("\"", "\\\"");
    client.print(title);
    client.print("\"");
  }
  client.print("]}");
}

void sendHTML(EthernetClient& client) {
  sendHeaders(client, "text/html; charset=utf-8");

  client.println(F("<!DOCTYPE html><html><head>"));
  client.println(F("<meta charset=\"UTF-8\">"));
  client.println(F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"));
  client.println(F("<title>Is It DEF CON?</title>"));
  client.println(F("<link rel=\"stylesheet\" href=\"/style.css\">"));
  client.println(F("</head><body>"));
  client.println(F("<div class=\"container\" id=\"app\">"));
  client.println(F("<div class=\"main-card\" id=\"main-card\">"));
  client.println(F("<div class=\"question\" id=\"question\">Is it DEF CON?</div>"));
  client.println(F("<div class=\"answer\"><span class=\"answer-text\" id=\"answer-text\">...</span></div>"));
  client.println(F("<div class=\"countdown\" id=\"countdown\"></div>"));
  client.println(F("<div class=\"detail\" id=\"detail\"></div>"));
  client.println(F("</div>"));
  client.println(F("<div class=\"news\" id=\"news\"></div>"));
  client.println(F("<div class=\"footer\">"));
  client.println(F("<p>Powered by M5Stack CoreS3 SE</p>"));
  client.println(F("</div></div>"));
  client.println(F("<script src=\"/app.js\"></script>"));
  client.println(F("</body></html>"));
}

void sendCSS(EthernetClient& client) {
  sendHeaders(client, "text/css");

  client.println(F("*{box-sizing:border-box;margin:0;padding:0}"));
  client.println(F(":root{--bg:#0a0a0a;--card:#111;--yes:#00ff41;--no:#ff3131;--text:#e0e0e0;--muted:#666;--accent:#4af}"));
  client.println(F("html,body{height:100%;background:var(--bg);color:var(--text);font-family:'Courier New',monospace}"));
  client.println(F("body{display:flex;align-items:center;justify-content:center;min-height:100vh;padding:1rem}"));
  client.println(F(".container{width:100%;max-width:500px;display:flex;flex-direction:column;gap:1rem;align-items:center}"));
  client.println(F(".main-card{width:100%;background:var(--card);border:1px solid #1f1f1f;border-radius:4px;padding:2rem;text-align:center}"));
  client.println(F(".main-card.yes{border-color:var(--yes);box-shadow:0 0 30px rgba(0,255,65,.1)}"));
  client.println(F(".question{font-size:.8rem;color:var(--muted);letter-spacing:.1em;text-transform:uppercase;margin-bottom:1rem}"));
  client.println(F(".answer-text{font-size:clamp(2rem,10vw,4rem);font-weight:900;display:block}"));
  client.println(F(".answer-text.yes{color:var(--yes);text-shadow:0 0 20px rgba(0,255,65,.5)}"));
  client.println(F(".answer-text.no{color:var(--no)}"));
  client.println(F(".countdown{font-size:1.2rem;color:var(--accent);margin-top:.5rem}"));
  client.println(F(".detail{font-size:.75rem;color:var(--muted);margin-top:.5rem}"));
  client.println(F(".news{width:100%;background:var(--card);border:1px solid #1f1f1f;border-radius:4px;padding:1rem}"));
  client.println(F(".news h3{font-size:.7rem;color:var(--muted);margin-bottom:.5rem;text-transform:uppercase}"));
  client.println(F(".news ul{list-style:none;font-size:.8rem;line-height:1.6}"));
  client.println(F(".news li{border-bottom:1px solid #1a1a1a;padding:.4rem 0}"));
  client.println(F(".footer{font-size:.65rem;color:#333;text-align:center}"));
}

void sendJS(EthernetClient& client) {
  sendHeaders(client, "application/javascript");

  client.println(F("(function(){"));
  client.println(F("const card=document.getElementById('main-card');"));
  client.println(F("const q=document.getElementById('question');"));
  client.println(F("const ans=document.getElementById('answer-text');"));
  client.println(F("const cd=document.getElementById('countdown');"));
  client.println(F("const det=document.getElementById('detail');"));
  client.println(F("const news=document.getElementById('news');"));

  client.println(F("function load(){"));
  client.println(F("fetch('/api/check').then(r=>r.json()).then(d=>{"));
  client.println(F("q.textContent=d.question;"));
  client.println(F("card.classList.remove('yes','no');"));
  client.println(F("ans.classList.remove('yes','no');"));
  client.println(F("if(d.active){card.classList.add('yes');ans.classList.add('yes');ans.textContent='YES.';cd.textContent='';det.textContent=d.yes_message;}"));
  client.println(F("else if(d.passed){ans.classList.add('no');ans.textContent='NO.';cd.textContent='';det.textContent='It already happened.';}"));
  client.println(F("else{ans.classList.add('no');ans.textContent='NO.';"));
  client.println(F("if(d.days_until>0)cd.textContent=d.days_until+' days, '+d.hours_until+'h '+d.minutes_until+'m '+d.seconds_until+'s';"));
  client.println(F("else cd.textContent=d.hours_until+':'+String(d.minutes_until).padStart(2,'0')+':'+String(d.seconds_until).padStart(2,'0');"));
  client.println(F("det.textContent='Aug '+d.start.split('-')[2]+'-'+d.end.split('-')[2]+', '+d.start.split('-')[0]+' - Las Vegas';}"));
  client.println(F("}).catch(()=>{ans.textContent='ERR';});}"));

  client.println(F("function loadNews(){"));
  client.println(F("fetch('/api/news').then(r=>r.json()).then(d=>{"));
  client.println(F("if(d.items&&d.items.length){"));
  client.println(F("let h='<h3>Latest News</h3><ul>';"));
  client.println(F("d.items.forEach(i=>{h+='<li>'+i+'</li>';});"));
  client.println(F("h+='</ul>';news.innerHTML=h;}"));
  client.println(F("}).catch(()=>{});}"));

  client.println(F("load();loadNews();"));
  client.println(F("setInterval(load,1000);"));
  client.println(F("setInterval(loadNews,60000);"));
  client.println(F("})();"));
}
