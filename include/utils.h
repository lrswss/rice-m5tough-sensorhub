/***************************************************************************
  Copyright (c) 2023 Lars Wessels

  This file a part of the "RICE-M5Tough-SensorHub" source code.

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

#ifndef _UTILS_H
#define _UTILS_H

#include <Arduino.h>
#include <M5Tough.h>
#include "FreeSansBold36pt7b.h"
#include "FreeSans8pt7b.h"
#include "logo.h"

void displayLogo();
void displaySplashScreen();
void displayStatusMsg(const char msg[], uint16_t pos, bool bold, uint16_t colorText, uint16_t colorBackground);
void printDegree(uint16_t color);
time_t tsDiff(time_t tsMillis);
String getSystemID();

#endif