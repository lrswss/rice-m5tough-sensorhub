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
#include "display.h"


// gesture to restart ESP
Gesture swipeRight("right", 100, DIR_RIGHT, 50, true);
static bool endButtonWaitLoop = false;
static bool restartESP = false;
bool blockScreen = false;
RTC_DATA_ATTR bool lowBattery = false;


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


// display usb power status and battery info (if availabl) at startup
// and every BATTERY_LEVEL_INTERVAL_SECS in status message bar
void displayPowerStatus(bool fullScreen) {
    static time_t lastCheck = 0;
    static char statusMsg[32], vbat[4];
    uint16_t displayInterval = BATTERY_LEVEL_INTERVAL_SECS;
    float batLevel = 0.0;

    if (M5.Axp.GetBatVoltage() < 1.0)
        return;

    dtostrf(M5.Axp.GetBatVoltage(), 4, 2, vbat);
    batLevel = M5.Axp.GetBatteryLevel();

    if (batLevel <= BATTERY_WARNING_LEVEL)
        displayInterval = 30;

    if (fullScreen) { // shows battery status at startup
        M5.Lcd.clearDisplay(BLUE);
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.setFreeFont(&FreeSans12pt7b);
        M5.Lcd.setCursor(15, 40);
        M5.Lcd.printf("USB powered: %s", usbPowered() ? "Yes" : "No");
        M5.Lcd.setCursor(15, 70);
        M5.Lcd.printf("Internal Battery: %d%%", int(batLevel));
        M5.Lcd.setCursor(15, 100);
        M5.Lcd.printf("Battery Voltage: %sV", vbat);
        M5.Lcd.setCursor(15, 130);
        M5.Lcd.printf("Battery Charging: %s", M5.Axp.isCharging() ? "Yes" : "No");
        Serial.printf("BAT: %sV (%d%%), ", vbat, int(batLevel));
        Serial.printf("%scharging\n", M5.Axp.isCharging() ? "" : "not ");
        delay(2500);
    } else if (tsDiff(lastCheck) > (displayInterval * 1000)) {
        lastCheck = millis();
        Serial.printf("BAT: %sV (%d%%), ", vbat, int(batLevel));
        Serial.printf("%scharging\n", M5.Axp.isCharging() ? "" : "not ");
        // show battery level in status bar if currently charging or not USB powered
        if (M5.Axp.isCharging() || !usbPowered()) {
            snprintf(statusMsg, sizeof(statusMsg), "Battery %d%%", int(batLevel));
            if (batLevel >= BATTERY_WARNING_LEVEL)
                queueStatusMsg(statusMsg, 95, false);
            else
                queueStatusMsg(statusMsg, 95, true);
        }
    }
}


// button event handler
static void eventRestartESP(Event& e) {
    endButtonWaitLoop = true;
    restartESP = !strcmp(e.objName(), "Yes") ? true : false;
}


// show yes/no dialog to confirm restart triggered by swipe gesture
void confirmRestart(Event &e) {
    uint16_t timeout = 0;

    ButtonColors onColor = {RED, WHITE, WHITE};
    ButtonColors offColor = {DARKGREEN, WHITE, WHITE};
    Button bYes(35, 110, 120, 60, false, "Yes", offColor, onColor, MC_DATUM);
    Button bNo(165, 110, 120, 60, false, "No", offColor, onColor, MC_DATUM);

    blockScreen = true;
    M5.Lcd.clearDisplay(WHITE);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(50, 80);
    M5.Lcd.print("Restart Sensor Hub?");

    M5.Buttons.draw();
    bYes.addHandler(eventRestartESP, E_RELEASE);
    bNo.addHandler(eventRestartESP, E_RELEASE);
    while (timeout++ < (DISPLAY_DIALOG_TIMEOUT_SECS * 1000) && !endButtonWaitLoop) {
        M5.update();
        delay(1);
    }
    if (endButtonWaitLoop && restartESP) {
        M5.Lcd.clearDisplay(BLUE);
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.setFreeFont(&FreeSans12pt7b);
        M5.Lcd.setCursor(20,40);
        M5.Lcd.print("Restarting Sensor Hub...");
        savePrefs(true);
    }
    blockScreen = false;
}


// returns true if M5Tough is powered over USB
bool usbPowered() {
    return M5.Axp.GetVinVoltage() > 3.5 ? true : false;
}


// if battery level is below BATTERY_LOW_LEVEL display full
// screen warning message every BATTERY_LEVEL_INTERVAL_SECS secs
// enter deep sleep if level falls below BATTERY_SHUTDOWN_LEVEL
// set/unset global variable 'lowBattery' which is kept in RTC memory
void lowBatteryCheck() {
    static time_t lastWarning = 0;
    char strBatLevel[8];

    if ((lastWarning > 0) && (tsDiff(lastWarning) < (BATTERY_LEVEL_INTERVAL_SECS * 1000)))
        return;

    if (usbPowered()) { // skip warning if connected to USB power
        lowBattery = false;

    // RTC variable lowBattery will trigger warning message after deep
    // sleep even if battery level has recovered above BATTERY_LOW_LEVEL
    } else if (lowBattery || (M5.Axp.GetBatteryLevel() <= BATTERY_LOW_LEVEL)) {
        lastWarning = millis();
        blockScreen = true; // blocks status messages at the bottom of the screen
        M5.Lcd.clearDisplay(RED);
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.setFreeFont(&FreeSansBold24pt7b);
        M5.Lcd.setCursor(25, 80);
        M5.Lcd.print("Low Battery");
        dtostrf(M5.Axp.GetBatteryLevel(), 3, 0, strBatLevel);
        M5.Lcd.setCursor(110, 140);
        M5.Lcd.printf("%s %%", strBatLevel);
        if (lowBattery || (M5.Axp.GetBatteryLevel() <= BATTERY_SHUTDOWN_LEVEL)) {
            // lowBattery flag terminates background tasks and 
            // survives deep sleep (battery level hysteresis)
            lowBattery = true;
            M5.Lcd.setCursor(30, 190);
            M5.Lcd.setFreeFont(&FreeSans12pt7b);
            M5.Lcd.printf("Power down for %d secs...", BATTERY_LOW_DEEPSLEEP_SECS);
            delay(5000);
            M5.Lcd.clear();
            M5.Axp.DeepSleep(BATTERY_LOW_DEEPSLEEP_SECS * 1000 * 1000);
        }
        delay(3000);
        blockScreen = false;
    }
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