/*
 * M5Stack LAN Module 13.2 WebServer Example
 * Source: https://github.com/m5stack/M5Module-LAN-13.2/tree/main/examples/WebServer
 *
 * Demonstrates a basic HTTP web server using the W5500 Ethernet controller.
 * Serves a simple HTML page with auto-refresh.
 */

#include <M5Unified.h>
#include <M5GFX.h>
#include <M5Module_LAN.h>
#include <SPI.h>

// Pin definitions vary by board
// M5Stack Basic:  CS=5,  RST=0, INT=35
// M5Stack Core2:  CS=33, RST=0, INT=35
// M5Stack CoreS3: CS=1,  RST=0, INT=10

#if defined(ARDUINO_M5STACK_CORES3)
#define CS_PIN   1
#define RST_PIN  0
#define INT_PIN  10
#elif defined(ARDUINO_M5STACK_CORE2)
#define CS_PIN   33
#define RST_PIN  0
#define INT_PIN  35
#else  // M5Stack Basic
#define CS_PIN   5
#define RST_PIN  0
#define INT_PIN  35
#endif

M5Module_LAN LAN;
EthernetServer server(80);

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

M5Canvas canvas(&M5.Display);

void setup() {
    M5.begin();
    Serial.begin(115200);

    // Initialize display
    canvas.createSprite(M5.Display.width(), M5.Display.height());
    canvas.setTextSize(2);
    canvas.fillSprite(0x0760);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(0, 0);

    // Initialize SPI
    SPI.begin();

    // Initialize LAN module
    LAN.setResetPin(RST_PIN);
    LAN.reset();
    LAN.init(CS_PIN);

    // Wait for DHCP
    canvas.println("Getting IP via DHCP...");
    canvas.pushSprite(0, 0);

    while (LAN.begin(mac) != 1) {
        Serial.println("DHCP failed, retrying...");
        delay(2000);
    }

    // Display IP address
    canvas.fillSprite(0x0760);
    canvas.setCursor(0, 0);
    canvas.print("IP: ");
    canvas.println(Ethernet.localIP());
    canvas.pushSprite(0, 0);

    // Start server
    server.begin();
    Serial.print("Server started at: ");
    Serial.println(Ethernet.localIP());
}

void loop() {
    EthernetClient client = server.available();

    if (client) {
        Serial.println("New client connected");
        boolean currentLineIsBlank = true;

        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                Serial.write(c);

                // HTTP request ends with blank line
                if (c == '\n' && currentLineIsBlank) {
                    // Send HTTP response
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println("Connection: close");
                    client.println("Refresh: 5");  // Auto-refresh every 5 seconds
                    client.println();

                    // HTML content
                    client.println("<!DOCTYPE HTML>");
                    client.println("<html>");
                    client.println("<head><title>M5Stack LAN Module</title></head>");
                    client.println("<body>");
                    client.println("<h1>Hello M5Stack LAN Module!</h1>");
                    client.println("<p>This page refreshes every 5 seconds.</p>");
                    client.print("<p>Uptime: ");
                    client.print(millis() / 1000);
                    client.println(" seconds</p>");
                    client.println("</body>");
                    client.println("</html>");
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
}
