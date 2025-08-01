#include <Wire.h>
#include <U8g2lib.h>
#include <OneButton.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <I2S.h>
#include "config.h"
#include "icons.h"
#include "chatbot.h"
#include "transcription.h"
#include "camera.h"

// Wi-Fi credentials
const char* ssid = "Minh Hien";
const char* password = "0903304918";

// WebSocket server
const char* ws_server = "192.168.1.120";
const int ws_port = 8765;
const char* ws_path = "/ws";

// OLED Display pins
#define SDA_PIN 5
#define SCL_PIN 6
#define BUTTON_PIN 2

// OLED Display
U8G2_SSD1306_64X32_1F_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCL_PIN, SDA_PIN);
OneButton button(BUTTON_PIN, true, true);

// Text display configuration
String displayPages[MAX_PAGES][MAX_LINES];
int currentPage = 0;
int totalPages = 0;
String currentSentence = "";

// Timing variables
unsigned long lastUpdateTime = 0;
const unsigned long PAGE_DURATION = 900;
const unsigned long DISPLAY_PAUSE = 2000;
bool displayActive = false;

// WebSocket client
WebSocketsClient webSocket;

// System states
enum SystemState {
  STATE_MENU,
  STATE_APP,
  STATE_TRANSCRIPTION,
  STATE_CHATBOT,
  STATE_CAMERA
};
SystemState current_state = STATE_MENU;
bool menu_cleared = false;
unsigned long menu_clear_time = 0;
const unsigned long MENU_CLEAR_DURATION = 2000;

// Animation variables
float time_anim = 0.0;
bool animation_active = false;

// Pill shape (capsule)
int pill_center_x = 32;
int pill_center_y = 15;
int pill_radius = 10;
int pill_length = 16;

// Moving circle (magnifying glass)
float circle_x = 95;
float circle_y = 16;
int circle_radius = 11;

// Metaball size
float metaball_size = 28;

// Magnification settings
float magnify_strength = 1.4;
float distortion_strength = 0.1;

// Icon management variables
int item_selected = 0;
int item_sel_previous = 2;
int item_sel_next = 1;
bool icon_transition_started = false;

// Icon names array
const char* icon_names[] = {
  "AI",
  "Transcribe",
  "Camera"
};

// App indices
const int APP_AI = 0;
const int APP_TRANSCRIBE = 1;
const int APP_CAMERA = 2;

// WiFi connection status
bool isWiFiConnecting = false;
bool isWiFiConnected = false;

// Function declarations
void setupMenuHandlers();
void setupCameraHandlers();

// Distance to pill shape
float distanceToPill(int x, int y) {
  int pill_left = pill_center_x - pill_length / 2;
  int pill_right = pill_center_x + pill_length / 2;
  float dist;
  if (x < pill_left) {
    dist = sqrt(pow(x - pill_left, 2) + pow(y - pill_center_y, 2)) - pill_radius;
  } else if (x > pill_right) {
    dist = sqrt(pow(x - pill_right, 2) + pow(y - pill_center_y, 2)) - pill_radius;
  } else {
    dist = abs(y - pill_center_y) - pill_radius;
  }
  return max(dist, 0.1f);
}

// Get pixel from bitmap with bilinear interpolation
bool getPixelFromBitmap(const unsigned char* bitmap, float x, float y) {
  if (x < 0 || x >= 63.5 || y < 0 || y >= 31.5) {
    return false;
  }
  int x1 = (int)floor(x);
  int y1 = (int)floor(y);
  int x2 = min(x1 + 1, 63);
  int y2 = min(y1 + 1, 31);
  x1 = max(0, min(x1, 63));
  y1 = max(0, min(y1, 31));
  bool p1 = getPixelRaw(bitmap, x1, y1);
  bool p2 = getPixelRaw(bitmap, x2, y1);
  bool p3 = getPixelRaw(bitmap, x1, y2);
  bool p4 = getPixelRaw(bitmap, x2, y2);
  int pixel_count = p1 + p2 + p3 + p4;
  return pixel_count >= 2;
}

// Get raw pixel from bitmap
bool getPixelRaw(const unsigned char* bitmap, int x, int y) {
  if (x < 0 || x >= 64 || y < 0 || y >= 32) {
    return false;
  }
  int byte_index = y * 8 + (x / 8);
  int bit_index = 7 - (x % 8);
  return (bitmap[byte_index] >> bit_index) & 1;
}

