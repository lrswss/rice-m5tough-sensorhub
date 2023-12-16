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

QueueHandle_t statusMsgQueue;

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
void displayStatusMsg(const char* msg, uint16_t pos, bool bold, uint16_t cText, uint16_t cBack) {
    M5.Lcd.setTextColor(cText);
    M5.Lcd.fillRect(0, 205, 320, 35, cBack);
    if (bold)
        M5.Lcd.setFreeFont(&FreeSansBold12pt7b);
    else
        M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(pos, 230);
    M5.Lcd.printf(msg);
}


void updateStatusBar() {
    static time_t lastUpdate = 0;
    static bool showMessage = false;
    StatusMsg_t statusMsg;

    if (tsDiff(lastUpdate) >= 1000) {
        lastUpdate = millis();
        if (showMessage && xQueueReceive(statusMsgQueue, &statusMsg, 0) == pdTRUE) {
            displayStatusMsg(statusMsg.text, statusMsg.position, statusMsg.bold,
                statusMsg.colorText, statusMsg.colorBackground);
            showMessage = false;
        } else {
            displayStatusMsg(getDateString(), 15, false, WHITE, BLUE);
            M5.Lcd.setCursor(215, 230);
            M5.Lcd.print(getTimeString());
            showMessage = uxQueueMessagesWaiting(statusMsgQueue) ? true : false;
        }
    }
}


void queueStatusMsg(const char* text, uint16_t pos, bool warning) {
    StatusMsg_t msg;

    strlcpy(msg.text, text, sizeof(msg.text));
    msg.position = pos;
    msg.bold = true;
    msg.ts = millis();

    if (uxQueueMessagesWaiting(statusMsgQueue) == STATUS_MESSAGE_QUEUE_SIZE)
        Serial.println("ERROR: statusMsgQueue overflow");

    if (warning) {
        msg.colorText = WHITE;
        msg.colorBackground = RED;
        xQueueSendToFront(statusMsgQueue, (void*)&msg, 250/portTICK_PERIOD_MS);
    } else {
        msg.colorText = BLUE;
        msg.colorBackground = WHITE;
        xQueueSendToBack(statusMsgQueue, (void*)&msg, 250/portTICK_PERIOD_MS);
    }
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
    Serial.printf("WTD: timeout set to %d seconds\n", WATCHDOG_TIMEOUT_SEC);
}


// stop watchdog timer
void stopWatchdog() {
    esp_task_wdt_delete(NULL);
    esp_task_wdt_deinit();
    Serial.println("WDT: disabled");
}


// turn hex byte array of given length into a null-terminated hex string
void array2string(const byte *arr, int len, char *buf) {
    for (int i = 0; i < len; i++)
        sprintf(buf + i * 2, "%02X", arr[i]);
}


#ifdef MEMORY_DEBUG_INTERVAL_SECS
void printFreeHeap() {
    static time_t lastMsgMillis = 0;

    if (tsDiff(lastMsgMillis) > (MEMORY_DEBUG_INTERVAL_SECS * 1000)) {
        Serial.printf("DEBUG: runtime %d min, FreeHeap %d bytes\n",
            getRuntimeMinutes(), ESP.getFreeHeap());
        lastMsgMillis = millis();
    }
}

UBaseType_t printFreeStackWatermark(const char *taskName) {
    UBaseType_t wm = uxTaskGetStackHighWaterMark(NULL);
    Serial.printf("DEBUG: %s, runtime %d min, Core %d, StackHighWaterMark %d bytes\n",
        taskName, getRuntimeMinutes(), xPortGetCoreID(), wm);
    return wm;
}
#endif