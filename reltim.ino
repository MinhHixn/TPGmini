#include <WiFi.h>
#include <WebSocketsClient.h>
#include <I2S.h>
#include "transcription.h"

bool isTranscribing = false;

void webSocketEventTranscription(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[Transcription] WebSocket disconnected");
      isTranscribing = false;
      displayActive = false;
      showStartupMessage("Disconnected");
      break;
    case WStype_CONNECTED:
      Serial.println("[Transcription] WebSocket connected");
      showStartupMessage("WS Connected");
      webSocket.sendTXT("APP_TRANSCRIBE");
      break;
    case WStype_TEXT: {
      String text = String((char*)payload);
      text.trim();
      if (text.length() > 0) {
        Serial.println("[Transcription] Received: " + text);
        currentSentence = text;
        preparePages(currentSentence);
        lastUpdateTime = millis();
        currentPage = 0;
        displayActive = true;
        updateDisplay();
      }
      break;
    }
  }
}