// Draw magnifying glass
void drawOptimizedMagnifyingGlass() {
  u8g2.drawXBMP(0, 0, 64, 32, epd_bitmap_allArray[item_selected]);
  int min_x = max(0, (int)(circle_x - circle_radius - 1));
  int max_x = min(63, (int)(circle_x + circle_radius + 1));
  int min_y = max(0, (int)(circle_y - circle_radius - 1));
  int max_y = min(31, (int)(circle_y + circle_radius + 1));
  for (int screen_y = min_y; screen_y <= max_y; screen_y++) {
    for (int screen_x = min_x; screen_x <= max_x; screen_x++) {
      float dx = screen_x - circle_x;
      float dy = screen_y - circle_y;
      float distance_to_center = sqrt(dx * dx + dy * dy);
      if (distance_to_center <= circle_radius) {
        float edge_fade = 1.0;
        if (distance_to_center > circle_radius - 1.0) {
          edge_fade = circle_radius - distance_to_center;
        }
        if (edge_fade < 0.3) continue;
        float effect_strength = 1.0 - (distance_to_center / circle_radius);
        effect_strength = effect_strength * effect_strength;
        float adaptive_magnify = magnify_strength * effect_strength * 0.7 + 1.0;
        float sample_x = circle_x + dx / adaptive_magnify;
        float sample_y = circle_y + dy / adaptive_magnify;
        if (distance_to_center < circle_radius * 0.7) {
          float angle = atan2(dy, dx);
          float wave_intensity = effect_strength * distortion_strength;
          float wave = sin(distance_to_center * 0.8 + time_anim * 0.008) * wave_intensity;
          sample_x += cos(angle + PI / 2) * wave * 1.5;
          sample_y += sin(angle + PI / 2) * wave * 1.5;
        }
        bool magnified_pixel = getPixelFromBitmap(epd_bitmap_allArray[item_selected], sample_x, sample_y);
        if (magnified_pixel) {
          u8g2.drawPixel(screen_x, screen_y);
        } else {
          u8g2.setDrawColor(0);
          u8g2.drawPixel(screen_x, screen_y);
          u8g2.setDrawColor(1);
        }
      }
    }
  }
}

// Draw magnify reveal
void drawOptimizedMagnifyReveal() {
  int circle_left_edge = (int)(circle_x - circle_radius);
  u8g2.drawXBMP(0, 0, 64, 32, epd_bitmap_allArray[item_selected]);
  if (circle_left_edge > 0 && circle_left_edge < 64) {
    u8g2.setClipWindow(0, 0, circle_left_edge - 1, 31);
    u8g2.drawXBMP(0, 0, 64, 32, epd_bitmap_allArray[item_sel_previous]);
    u8g2.setMaxClipWindow();
  }
  int min_x = max(0, (int)(circle_x - circle_radius - 1));
  int max_x = min(63, (int)(circle_x + circle_radius + 1));
  int min_y = max(0, (int)(circle_y - circle_radius - 1));
  int max_y = min(31, (int)(circle_y + circle_radius + 1));
  for (int screen_y = min_y; screen_y <= max_y; screen_y++) {
    for (int screen_x = min_x; screen_x <= max_x; screen_x++) {
      float dx = screen_x - circle_x;
      float dy = screen_y - circle_y;
      float distance_to_center = sqrt(dx * dx + dy * dy);
      if (distance_to_center <= circle_radius) {
        float edge_fade = 1.0;
        if (distance_to_center > circle_radius - 1.0) {
          edge_fade = circle_radius - distance_to_center;
        }
        if (edge_fade < 0.3) continue;
        const unsigned char* source_bitmap = (screen_x >= circle_left_edge) ?
                         epd_bitmap_allArray[item_selected] : epd_bitmap_allArray[item_sel_previous];
        float effect_strength = 1.0 - (distance_to_center / circle_radius);
        effect_strength = effect_strength * effect_strength;
        float adaptive_magnify = magnify_strength * effect_strength * 0.7 + 1.0;
        float sample_x = circle_x + dx / adaptive_magnify;
        float sample_y = circle_y + dy / adaptive_magnify;
        if (distance_to_center < circle_radius * 0.6) {
          float angle = atan2(dy, dx);
          float wave_intensity = effect_strength * distortion_strength * 0.6;
          float wave = sin(distance_to_center * 0.8 + time_anim * 0.008) * wave_intensity;
          sample_x += cos(angle + PI / 2) * wave * 1.5;
          sample_y += sin(angle + PI / 2) * wave * 1.5;
        }
        bool magnified_pixel = getPixelFromBitmap(source_bitmap, sample_x, sample_y);
        if (magnified_pixel) {
          u8g2.drawPixel(screen_x, screen_y);
        } else {
          u8g2.setDrawColor(0);
          u8g2.drawPixel(screen_x, screen_y);
          u8g2.setDrawColor(1);
        }
      }
    }
  }
}

