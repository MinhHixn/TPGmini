#include "camera.h"
#include <U8g2lib.h>
#include <OneButton.h>

// External references
extern U8G2_SSD1306_64X32_1F_F_HW_I2C u8g2;
extern OneButton button;

// Camera variables
bool camera_initialized = false;
bool sd_initialized = false;
int imageCount = 1;
int videoCount = 1;
bool isCameraRecording = false;
unsigned long recordingStartTime = 0;
unsigned long lastLedToggle = 0;
unsigned long lastFrameTime = 0;
bool ledState = false;
File videoFile;
uint32_t frameCount = 0;
uint32_t totalVideoSize = 0;
uint32_t aviHeaderSize = 0;

// Constants
const unsigned long RECORD_DURATION = 15000; // 15 seconds
const unsigned long LED_BLINK_INTERVAL = 500; // 500ms blink interval
const unsigned long FRAME_INTERVAL = 40; // 25 FPS

bool initializeCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    config.frame_size = FRAMESIZE_240X240;
    #if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
    #endif
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }
  return true;
}

bool initializeSDCard() {
  if (!SD.begin(21)) {
    Serial.println("SD Card Mount Failed");
    return false;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return false;
  }
  return true;
}

void setCounters() {
  File dir = SD.open("/");
  imageCount = getHighestFile(dir, "image") + 1;
  videoCount = getHighestFile(dir, "video") + 1;
  dir.close();
}

int getHighestFile(File dir, String prefix) {
  int highestNum = 0;
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;
    String fileName = entry.name();
    if (fileName.startsWith(prefix)) {
      int num = extractIntFromString(fileName);
      if (num > highestNum) highestNum = num;
    }
    entry.close();
  }
  return highestNum;
}

int extractIntFromString(String str) {
  String numString = "";
  for (int i = 0; i < str.length(); i++) {
    if (isDigit(str[i])) {
      numString += str[i];
    }
  }
  if (numString.length() > 0) {
    return numString.toInt();
  }
  return -1;
}

void writeFile(fs::FS &fs, const char * path, uint8_t * data, size_t len) {
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (!file.write(data, len)) {
    Serial.println("Write failed");
  }
  file.close();
}

void photo_save(const char * fileName) {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }
  writeFile(SD, fileName, fb->buf, fb->len);
  esp_camera_fb_return(fb);
  Serial.printf("Saved picture: %s\n", fileName);
}

void displayMenu() {
  Serial.println("XIAO ESP32S3 CAMERA APP - Photos: " + String(imageCount - 1));
  updateOLED();
}

void updateOLED() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_4x6_tr);
  String photoStr = String(imageCount - 1);
  u8g2.drawStr(45, 6, photoStr.c_str());
  u8g2.sendBuffer();
}

void showPhotoTaken() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(8, 16, "PHOTO");
  u8g2.drawStr(8, 28, "SAVED!");
  u8g2.sendBuffer();
  delay(1000);
  updateOLED();
}

void showRecordingScreen(int secondsLeft) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(4, 12, "REC");
  int progress = (15 - secondsLeft) * 60 / 15;
  u8g2.drawFrame(2, 29, 60, 6);
  u8g2.drawBox(2, 29, progress, 6);
  if (ledState) {
    u8g2.drawDisc(58, 8, 2);
  }
  u8g2.sendBuffer();
}

void writeAviHeader() {
  AviHeader header;
  header.total_frames = 0;
  header.length = 0;
  header.file_size = 0;
  header.movi_size = 0;
  videoFile.write((uint8_t*)&header, sizeof(AviHeader));
  aviHeaderSize = sizeof(AviHeader);
}

void writeVideoFrame() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed during video");
    return;
  }
  uint8_t chunkId[4] = {'0', '0', 'd', 'c'};
  uint32_t chunkSize = fb->len;
  videoFile.write(chunkId, 4);
  videoFile.write((uint8_t*)&chunkSize, 4);
  videoFile.write(fb->buf, fb->len);
  if (fb->len % 2 == 1) {
    uint8_t pad = 0;
    videoFile.write(&pad, 1);
    totalVideoSize += 1;
  }
  totalVideoSize += 8 + fb->len;
  frameCount++;
  esp_camera_fb_return(fb);
}

void finalizeAvi() {
  uint32_t finalFileSize = aviHeaderSize + totalVideoSize - 8;
  uint32_t moviSize = totalVideoSize + 4;
  videoFile.seek(0);
  AviHeader header;
  header.file_size = finalFileSize;
  header.total_frames = frameCount;
  header.length = frameCount;
  header.movi_size = moviSize;
  videoFile.write((uint8_t*)&header, aviHeaderSize);
  Serial.printf("Video saved: /video%d.avi (%d frames)\n", videoCount, frameCount);
}

