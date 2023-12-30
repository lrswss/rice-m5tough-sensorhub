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

#ifndef _RTC_H
#define _RTC_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <sys/time.h>

#define NTP_UPDATE_SECS  1800  // interval in seconds between updates
#ifdef MEMORY_DEBUG_INTERVAL_SECS
extern UBaseType_t stackWmNtpTask;
#endif

class SystemTime {
    public:
        SystemTime();
        void begin();
        uint32_t getRuntimeMinutes();
        char* getTimeString();
        char* getDateString();
        bool isTimeSet();
        ~SystemTime();
    private:
        void set(time_t epoch);
        void setRTCFromNTP();
        void setFromRTC();
        void ntpTask();
        static void ntpTaskWrapper(void* parameter);
        TaskHandle_t ntpTaskHandle;
};

extern SystemTime SysTime;
#endif