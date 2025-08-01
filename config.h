#ifndef CONFIG_H
#define CONFIG_H

#include <WebSocketsClient.h>
#include <U8g2lib.h>

// Wi-Fi credentials
extern const char* ssid;
extern const char* password;

// WebSocket server
extern const char* ws_server;
extern const int ws_port;
extern const char* ws_path;

// OLED Display
extern U8G2_SSD1306_64X32_1F_F_HW_I2C u8g2;

// Text display configuration
constexpr int MAX_CHARS_PER_LINE = 10;
constexpr int MAX_LINES = 2;
constexpr int MAX_PAGES = 15;
extern String displayPages[MAX_PAGES][MAX_LINES];
extern int currentPage;
extern int totalPages;
extern String currentSentence;

// Timing variables
extern unsigned long lastUpdateTime;
extern const unsigned long PAGE_DURATION;
extern const unsigned long DISPLAY_PAUSE;
extern bool displayActive;

// Audio configuration
#define SAMPLE_RATE 16000U
#define SAMPLE_BITS 16
#define VOLUME_GAIN 1

// WebSocket client
extern WebSocketsClient webSocket;

// Function declarations
void startApp(const char* appName);
void stopApp();
void loopApp();
void drawDisplay();
void preparePages(String sentence);
void clearAllPages();
void updateDisplay();
void showStartupMessage(String msg);
void clearDisplay();

#endif