// Draw metaballs
void drawMetaballs() {
  for (int x = 0; x < 64; x++) {
    for (int y = 0; y < 32; y++) {
      float pill_dist = distanceToPill(x, y);
      float pill_metaball = metaball_size / pill_dist;
      float pixel_value = pill_metaball;
      if (animation_active) {
        float circle_dist = sqrt(pow(x - circle_x, 2) + pow(y - circle_y, 2)) - circle_radius;
        circle_dist = max(circle_dist, 0.1f);
        float circle_metaball = metaball_size / circle_dist;
        pixel_value += circle_metaball;
      }
      if (pixel_value > 5.2 && pixel_value < 7.8) {
        u8g2.drawPixel(x, y);
      }
    }
  }
}

// Draw generic app screen
void drawAppScreen() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  const char* app_name = (item_selected < sizeof(icon_names) / sizeof(icon_names[0])) ?
                         icon_names[item_selected] : "Unknown App";
  u8g2.setFont(u8g2_font_6x10_tf);
  int text_width = u8g2.getStrWidth(app_name);
  int x_pos = (64 - text_width) / 2;
  int y_pos = 20;
  u8g2.drawStr(x_pos, y_pos, app_name);
  u8g2.drawFrame(0, 0, 64, 32);
  u8g2.sendBuffer();
}

// Draw hold screen
void drawHoldScreen() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  const char* app_name = (item_selected < sizeof(icon_names) / sizeof(icon_names[0])) ?
                         icon_names[item_selected] : "Unknown";
  u8g2.setFont(u8g2_font_7x13_tf);
  int text_width = u8g2.getStrWidth(app_name);
  int x_pos = (64 - text_width) / 2;
  int y_pos = 20;
  u8g2.drawStr(x_pos, y_pos, app_name);
  u8g2.sendBuffer();
}

// Setup menu button handlers
void setupMenuHandlers() {
  button.attachClick(handleClick);
  button.attachLongPressStart(handleLongPressStart);
  button.attachLongPressStop(handleLongPressStop);
  button.attachDoubleClick(handleDoubleClick);
  button.setClickTicks(80);
  button.setPressTicks(600);
  button.setDebounceTicks(30);
  Serial.println("Menu handlers attached");
}

// Setup camera button handlers
void setupCameraHandlers() {
  button.attachClick(handleCameraClick);
  button.attachLongPressStart(handleCameraLongPressStart);
  button.attachLongPressStop(handleCameraLongPressStop);
  button.attachDoubleClick(handleDoubleClick);
  button.setClickTicks(80);
  button.setPressTicks(1000);  // Longer press for camera video recording
  button.setDebounceTicks(30);
  Serial.println("Camera handlers attached");
}

// Button event handlers
void handleClick() {
  if (current_state == STATE_MENU && !menu_cleared) {
    startAnimation();
    Serial.println("Navigating to next icon");
  } else if (current_state == STATE_CAMERA) {
    handleCameraClick();
  }
}

void handleLongPressStart() {
  if (current_state == STATE_MENU) {
    menu_cleared = true;
    menu_clear_time = millis();
    Serial.print("Holding - showing app: ");
    Serial.println((item_selected < sizeof(icon_names) / sizeof(icon_names[0])) ?
                   icon_names[item_selected] : "Unknown");
  } else if (current_state == STATE_CHATBOT && !isChatbotRecording) {
    if (!isWiFiConnected) {
      Serial.println("Cannot start recording: WiFi not connected");
      showStartupMessage("No WiFi");
      delay(1000);
      return;
    }
    isChatbotRecording = true;
    Serial.println("Recording started for chatbot");
  } else if (current_state == STATE_CAMERA) {
    handleCameraLongPressStart();
  }
}

