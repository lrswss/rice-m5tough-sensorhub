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

#include "config.h"
#include "wlan.h"


void wifi_connect() {
    uint8_t waitSecs = 0;
    
    M5.Lcd.fillScreen(BLUE);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(20,40);
    M5.Lcd.print("Connecting to WiFi..");
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    while (waitSecs < WIFI_WAIT_SECS) {
        if (WiFi.status() != WL_CONNECTED) {
            M5.Lcd.print(".");
            delay(1000);
            waitSecs++;
        } else {
            M5.Lcd.print("OK");
            M5.Lcd.setCursor(20,100);
            M5.Lcd.print("SSID: ");
            M5.Lcd.println(WiFi.SSID());
            M5.Lcd.setCursor(20,130);
            M5.Lcd.print("RSSI: ");
            M5.Lcd.print(WiFi.RSSI());
            M5.Lcd.println(" dBm");
            M5.Lcd.setCursor(20,160);
            M5.lcd.print("IP: ");
            M5.lcd.println(WiFi.localIP());
            Serial.printf("WiFi: connected to %s with IP ", WiFi.SSID().c_str());
            Serial.println(WiFi.localIP());
            delay(2000);
            break;
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        M5.Lcd.fillScreen(RED);
        M5.Lcd.setCursor(30,70);
        M5.Lcd.print("Failed to connect to SSID");
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString(WIFI_SSID, 160, 120, 4);
        Serial.printf("WiFi: connection to SSID %s failed!\n", WIFI_SSID);
        delay(5000);
    }
}


void wifi_reconnect() {
    static time_t wifiReconnect = millis() + (WIFI_RETRY_SECS * 1000);

    if (millis() > wifiReconnect && WiFi.status() != WL_CONNECTED) {
        wifiReconnect = millis() + (WIFI_RETRY_SECS * 1000);
        wifi_connect();
    }
}