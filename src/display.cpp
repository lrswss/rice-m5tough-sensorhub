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

#include "config.h"
#include "display.h"
#include "rtc.h"
#include "utils.h"

QueueHandle_t statusMsgQueue = NULL;


// print fake ° symbol on display (font size 36pt)
void printDegree(uint16_t color) {
    M5.Lcd.fillCircle(M5.Lcd.getCursorX()+14, M5.Lcd.getCursorY()-42, 8, WHITE);
    M5.Lcd.fillCircle(M5.Lcd.getCursorX()+14, M5.Lcd.getCursorY()-42, 4, color);
}


#ifdef DISPLAY_LOGO
void displayLogo() {
    M5.Lcd.clearDisplay(WHITE);
    M5.Lcd.pushImage(5, 70, logoWidth, logoHeight, logo);
    delay(3000);
}
#endif


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
    M5.Lcd.setCursor(105,160);
    M5.Lcd.printf("Firmware %s", FIRMWARE_VERSION);
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
    M5.Lcd.print(msg);
}


// query status message queue every second and displays
// date/time if there's no message wait
void updateStatusBar() {
    static time_t lastUpdate = 0, showMessageUntil = 0;
    static bool showMessage = false;
    StatusMsg_t statusMsg;

    if (tsDiff(lastUpdate) >= 1000 && !blockScreen) {
        lastUpdate = millis();
        if (showMessageUntil > millis()) {
            return;
        } else if (showMessage && (xQueueReceive(statusMsgQueue, &statusMsg, 0) == pdTRUE)) {
            showMessageUntil = millis() + (statusMsg.durationSecs * 1000);
            displayStatusMsg(statusMsg.text, statusMsg.positionText, statusMsg.boldText,
                statusMsg.colorText, statusMsg.colorBackground);
            showMessage = false; // display date/time next
        } else {
            displayStatusMsg(SysTime.getDateString(), 15, false, WHITE, BLUE);
            if (SysTime.isTimeSet())
                M5.Lcd.setCursor(215, 230);
            else
                M5.Lcd.setCursor(245, 230);
            M5.Lcd.print(SysTime.getTimeString());
            showMessage = (uxQueueMessagesWaiting(statusMsgQueue) > 0) ? true : false;
        }
    }
}


// queue status messages for display with updateStatusBar()
// warning messages are placed in the front of the queue and displayed for 2 secs
bool queueStatusMsg(const char* text, uint16_t pos, bool warning) {
    StatusMsg_t msg;

    if (statusMsgQueue == NULL)
        return false;

    strlcpy(msg.text, text, sizeof(msg.text));
    msg.positionText = pos;

    msg.durationSecs = warning ? 2 : 1;
    msg.boldText = true;
    msg.colorText = warning ? WHITE : BLUE;
    msg.colorBackground = warning ? RED : WHITE;

    if (uxQueueMessagesWaiting(statusMsgQueue) == STATUS_MESSAGE_QUEUE_SIZE) {
        Serial.println("ERROR: statusMsgQueue overflow");
        return false;
    }

    if (warning) 
        return (xQueueSendToFront(statusMsgQueue, (void*)&msg, 250/portTICK_PERIOD_MS) == pdTRUE);
    else 
        return (xQueueSendToBack(statusMsgQueue, (void*)&msg, 250/portTICK_PERIOD_MS) == pdTRUE);
}