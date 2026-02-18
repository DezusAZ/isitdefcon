/*
 * M5Unified Basic Example for CoreS3 SE
 *
 * Demonstrates basic M5Unified initialization and display usage.
 * Shows how to configure for CoreS3 SE (no camera, no IMU, no battery).
 */

#include <M5Unified.h>

void setup() {
    // Configure M5Unified for CoreS3 SE
    auto cfg = M5.config();

    // CoreS3 SE specific settings
    // Disable peripherals that don't exist on SE variant
    cfg.internal_mic = true;   // Mic exists on SE
    cfg.internal_spk = true;   // Speaker exists on SE
    cfg.internal_rtc = true;   // RTC exists on SE
    cfg.led_brightness = 0;    // No LED on SE (or set to 0)

    // General settings
    cfg.serial_baudrate = 115200;
    cfg.clear_display = true;

    // For Ethernet usage, you may need to disable mic:
    // cfg.internal_mic = false;  // If using GPIO14 for W5500 INT

    M5.begin(cfg);

    // Display setup
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.fillScreen(TFT_BLACK);

    M5.Display.setCursor(10, 10);
    M5.Display.println("CoreS3 SE Ready");

    M5.Display.setCursor(10, 40);
    M5.Display.setTextSize(1);
    M5.Display.println("Display: 320x240 IPS");
    M5.Display.println("Touch: Capacitive");
    M5.Display.println("Processor: ESP32-S3 @ 240MHz");
    M5.Display.println("Flash: 16MB / PSRAM: 8MB");

    // Show device info
    M5.Display.println();
    M5.Display.print("Board: ");

    // M5Unified provides board type detection
    switch (M5.getBoard()) {
        case m5::board_t::board_M5StackCoreS3:
            M5.Display.println("CoreS3");
            break;
        case m5::board_t::board_M5StackCoreS3SE:
            M5.Display.println("CoreS3 SE");
            break;
        default:
            M5.Display.println("Unknown");
            break;
    }
}

void loop() {
    // IMPORTANT: Call M5.update() every loop iteration
    // This handles touch input, buttons, and power management
    M5.update();

    // Touch handling
    if (M5.Touch.getCount()) {
        auto touch = M5.Touch.getDetail(0);

        if (touch.wasPressed()) {
            M5.Display.fillRect(0, 180, 320, 60, TFT_BLACK);
            M5.Display.setCursor(10, 180);
            M5.Display.setTextSize(2);
            M5.Display.print("Touch: ");
            M5.Display.print(touch.x);
            M5.Display.print(", ");
            M5.Display.println(touch.y);
        }
    }

    // Power button handling (CoreS3 has touch-based power area)
    if (M5.BtnPWR.wasClicked()) {
        M5.Display.fillScreen(TFT_BLUE);
        M5.Display.setCursor(100, 100);
        M5.Display.setTextSize(2);
        M5.Display.println("PWR clicked!");
        delay(500);
        M5.Display.fillScreen(TFT_BLACK);
    }

    delay(10);
}
