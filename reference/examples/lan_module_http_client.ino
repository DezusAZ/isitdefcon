/*
 * M5Stack LAN Module 13.2 HTTP Client Example
 * Source: https://github.com/m5stack/M5Module-LAN-13.2/tree/main/examples/HTTP
 *
 * Demonstrates HTTP GET and POST requests using the W5500 Ethernet controller.
 * Uses httpbin.org as a test server.
 */

#include <M5Unified.h>
#include <M5GFX.h>
#include <M5Module_LAN.h>
#include <ArduinoHttpClient.h>
#include <SPI.h>

// Pin definitions vary by board
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

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// HTTP server to test with
char serverAddress[] = "httpbin.org";
int serverPort = 80;

EthernetClient ethClient;
HttpClient client = HttpClient(ethClient, serverAddress, serverPort);

M5Canvas canvas(&M5.Display);

void setup() {
    M5.begin();
    Serial.begin(115200);

    // Initialize display
    canvas.createSprite(M5.Display.width(), M5.Display.height());
    canvas.setTextSize(1);
    canvas.fillSprite(TFT_BLACK);
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
    canvas.fillSprite(TFT_BLACK);
    canvas.setCursor(0, 0);
    canvas.print("IP: ");
    canvas.println(Ethernet.localIP());
    canvas.pushSprite(0, 0);

    Serial.print("IP: ");
    Serial.println(Ethernet.localIP());
}

void loop() {
    // === GET Request ===
    canvas.fillSprite(TFT_BLACK);
    canvas.setCursor(0, 0);
    canvas.println("Making GET request...");
    canvas.pushSprite(0, 0);

    Serial.println("\n--- GET Request ---");
    client.get("/get");

    int statusCode = client.responseStatusCode();
    String response = client.responseBody();

    Serial.print("Status: ");
    Serial.println(statusCode);
    Serial.print("Response: ");
    Serial.println(response.substring(0, 200));  // First 200 chars

    canvas.println("GET Status: " + String(statusCode));
    canvas.pushSprite(0, 0);

    delay(5000);

    // === POST Request ===
    canvas.fillSprite(TFT_BLACK);
    canvas.setCursor(0, 0);
    canvas.println("Making POST request...");
    canvas.pushSprite(0, 0);

    Serial.println("\n--- POST Request ---");

    String contentType = "application/x-www-form-urlencoded";
    String postData = "name=M5Stack&device=CoreS3";

    client.post("/post", contentType, postData);

    statusCode = client.responseStatusCode();
    response = client.responseBody();

    Serial.print("Status: ");
    Serial.println(statusCode);
    Serial.print("Response: ");
    Serial.println(response.substring(0, 200));

    canvas.println("POST Status: " + String(statusCode));
    canvas.pushSprite(0, 0);

    delay(5000);
}
