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

#include "config.h"
#include "rtc.h"
#include "utils.h"
#include "wlan.h"
#include "prefs.h"

// setup the ntp udp client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

SystemTime SysTime;
#ifdef MEMORY_DEBUG_INTERVAL_SECS
UBaseType_t stackWmNtpTask;
#endif


SystemTime::SystemTime() {
    timeClient.setPoolServerName(prefs.ntpServer);
    timeClient.setUpdateInterval(NTP_UPDATE_SECS * 1000);
    setenv("TZ", LOCAL_TIMEZONE, 1);
    tzset();
}


SystemTime::~SystemTime() {
    timeClient.end();
    vTaskDelete(this->ntpTaskHandle);
}


// set ESP32‘s internal clock to UTC
void SystemTime::set(time_t epoch) {
    struct timeval tv;

    tv.tv_sec = epoch;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL); // UTC
}


// set M5Tough's RTC from network time (UTC)
void SystemTime::setRTCFromNTP() {
    RTC_TimeTypeDef rtcTime;
    RTC_DateTypeDef rtcDate;
    tm *tminfo;

    time_t epoch = timeClient.getEpochTime();
    tminfo = gmtime(&epoch);
    rtcTime.Seconds = tminfo->tm_sec;
    rtcTime.Minutes = tminfo->tm_min;
    rtcTime.Hours = tminfo->tm_hour;
    rtcDate.Date = tminfo->tm_mday;
    rtcDate.Month = tminfo->tm_mon + 1;
    rtcDate.Year = tminfo->tm_year + 1900;
    rtcDate.WeekDay = tminfo->tm_wday;

    M5.Rtc.SetTime(&rtcTime);
    M5.Rtc.SetDate(&rtcDate);
    Serial.printf("RTC: set from network time to %4d/%.2d/%.2d %.2d:%.2d:%.2d (UTC)\n", rtcDate.Year,
        rtcDate.Month, rtcDate.Date, rtcTime.Hours, rtcTime.Minutes, rtcTime.Seconds);
}


// set system time from RTC to UTC
void SystemTime::setFromRTC() {
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

    // ugly work around for missing timegm()
    // https://github.com/esp8266/Arduino/issues/7806
    setenv("TZ", "GMT0", 1);
    tzset();
    epoch = mktime(&tm);
    setenv("TZ", LOCAL_TIMEZONE, 1);
    tzset();

    this->set(epoch);
    strftime(str, sizeof(str), "RTC: set system time to %Y/%m/%d %H:%M:%S (UTC)", gmtime(&epoch));
    Serial.println(str);
}


void SystemTime::ntpTask() {
    time_t lastRTCupdate = millis();
    time_t lastSysTimeUpdate = millis();
    bool ntpInit = false;
#ifdef MEMORY_DEBUG_INTERVAL_SECS
    uint16_t loopCounter = 0;
#endif

    while(true) {
        if (WiFi.isConnected()) {
            if (!ntpInit) {
                timeClient.begin();
                ntpInit = true;
            }
            // set RTC and system time to network time once every hour
            if (timeClient.update() && (tsDiff(lastRTCupdate) >= (3600 * 1000))) {  // timeout 1000ms
                lastRTCupdate = millis();
                this->set(timeClient.getEpochTime());
                this->setRTCFromNTP();
            }

        // if NTP sync fails, set system time from RTC once every hour
        } else if (!timeClient.isTimeSet() && (tsDiff(lastSysTimeUpdate) >= (3600 * 1000))) {
            lastSysTimeUpdate = millis();
            this->setFromRTC();
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


void SystemTime::ntpTaskWrapper(void* _this) {
    static_cast<SystemTime*>(_this)->ntpTask();
}


char* SystemTime::getTimeString() {
    static char strTime[9]; // HH:MM:SS
    time_t now;
    tm *tminfo;

    time(&now);
    tminfo = localtime(&now);
    if ((tminfo->tm_year+1900) >= 2023) {
        sprintf(strTime, "%.2d:%.2d:%.2d", tminfo->tm_hour, tminfo->tm_min, tminfo->tm_sec);
    } else {
        strncpy(strTime, "--:--:--", 8);
    }
    return strTime;
}


// returns ptr to array with current date
char* SystemTime::getDateString() {
    static char strDate[11]; // DD.MM.YYYY
    time_t now;
    tm *tminfo;

    time(&now);
    tminfo = localtime(&now);
    if ((tminfo->tm_year+1900) >= 2023) {
        sprintf(strDate, "%.2d.%.2d.%4d", tminfo->tm_mday,
            (tminfo->tm_mon + 1), (tminfo->tm_year + 1900));
    } else {
        strncpy(strDate, "--.--.----", 10);
    }
    return strDate;
}


// returns runtime in minutes
uint32_t SystemTime::getRuntimeMinutes() {
    static time_t lastMillis = 0;
    static float seconds = 0.0;

    seconds += tsDiff(lastMillis) / 1000.0;
    lastMillis = millis();
    return (uint32_t)seconds/60;
}


bool SystemTime::isTimeSet() {
    return (time(nullptr) > 1701388800);  // 12/01/2023
}


void SystemTime::begin() {
    uint8_t timeout = 0;

    M5.Lcd.clearDisplay(BLUE);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(20, 40);
    M5.Lcd.print("Get network time...");
    Serial.printf("NTP: sync time with server %s...", prefs.ntpServer);
    xTaskCreatePinnedToCore(this->ntpTaskWrapper, "ntpTask", 2560, this, 3, &this->ntpTaskHandle, 0);

   if (!WiFi.isConnected()) {
        M5.Lcd.print("no WiFi");
        Serial.println("aborting, no WiFi uplink");
        delay(1000);
        M5.Lcd.setCursor(20, 70);
        M5.Lcd.print("Set time from RTC...");
        this->setFromRTC();
        M5.Lcd.print("OK");
        delay(1500);
        return;
    }

    // wait for ntpTask()
    while (timeout++ < 20 && !timeClient.isTimeSet())
        delay(250);

    if (timeClient.isTimeSet()) {
        this->set(timeClient.getEpochTime());
        M5.Lcd.print("OK");
        Serial.println("OK");
        delay(1000);
        M5.Lcd.setCursor(20, 70);
        M5.Lcd.print("Set RTC from NTP...");
        this->setRTCFromNTP();
        M5.Lcd.print("OK");
    } else {
        M5.Lcd.print("ERR");
        Serial.println("ERROR");
        delay(1000);
        M5.Lcd.setCursor(20, 70);
        M5.Lcd.print("Set time from RTC...");
        this->setFromRTC();
        M5.Lcd.print("OK");
    }
    delay(1500);
}