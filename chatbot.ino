#include <WiFi.h>
#include <WebSocketsClient.h>
#include <I2S.h>
#include "chatbot.h"

bool isStreaming = false;
bool isChatbotRecording = false;

void webSocketEventChatbot(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[Chatbot] WebSocket disconnected");
      isStreaming = false;
      isChatbotRecording = false;
      displayActive = false;
      showStartupMessage("Disconnected");
      break;
    case WStype_CONNECTED:
      Serial.println("[Chatbot] WebSocket connected");
      showStartupMessage("WS Connected");
      webSocket.sendTXT("APP_CHATBOT");
      break;
    case WStype_TEXT: {
      String text = String((char*)payload);
      text.trim();
      if (text.length() > 0) {
        Serial.println("[Chatbot] Received: " + text);
        if (text.indexOf("không") != -1 || text.indexOf("có") != -1) {
          text = "Gemini: " + text;
        }
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