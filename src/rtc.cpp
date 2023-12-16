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
#include "rtc.h"
#include "utils.h"
#include "wlan.h"
#include "prefs.h"

// setup the ntp udp client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, prefs.ntpServer, 0, (NTP_UPDATE * 1000));
static bool ntpReady = false;
#ifdef MEMORY_DEBUG_INTERVAL_SECS
UBaseType_t stackWmNtpTask;
#endif

// TimeZone Settings Details https://github.com/JChristensen/Timezone
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};
Timezone CE(CEST, CET);  // Frankfurt, Paris


static void ntpUpdateTask(void* parameter) {
#ifdef MEMORY_DEBUG_INTERVAL_SECS
    uint16_t loopCounter = 0;
#endif

    while(true) {
        if (WiFi.isConnected()) {
            if (!ntpReady) {
                timeClient.setPoolServerName(prefs.ntpServer);
                timeClient.begin();
                timeClient.forceUpdate();
                ntpReady = true;
            }
            timeClient.update();
        }
#ifdef MEMORY_DEBUG_INTERVAL_SECS
        if (loopCounter++ >= MEMORY_DEBUG_INTERVAL_SECS/5) {
            stackWmNtpTask = printFreeStackWatermark("ntpTask");
            loopCounter = 0;
        }
#endif
        vTaskDelay(5000/portTICK_PERIOD_MS);
    }
}


char* getTimeString() {
    static char strTime[16]; // HH:MM:SS
    time_t epoch = timeClient.getEpochTime();
    time_t t = CE.toLocal(epoch);

    if (timeClient.isTimeSet()) {
        sprintf(strTime, "%.2d:%.2d:%.2d", hour(t), minute(t), second(t));
    } else {
        strncpy(strTime, "     --:--:--", 13);
    }
    return strTime;
}


// returns ptr to array with current date
char* getDateString() {
    static char strDate[11]; // DD.MM.YYYY
    time_t epoch = timeClient.getEpochTime();
    time_t t = CE.toLocal(epoch);

    if (timeClient.isTimeSet()) {
        sprintf(strDate, "%.2d.%.2d.%4d", day(t), month(t), year(t));
    } else {
        strncpy(strDate, "--.--.----", 10);
    }
    return strDate;
}


// returns runtime in minutes
uint32_t getRuntimeMinutes() {
    static time_t lastMillis = 0;
    static uint32_t seconds = 0;

    seconds += tsDiff(lastMillis) / 1000;
    lastMillis = millis();
    return seconds/60;
}


void ntp_init() {
    uint8_t timeout = 0;

    M5.Lcd.clearDisplay(BLUE);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(20, 40);
    M5.Lcd.print("Sync time...");
    Serial.printf("NTP: sync time with server %s...", prefs.ntpServer);
    xTaskCreatePinnedToCore(ntpUpdateTask, "ntpTask", 2560, NULL, 3, NULL, 0);

   if (!WiFi.isConnected()) {
        M5.Lcd.print("no WiFi");
        Serial.println("aborting, no WiFi uplink");
        delay(1500);
        return;
    }

    // wait for ntpUpdateTask()
    while (timeout++ < 20 && !timeClient.isTimeSet())
        delay(250);

    if (timeClient.isTimeSet()) {
        M5.Lcd.print("OK");
        Serial.println("OK");
    } else {
        M5.Lcd.print("ERR");
        Serial.println("ERROR");
    }
    delay(1500);
}





