#ifndef CAMERA_H
#define CAMERA_H

#include "esp_camera.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <U8g2lib.h>

// CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
// CAMERA PINS
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

#define LED_GPIO_NUM      21

// Camera states
enum CameraState {
  CAMERA_IDLE,
  CAMERA_TAKING_PHOTO,
  CAMERA_RECORDING_VIDEO
};

// Camera variables
extern CameraState camera_state;
extern bool camera_initialized;
extern bool sd_initialized;
extern int imageCount;
extern int videoCount;
extern bool isCameraRecording;
extern unsigned long recordingStartTime;
extern unsigned long lastLedToggle;
extern unsigned long lastFrameTime;
extern bool ledState;
extern File videoFile;
extern uint32_t frameCount;
extern uint32_t totalVideoSize;
extern uint32_t aviHeaderSize;

// Constants
extern const unsigned long RECORD_DURATION;
extern const unsigned long LED_BLINK_INTERVAL;
extern const unsigned long FRAME_INTERVAL;

// AVI file structure
struct AviHeader {
    // RIFF header
    uint8_t riff_header[4] = {'R', 'I', 'F', 'F'};
    uint32_t file_size;
    uint8_t wave_header[4] = {'A', 'V', 'I', ' '};
    
    // LIST hdrl
    uint8_t list_header[4] = {'L', 'I', 'S', 'T'};
    uint32_t list_size = 192;
    uint8_t list_type[4] = {'h', 'd', 'r', 'l'};
    
    // avih chunk
    uint8_t avih_header[4] = {'a', 'v', 'i', 'h'};
    uint32_t avih_size = 56;
    uint32_t microsec_per_frame = 40000; // 25 FPS
    uint32_t max_bytes_per_sec = 0;
    uint32_t padding_granularity = 0;
    uint32_t flags = 0x10;
    uint32_t total_frames;
    uint32_t initial_frames = 0;
    uint32_t streams = 1;
    uint32_t suggested_buffer_size = 0;
    uint32_t width = 320;
    uint32_t height = 240;
    uint32_t reserved[4] = {0};
    
    // LIST strl
    uint8_t strl_header[4] = {'L', 'I', 'S', 'T'};
    uint32_t strl_size = 116;
    uint8_t strl_type[4] = {'s', 't', 'r', 'l'};
    
    // strh chunk
    uint8_t strh_header[4] = {'s', 't', 'r', 'h'};
    uint32_t strh_size = 56;
    uint8_t stream_type[4] = {'v', 'i', 'd', 's'};
    uint8_t handler[4] = {'M', 'J', 'P', 'G'};
    uint32_t stream_flags = 0;
    uint16_t priority = 0;
    uint16_t language = 0;
    uint32_t initial_frames_stream = 0;
    uint32_t scale = 1;
    uint32_t rate = 25;
    uint32_t start = 0;
    uint32_t length;
    uint32_t suggested_buffer_size_stream = 0;
    uint32_t quality = 10000;
    uint32_t sample_size = 0;
    uint16_t rect_left = 0;
    uint16_t rect_top = 0;
    uint16_t rect_right = 320;
    uint16_t rect_bottom = 240;
    
    // strf chunk
    uint8_t strf_header[4] = {'s', 't', 'r', 'f'};
    uint32_t strf_size = 40;
    uint32_t bi_size = 40;
    uint32_t bi_width = 320;
    uint32_t bi_height = 240;
    uint16_t bi_planes = 1;
    uint16_t bi_bit_count = 24;
    uint8_t bi_compression[4] = {'M', 'J', 'P', 'G'};
    uint32_t bi_size_image = 0;
    uint32_t bi_x_pels_per_meter = 0;
    uint32_t bi_y_pels_per_meter = 0;
    uint32_t bi_clr_used = 0;
    uint32_t bi_clr_important = 0;
    
    // LIST movi
    uint8_t movi_header[4] = {'L', 'I', 'S', 'T'};
    uint32_t movi_size;
    uint8_t movi_type[4] = {'m', 'o', 'v', 'i'};
};

// Function declarations
bool initializeCamera();
bool initializeSDCard();
void setCounters();
int getHighestFile(File dir, String prefix);
int extractIntFromString(String str);
void writeFile(fs::FS &fs, const char * path, uint8_t * data, size_t len);
void photo_save(const char * fileName);
void takePhoto();
void startVideoRecording();
void stopVideoRecording();
void writeAviHeader();
void writeVideoFrame();
void finalizeAvi();
void blinkLED();
void drawCameraScreen();
void updateCameraOLED();
void showPhotoTaken();
void showRecordingScreen(int secondsLeft);
void handleCameraClick();
void handleCameraLongPressStart();
void handleCameraLongPressStop();
void loopCamera();
void startCameraApp();
void stopCameraApp();

#endif