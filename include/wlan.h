/***************************************************************************
  Copyright (c) 2023-2024 Lars Wessels

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

#ifndef _WLAN_H
#define _WLAN_H

#include <Arduino.h>
#include <M5Tough.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "utils.h"

#define WIFI_RETRY_SECS 30
#define WIFI_PORTAL_SSID "SensorHub"
#define WIFI_MIN_RSSI 30
#define WIFI_CONNECT_TIMEOUT_SECS 10
#define WIFI_SETUP_TIMEOUT_SECS 180

#ifdef MEMORY_DEBUG_INTERVAL_SECS
extern UBaseType_t stackWmWifiTask;
#endif


class WLAN {
    public:
        void begin();
        uint16_t wifiReconnectSuccess;
        uint16_t wifiReconnectFail;
        ~WLAN();
    private:
        void wifiManager(bool forcePortal);
        void connectionTask();
        static void connectionTaskWrapper(void* parameter);
        static void connectionFailed(const char* apname);
        static void connectionSuccess(bool success);
        static char* randomPassword();
        static void startPortalCallback(WiFiManager *wm);
        static void portalTimeoutCallback();
        static void saveParamsCallback();
        static void saveWifiCallback();
        static void eventStartPortal(Event& e);
        char ssid[32];
        char psk[32];
        TaskHandle_t connectionTaskHandle;
};

extern WLAN WifiUplink;
#endif