void handleLongPressStop() {
  if (current_state == STATE_MENU && menu_cleared) {
    Serial.print("Attempting to enter app: ");
    Serial.println((item_selected < sizeof(icon_names) / sizeof(icon_names[0])) ?
                   icon_names[item_selected] : "Unknown App");
    // Check WiFi status for apps that require it
    if (item_selected == APP_TRANSCRIBE || item_selected == APP_AI) {
      if (!isWiFiConnected) {
        Serial.println("Cannot enter app: WiFi not connected");
        showStartupMessage("No WiFi");
        delay(1000);
        menu_cleared = false;
        return;
      }
    }
    // Proceed to enter the app
    if (item_selected == APP_TRANSCRIBE) {
      current_state = STATE_TRANSCRIPTION;
      webSocket.onEvent(webSocketEventTranscription);
      startApp("APP_TRANSCRIBE");
    } else if (item_selected == APP_AI) {
      current_state = STATE_CHATBOT;
      webSocket.onEvent(webSocketEventChatbot);
      startApp("APP_CHATBOT");
    } else if (item_selected == APP_CAMERA) {
      current_state = STATE_CAMERA;
      setupCameraHandlers();
      startCameraApp();
    } else {
      current_state = STATE_APP;
      drawAppScreen();
    }
    menu_cleared = false;
  } else if (current_state == STATE_CHATBOT && isChatbotRecording) {
    isChatbotRecording = false;
    if (webSocket.isConnected()) {
      webSocket.sendTXT("EOS");
      Serial.println("Recording stopped, sent EOS to server");
    }
  } else if (current_state == STATE_CAMERA) {
    handleCameraLongPressStop();
  }
}

void handleDoubleClick() {
  if (current_state == STATE_APP || current_state == STATE_TRANSCRIPTION || 
      current_state == STATE_CHATBOT || current_state == STATE_CAMERA) {
    if (current_state == STATE_TRANSCRIPTION || current_state == STATE_CHATBOT) {
      stopApp();
    } else if (current_state == STATE_CAMERA) {
      stopCameraApp();
    }
    current_state = STATE_MENU;
    menu_cleared = false;
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    
    // Reattach menu handlers when returning to menu
    setupMenuHandlers();
    
    Serial.println("Exiting app - returning to menu");
  }
}

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_6x10_tf);
  showStartupMessage("Connecting...");

  // Start WiFi connection (non-blocking)
  WiFi.begin(ssid, password);
  isWiFiConnecting = true;
  isWiFiConnected = false;
  Serial.println("Attempting to connect to WiFi...");

  // Initialize I2S for audio
  I2S.setAllPins(-1, 42, 41, -1, -1);
  if (!I2S.begin(PDM_MONO_MODE, SAMPLE_RATE, SAMPLE_BITS)) {
    Serial.println("Failed to initialize I2S!");
    showStartupMessage("Mic Error");
    while (1);
  }

  // Setup initial menu handlers
  setupMenuHandlers();

  item_sel_previous = (item_selected - 1 + epd_bitmap_allArray_LEN) % epd_bitmap_allArray_LEN;
  item_sel_next = (item_selected + 1) % epd_bitmap_allArray_LEN;

  Serial.println("ESP32 S3 UI - Menu System with Camera, Chatbot, Transcription!");
  Serial.print("Total icons available: ");
  Serial.println(epd_bitmap_allArray_LEN);
  Serial.println("Controls:");
  Serial.println("- Click: Navigate menu");
  Serial.println("- Hold in menu: Enter app");
  Serial.println("- Hold in AI app: Start/stop recording question");
  Serial.println("- Click in Camera app: Take photo");
  Serial.println("- Hold in Camera app: Record 15s video");
  Serial.println("- Double-click (in app): Exit to menu");
  Serial.println("- AI app: Hold to record, release to send question");
  Serial.println("- Transcribe app: Auto-start continuous transcription");
  Serial.println("- Camera app: Click for photo, hold for video");
}

