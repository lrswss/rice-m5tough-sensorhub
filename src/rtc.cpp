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


// set ESP32‘s internal clock to UTC
static void setSystemTime(time_t epoch) {
    struct timeval tv;

    tv.tv_sec = epoch;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL); // UTC
}


// set RTC to from newtwork time (UTC)
static void setRTCFromNTP() {
    RTC_TimeTypeDef rtcTime;
    RTC_DateTypeDef rtcDate;

    time_t t = timeClient.getEpochTime();
    rtcTime.Seconds = second(t);
    rtcTime.Minutes = minute(t);
    rtcTime.Hours = hour(t);
    rtcDate.Date = day(t);
    rtcDate.Month = month(t);
    rtcDate.Year = year(t);
    rtcDate.WeekDay = weekday(t);

    M5.Rtc.SetTime(&rtcTime);
    M5.Rtc.SetDate(&rtcDate);
    Serial.printf("RTC: set from network time to %4d/%.2d/%.2d %.2d:%.2d:%.2d (UTC)\n", rtcDate.Year,
        rtcDate.Month, rtcDate.Date, rtcTime.Hours, rtcTime.Minutes, rtcTime.Seconds);
}

// set system time from RTC to UTC
static void setTimeFromRTC() {
    RTC_TimeTypeDef rtcTime;
    RTC_DateTypeDef rtcDate;
    static char str[56];
    struct tm tm;
    time_t epoch;

    M5.Rtc.GetTime(&rtcTime);
    M5.Rtc.GetDate(&rtcDate);
    tm.tm_year = rtcDate.Year - 1900;
    tm.tm_mon = rtcDate.Month - 1;
    tm.tm_mday = rtcDate.Date;
    tm.tm_hour = rtcTime.Hours;
    tm.tm_min = rtcTime.Minutes;
    tm.tm_sec = rtcTime.Seconds;

    epoch = mktime(&tm);
    setSystemTime(epoch);
    strftime(str, sizeof(str), "RTC: set system time to %Y/%m/%d %H:%M:%S (UTC)", localtime(&epoch));
    Serial.println(str);
}


static void ntpUpdateTask(void* parameter) {
    time_t lastTimeSync = 0;
    bool initialTimeSync = true;
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
            // set RTC and system time to network time once every hour
            if (timeClient.isTimeSet() && (tsDiff(lastTimeSync) > (3600 * 1000) || initialTimeSync)) {
                lastTimeSync = millis();
                initialTimeSync = false;
                setSystemTime(timeClient.getEpochTime());
                if (millis() > (300 * 1000))
                    setRTCFromNTP();
            }
        }
        // sync system time from RTC every half hour
        if (timeClient.isTimeSet() && tsDiff(lastTimeSync) > (1800 * 1000)) {
            lastTimeSync = millis();
            setTimeFromRTC();
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
    static char strTime[9]; // HH:MM:SS
    time_t t = CE.toLocal(time(nullptr));

    if (t > 1701388800) { // 01/01/2023
        sprintf(strTime, "%.2d:%.2d:%.2d", hour(t), minute(t), second(t));
    } else {
        strncpy(strTime, "--:--:--", 8);
    }
    return strTime;
}


// returns ptr to array with current date
char* getDateString() {
    static char strDate[11]; // DD.MM.YYYY
    time_t t = CE.toLocal(time(nullptr));

    if (t > 1701388800) {
        sprintf(strDate, "%.2d.%.2d.%4d", day(t), month(t), year(t));
    } else {
        strncpy(strDate, "--.--.----", 10);
    }
    return strDate;
}


// returns runtime in minutes
uint32_t getRuntimeMinutes() {
    static time_t lastMillis = 0;
    static float seconds = 0.0;

    seconds += tsDiff(lastMillis) / 1000.0;
    lastMillis = millis();
    return (uint32_t)seconds/60;
}


void ntp_init() {
    uint8_t timeout = 0;

    M5.Lcd.clearDisplay(BLUE);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(20, 40);
    M5.Lcd.print("Get network time...");
    Serial.printf("NTP: sync time with server %s...", prefs.ntpServer);
    xTaskCreatePinnedToCore(ntpUpdateTask, "ntpTask", 2560, NULL, 3, NULL, 0);

   if (!WiFi.isConnected()) {
        M5.Lcd.print("no WiFi");
        Serial.println("aborting, no WiFi uplink");
        delay(1000);
        M5.Lcd.setCursor(20, 70);
        M5.Lcd.print("Set time from RTC...");
        setTimeFromRTC();
        M5.Lcd.print("OK");
        delay(1500);
        return;
    }

    // wait for ntpUpdateTask()
    while (timeout++ < 20 && !timeClient.isTimeSet())
        delay(250);

    if (timeClient.isTimeSet()) {
        setSystemTime(timeClient.getEpochTime());
        M5.Lcd.print("OK");
        Serial.println("OK");
        delay(1000);
        M5.Lcd.setCursor(20, 70);
        M5.Lcd.print("Set RTC from NTP...");
        setRTCFromNTP();
        M5.Lcd.print("OK");
    } else {
        M5.Lcd.print("ERR");
        Serial.println("ERROR");
        delay(1000);
        M5.Lcd.setCursor(20, 70);
        M5.Lcd.print("Set time from RTC...");
        setTimeFromRTC();
        M5.Lcd.print("OK");
    }
    delay(1500);
}