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
#include "utils.h"
#include "prefs.h"
#include "rtc.h"

time_t lastStatusMsg = 0;

// print fake ° symbol on display
void printDegree(uint16_t color) {
    M5.Lcd.fillCircle(M5.Lcd.getCursorX()+14, M5.Lcd.getCursorY()-42, 8, WHITE);
    M5.Lcd.fillCircle(M5.Lcd.getCursorX()+14, M5.Lcd.getCursorY()-42, 4, color);
}


void displayLogo() {
    M5.Lcd.clearDisplay(WHITE);
    M5.Lcd.pushImage(5, 70, logoWidth, logoHeight, logo);
    delay(3000);
}


// screen displayed during device startup sequence
void displaySplashScreen() {
    M5.Lcd.clearDisplay(WHITE);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.setFreeFont(&FreeSansBold24pt7b);
    M5.Lcd.setCursor(28, 80);
    M5.Lcd.print("Sensor Hub");
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(12,125);
    M5.Lcd.print("MLX90614/SFA30/BME680");
    M5.Lcd.setFreeFont(&FreeSans9pt7b);
    M5.Lcd.setCursor(100,160);
    M5.Lcd.printf("Firmware %s", FIRMWARE_VERSION);
}


// rollover safe comparison for given timestamp with millis()
time_t tsDiff(time_t tsMillis) {
    if ((millis() - tsMillis) < 0)
        return (LONG_MAX - tsMillis + 1);
    else
        return (millis() - tsMillis);
}


// display status message on bottom of screen
void displayStatusMsg(const char msg[], uint16_t pos, bool bold, uint16_t colorText, uint16_t colorBackground) {
    M5.Lcd.setTextColor(colorText);
    M5.Lcd.fillRect(0, 205, 320, 40, colorBackground);
    if (bold)
        M5.Lcd.setFreeFont(&FreeSansBold12pt7b);
    else
        M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(pos, 230);
    M5.Lcd.printf(msg);
    lastStatusMsg = millis();
}


// returns hardware system id (last 3 bytes of mac address)
String getSystemID() {
    uint8_t mac[8];
    char sysid[8];
    
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(sysid, "%02X%02X%02X", mac[3], mac[4], mac[5]);
    return String(sysid);
}


// initialize and start watchdog for this thread
void startWatchdog() {
    esp_task_wdt_init(WATCHDOG_TIMEOUT_SEC, true);
    esp_task_wdt_add(NULL);
    Serial.printf("Watchdog timeout set to %d seconds\n", WATCHDOG_TIMEOUT_SEC);
}


// stop watchdog timer
void stopWatchdog() {
    esp_task_wdt_delete(NULL);
    esp_task_wdt_deinit();
    Serial.println(F("[WARNING] watchdog disabled"));
}

#ifdef MEMORY_DEBUG_INTERVAL_MIN
void memoryDebug() {
    static time_t lastMsgMillis = 0;

    if ((millis() - lastMsgMillis) > (MEMORY_DEBUG_INTERVAL_MIN * 1000)) {
        Serial.printf("Runtime %d minutes, FreeHeap %d bytes\n",
            getRuntimeMinutes(), ESP.getFreeHeap());
        lastMsgMillis = millis();
    }
}
#endif