void loop() {
  button.tick();

  // Check WiFi connection status
  if (isWiFiConnecting && !isWiFiConnected) {
    if (WiFi.status() == WL_CONNECTED) {
      isWiFiConnecting = false;
      isWiFiConnected = true;
      Serial.println("Connected to WiFi");
      showStartupMessage("WiFi Connected");
      delay(1000); // Briefly show the connected message
    }
  }

  if (current_state == STATE_MENU) {
    if (menu_cleared) {
      drawHoldScreen();
      if (millis() - menu_clear_time > MENU_CLEAR_DURATION) {
        menu_cleared = false;
        Serial.println("Hold cancelled - timeout");
      }
    } else {
      u8g2.clearBuffer();
      u8g2.setDrawColor(1);
      if (animation_active) {
        time_anim += 3.3;
        circle_x = 75 - pow(time_anim, 0.978) * 1.3;
        float circle_left_edge = circle_x - circle_radius;
        if (circle_left_edge < 64 && !icon_transition_started) {
          item_sel_previous = item_selected;
          item_selected = (item_selected + 1) % epd_bitmap_allArray_LEN;
          item_sel_next = (item_selected + 1) % epd_bitmap_allArray_LEN;
          icon_transition_started = true;
          Serial.print("Transition to icon: ");
          Serial.println(item_selected);
        }
        if (icon_transition_started) {
          drawOptimizedMagnifyReveal();
        } else {
          drawOptimizedMagnifyingGlass();
        }
        if (circle_x < -20) {
          time_anim = 0.0;
          circle_x = 95;
          animation_active = false;
          icon_transition_started = false;
          Serial.println("Animation complete!");
        }
      } else {
        u8g2.drawXBMP(0, 0, 64, 32, epd_bitmap_allArray[item_selected]);
      }
      drawMetaballs();
      // Show WiFi status in the corner if still connecting
      if (isWiFiConnecting) {
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.drawStr(0, 6, "WiFi...");
      }
      u8g2.sendBuffer();
    }
  } else if (current_state == STATE_TRANSCRIPTION || current_state == STATE_CHATBOT) {
    loopApp();
    drawDisplay();
  } else if (current_state == STATE_CAMERA) {
    loopCamera();
  } else if (current_state == STATE_APP) {
    drawAppScreen();
  }
}

void startAnimation() {
  if (!animation_active && current_state == STATE_MENU && !menu_cleared) {
    animation_active = true;
    time_anim = 0.0;
    circle_x = 95;
    icon_transition_started = false;
    Serial.println("Magnifying animation started!");
  }
}

void startApp(const char* appName) {
  Serial.printf("Starting app: %s\n", appName);
  if (!isStreaming && !isTranscribing) {
    clearAllPages();
    totalPages = 0;
    currentPage = 0;
    currentSentence = "";
    displayActive = false;
    
    webSocket.begin(ws_server, ws_port, ws_path);
    webSocket.setReconnectInterval(5000);
    
    if (current_state == STATE_CHATBOT) {
      isStreaming = true;
      isChatbotRecording = false;
      webSocket.onEvent(webSocketEventChatbot);
      Serial.println("Chatbot started - hold button to record");
      showStartupMessage("AI Ready");
    } else if (current_state == STATE_TRANSCRIPTION) {
      isTranscribing = true;
      webSocket.onEvent(webSocketEventTranscription);
      Serial.println("Transcription started - continuous mode");
      showStartupMessage("Transcribing...");
    }
    
    if (webSocket.isConnected()) {
      webSocket.sendTXT(appName);
    }
  }
}

void stopApp() {
  Serial.println("Stopping app process...");
  if (isStreaming || isTranscribing) {
    isStreaming = false;
    isTranscribing = false;
    isChatbotRecording = false;
    
    if (webSocket.isConnected()) {
      webSocket.sendTXT("EOS");
      webSocket.disconnect();
      unsigned long disconnect_start = millis();
      while (webSocket.isConnected() && (millis() - disconnect_start < 2000)) {
        webSocket.loop();
        delay(10);
      }
      if (webSocket.isConnected()) {
        Serial.println("Force closing WebSocket connection");
      }
    }
    
    currentPage = 0;
    totalPages = 0;
    currentSentence = "";
    lastUpdateTime = 0;
    displayActive = false;
    clearAllPages();
    
    Serial.println("App stopped and cleaned up");
  }
}

void loopApp() {
  if (!isStreaming && !isTranscribing) {
    return;
  }
  
  webSocket.loop();
  
  if (webSocket.isConnected()) {
    uint8_t i2s_data[1024];
    size_t bytes_read;
    esp_err_t result = esp_i2s::i2s_read(esp_i2s::I2S_NUM_0, i2s_data, sizeof(i2s_data), &bytes_read, pdMS_TO_TICKS(10));
    
    if (result == ESP_OK && bytes_read > 0) {
      if (SAMPLE_BITS > 0) {
        for (size_t i = 0; i < bytes_read; i += SAMPLE_BITS / 8) {
          (*(uint16_t *)(i2s_data + i)) <<= VOLUME_GAIN;
        }
      }
      
      if (current_state == STATE_TRANSCRIPTION && isTranscribing) {
        webSocket.sendBIN(i2s_data, bytes_read);
      } else if (current_state == STATE_CHATBOT && isStreaming && isChatbotRecording) {
        webSocket.sendBIN(i2s_data, bytes_read);
      }
    }
  }
  
  if (displayActive && totalPages > 0) {
    if (millis() - lastUpdateTime >= PAGE_DURATION) {
      if (currentPage < totalPages - 1) {
        currentPage++;
        lastUpdateTime = millis();
        updateDisplay();
      } else {
        if (millis() - lastUpdateTime >= (PAGE_DURATION + DISPLAY_PAUSE)) {
          clearDisplay();
          displayActive = false;
        }
      }
    }
  }
}

