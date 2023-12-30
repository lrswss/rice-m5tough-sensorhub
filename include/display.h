/***************************************************************************
  Copyright (c) 2023 Lars Wessels

  This file a part of the "RICE-M5Tough-SensorHub" source code.
  https://github.com/lrswss/rice-m5tough-sensorhub

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

***************************************************************************/

#ifndef _DISPLAY_H
#define _DISPLAY_H

#include <Arduino.h>
#include <M5Tough.h>
#include "FreeSansBold36pt7b.h"
#include "config.h"
#ifdef DISPLAY_LOGO
#include "logo.h"
#endif

#define STATUS_MESSAGE_QUEUE_SIZE 5

typedef struct {
    char text[64];
    uint16_t positionText;
    uint16_t colorText;
    bool boldText;
    uint16_t colorBackground;
    uint8_t durationSecs;
} StatusMsg_t;

extern QueueHandle_t statusMsgQueue;

void displayLogo();
void displaySplashScreen();
void displayStatusMsg(const char* msg, uint16_t pos, bool bold, uint16_t cText, uint16_t cBack);
void queueStatusMsg(const char* text, uint16_t pos, bool warning);
void updateStatusBar();
void printDegree(uint16_t color);

#endif