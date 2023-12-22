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


// rollover safe comparison for given timestamp with millis()
time_t tsDiff(time_t tsMillis) {
    if ((millis() - tsMillis) < 0)
        return (LONG_MAX - tsMillis + 1);
    else
        return (millis() - tsMillis);
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
        Serial.printf("DEBUG[%s]: runtime %d min, FreeHeap %d bytes\n",
            getTimeString(), getRuntimeMinutes(), ESP.getFreeHeap());
        lastMsgMillis = millis();
    }
}

UBaseType_t printFreeStackWatermark(const char *taskName) {
    UBaseType_t wm = uxTaskGetStackHighWaterMark(NULL);
    Serial.printf("DEBUG[%s]: %s, runtime %d min, Core %d, StackHighWaterMark %d bytes\n",
        getTimeString(), taskName, getRuntimeMinutes(), xPortGetCoreID(), wm);
    return wm;
}
#endif