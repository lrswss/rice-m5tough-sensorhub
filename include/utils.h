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

#ifndef _UTILS_H
#define _UTILS_H

#include <Arduino.h>
#include <M5Tough.h>
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_task_wdt.h"

#define BATTERY_LEVEL_INTERVAL_SECS 120
#define BATTERY_WARNING_LEVEL 50
#define BATTERY_LOW_LEVEL 35
#define BATTERY_SHUTDOWN_LEVEL 20
#define BATTERY_LOW_DEEPSLEEP_SECS 300

#define WATCHDOG_TIMEOUT_SEC 90
#define MEMORY_DEBUG_INTERVAL_SECS 20

extern Gesture swipeRight;
extern bool blockScreen;
extern bool lowBattery;

time_t tsDiff(time_t tsMillis);
String getSystemID();
void startWatchdog();
void stopWatchdog();
void array2string(const byte *arr, int len, char *buf);
void displayPowerStatus(bool fullScreen);
bool usbPowered();
void confirmRestart(Event &e);
void lowBatteryCheck();
#ifdef MEMORY_DEBUG_INTERVAL_SECS
void printFreeHeap();
UBaseType_t printFreeStackWatermark(const char *taskName);
#endif

#endif