void startVideoRecording() {
  isCameraRecording = true;
  frameCount = 0;
  totalVideoSize = 0;
  char filename[32];
  sprintf(filename, "/video%d.avi", videoCount);
  videoFile = SD.open(filename, FILE_WRITE);
  if (!videoFile) {
    Serial.println("Failed to create video file");
    isCameraRecording = false;
    return;
  }
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_framesize(s, FRAMESIZE_QVGA);
    s->set_quality(s, 10);
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
  }
  writeAviHeader();
  recordingStartTime = millis();
  lastFrameTime = millis();
  lastLedToggle = millis();
  Serial.println("Recording video: " + String(filename));
}

void stopVideoRecording() {
  if (isCameraRecording) {
    isCameraRecording = false;
    digitalWrite(LED_GPIO_NUM, LOW);
    if (videoFile) {
      finalizeAvi();
      videoFile.close();
    }
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
      s->set_framesize(s, FRAMESIZE_UXGA);
      s->set_quality(s, 10);
    }
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(8, 16, "VIDEO");
    u8g2.drawStr(8, 28, "SAVED!");
    u8g2.sendBuffer();
    delay(1500);
    videoCount++;
    displayMenu();
  }
}

void blinkLED() {
  if (isCameraRecording) {
    unsigned long currentTime = millis();
    if (currentTime - recordingStartTime >= RECORD_DURATION) {
      stopVideoRecording();
      return;
    }
    int secondsLeft = (RECORD_DURATION - (currentTime - recordingStartTime)) / 1000;
    if (currentTime - lastFrameTime >= FRAME_INTERVAL) {
      writeVideoFrame();
      lastFrameTime = currentTime;
    }
    if (currentTime - lastLedToggle >= LED_BLINK_INTERVAL) {
      ledState = !ledState;
      digitalWrite(LED_GPIO_NUM, ledState ? HIGH : LOW);
      lastLedToggle = currentTime;
      showRecordingScreen(secondsLeft);
    }
  }
}

void handleCameraClick() {
  if (!camera_initialized || !sd_initialized) {
    return; // Silently ignore if not initialized
  }
  if (isCameraRecording) {
    return; // Cannot take photo while recording
  }
  digitalWrite(LED_GPIO_NUM, HIGH);
  showPhotoTaken();
  digitalWrite(LED_GPIO_NUM, LOW);
  char filename[32];
  sprintf(filename, "/image%d.jpg", imageCount);
  photo_save(filename);
  imageCount++;
  displayMenu();
}

void handleCameraLongPressStart() {
  if (!camera_initialized || !sd_initialized) {
    return; // Silently ignore if not initialized
  }
  if (isCameraRecording) {
    return; // Already recording
  }
  startVideoRecording();
}

void handleCameraLongPressStop() {
  // Do nothing - video recording stops automatically after 15 seconds
}

void loopCamera() {
  if (!camera_initialized || !sd_initialized) {
    return; // Silently ignore if not initialized
  }
  if (isCameraRecording) {
    blinkLED();
  }
}

void startCameraApp() {
  camera_initialized = false;
  sd_initialized = false;
  
  pinMode(LED_GPIO_NUM, OUTPUT);
  digitalWrite(LED_GPIO_NUM, LOW);
  
  int attempts = 0;
  const int max_attempts = 3;
  while (attempts < max_attempts) {
    camera_initialized = initializeCamera();
    sd_initialized = initializeSDCard();
    
    if (camera_initialized && sd_initialized) {
      setCounters();
      displayMenu();
      
      // Attach button handlers
      button.attachClick(handleCameraClick);
      button.attachLongPressStart(handleCameraLongPressStart);
      button.attachLongPressStop(handleCameraLongPressStop);
      button.setPressTicks(1000);
      
      return;
    }
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(8, 16, "INIT");
    u8g2.drawStr(8, 28, "ERROR!");
    u8g2.sendBuffer();
    delay(500);
    attempts++;
  }
  
  Serial.println("Camera app startup failed");
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(8, 16, "FATAL");
  u8g2.drawStr(8, 28, "ERROR!");
  u8g2.sendBuffer();
}

void stopCameraApp() {
  if (isCameraRecording) {
    stopVideoRecording();
  }
  if (camera_initialized) {
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
      Serial.printf("Camera deinit failed with error 0x%x\n", err);
    }
  }
  if (sd_initialized) {
    SD.end();
  }
  digitalWrite(LED_GPIO_NUM, LOW);
  ledState = false;
  isCameraRecording = false;
  frameCount = 0;
  totalVideoSize = 0;
  aviHeaderSize = 0;
  camera_initialized = false;
  sd_initialized = false;
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  // Detach button handlers to prevent unintended triggers
  button.attachClick(NULL);
  button.attachLongPressStart(NULL);
  button.attachLongPressStop(NULL);
}