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

// setup the ntp udp client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, 0, (NTP_UPDATE * 1000));

// TimeZone Settings Details https://github.com/JChristensen/Timezone
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};
Timezone CE(CEST, CET);  // Frankfurt, Paris


static char* getTimeString(time_t epoch) {
    time_t t = CE.toLocal(epoch);
    static char strTime[9]; // HH:MM:SS
    
    if (epoch > 1000) { 
      sprintf(strTime, "%.2d:%.2d:%.2d", hour(t), minute(t), second(t));
    } else {
      strncpy(strTime, "--:--:--", 8); 
    }
    return strTime;
}


// returns ptr to array with current date
static char* getDateString(time_t epoch) {
    time_t t = CE.toLocal(epoch);
    static char strDate[11]; // DD.MM.YYYY

    if (epoch > 1000) {
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


void displayDateTime() {
    time_t epochTime = 0;

    if (WiFi.status() == WL_CONNECTED) {
        timeClient.update();
        epochTime = timeClient.getEpochTime();
        displayStatusMsg(getDateString(epochTime), 15, false, WHITE, BLUE);
        M5.Lcd.setCursor(215, 230);
        M5.Lcd.print(getTimeString(epochTime));
    }
}


void ntp_init() {
    timeClient.begin();
    if (WiFi.status() == WL_CONNECTED) {
        M5.Lcd.fillScreen(BLUE);
        M5.Lcd.setFreeFont(&FreeSans12pt7b);
        M5.Lcd.setCursor(20,40);
        M5.Lcd.print("Syncing time...");
        Serial.printf("Syncing time with NTP server %s...", NTP_ADDRESS);
        timeClient.forceUpdate();
        M5.Lcd.print("OK");
        Serial.println("OK");
        delay(1000);
    }
}





