#ifndef TRANSCRIPTION_H
#define TRANSCRIPTION_H

#include "config.h"

// Transcription control
extern bool isTranscribing;

// Function declarations
void webSocketEventTranscription(WStype_t type, uint8_t *payload, size_t length);

#endif