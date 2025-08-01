#ifndef CHATBOT_H
#define CHATBOT_H

#include "config.h"

// Chatbot control
extern bool isStreaming;
extern bool isChatbotRecording;

// Function declarations
void webSocketEventChatbot(WStype_t type, uint8_t *payload, size_t length);

#endif