void drawDisplay() {
  if (!displayActive || totalPages == 0) {
    return;
  }
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  int lineHeight = 14;
  
  if (currentPage < totalPages) {
    for (int i = 0; i < MAX_LINES; i++) {
      if (displayPages[currentPage][i].length() > 0) {
        u8g2.setCursor(0, (i + 1) * lineHeight);
        u8g2.print(displayPages[currentPage][i]);
      }
    }
  }
  
  u8g2.sendBuffer();
}

void preparePages(String sentence) {
  clearAllPages();
  totalPages = 0;
  int pageIndex = 0;
  int lineIndex = 0;
  int start = 0;
  
  while (start < sentence.length()) {
    int space = sentence.indexOf(' ', start);
    String word = (space != -1) ? sentence.substring(start, space) : sentence.substring(start);
    
    if (displayPages[pageIndex][lineIndex].length() + word.length() + 1 > MAX_CHARS_PER_LINE) {
      lineIndex++;
      if (lineIndex >= MAX_LINES) {
        pageIndex++;
        lineIndex = 0;
        if (pageIndex >= MAX_PAGES) {
          Serial.println("Warning: Text too long, truncating at " + String(MAX_PAGES) + " pages");
          break;
        }
      }
    }
    
    if (displayPages[pageIndex][lineIndex].length() > 0) {
      displayPages[pageIndex][lineIndex] += " ";
    }
    displayPages[pageIndex][lineIndex] += word;
    
    if (space == -1) break;
    start = space + 1;
  }
  
  totalPages = pageIndex + 1;
  if (totalPages > MAX_PAGES) {
    totalPages = MAX_PAGES;
  }
  
  Serial.println("Prepared " + String(totalPages) + " pages");
}

void clearAllPages() {
  for (int i = 0; i < MAX_PAGES; i++) {
    for (int j = 0; j < MAX_LINES; j++) {
      displayPages[i][j] = "";
    }
  }
}

void updateDisplay() {
  if (!displayActive || totalPages == 0 || currentPage >= totalPages) {
    return;
  }
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  int lineHeight = 16;
  
  for (int i = 0; i < MAX_LINES; i++) {
    if (displayPages[currentPage][i].length() > 0) {
      u8g2.setCursor(0, (i + 1) * lineHeight);
      u8g2.print(displayPages[currentPage][i]);
    }
  }
  
  u8g2.sendBuffer();
  Serial.println("Displaying page " + String(currentPage + 1) + "/" + String(totalPages));
}

void clearDisplay() {
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  clearAllPages();
  totalPages = 0;
  currentPage = 0;
  displayActive = false;
}

void showStartupMessage(String msg) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_unifont_t_vietnamese1);
  u8g2.setCursor(0, 10);
  u8g2.print(msg);
  u8g2.sendBuffer();
}

void setMagnifyStrength(float strength) {
  magnify_strength = constrain(strength, 1.0, 2.0);
}

void setDistortionStrength(float strength) {
  distortion_strength = constrain(strength, 0.0, 0.3);
}

void setPillSize(int radius, int length) {
  pill_radius = radius;
  pill_length = length;
}

void setCircleRadius(int radius) {
  circle_radius = radius;
}

void setSelectedIcon(int icon_index) {
  if (icon_index >= 0 && icon_index < epd_bitmap_allArray_LEN) {
    item_selected = icon_index;
    item_sel_previous = (item_selected - 1 + epd_bitmap_allArray_LEN) % epd_bitmap_allArray_LEN;
    item_sel_next = (item_selected + 1) % epd_bitmap_allArray_LEN;
  }
}

const char* getCurrentAppName() {
  return (item_selected < sizeof(icon_names) / sizeof(icon_names[0])) ?
         icon_names[item_selected] : "Unknown App";
}