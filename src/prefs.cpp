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

#include "prefs.h"
#include "config.h"
#include "mqtt.h"
#include "utils.h"
#include "rtc.h"
#include "wlan.h"

// use NVS to store settings to survive
// a system reset (cold start) or reflash
Preferences nvs;

// instantiate app settings and set default values
appPrefs_t prefs = {
    { 0 },
    READING_INTERVAL_SEC,
    MQTT_BROKER_HOST,
    MQTT_BROKER_PORT,
    MQTT_TOPIC,
    MQTT_PUBLISH_INTERVAL_SECS,
#if defined(MQTT_USERNAME) && defined(MQTT_PASSWORD)
    true,
    MQTT_USERNAME,
    MQTT_PASSWORD,
#else
    false,
    "",
    "",
#endif
    NTP_ADDRESS,
#ifdef CLEAR_NVS_ON_UPDATE
    true,
#else
    false,
#endif
    { 0 }
};

// check if a new firmware has just been flashed
static void checkFirmwareUpdate() {
    uint8_t sha256[32], sha256Prev[32];

    if (esp_partition_get_sha256(esp_ota_get_running_partition(), sha256) == ESP_OK) {
        Serial.print("Firmware SHA256: ");
        for (int i = 0; i < 32; i++) {
            Serial.print(sha256[i], HEX);
        }
        Serial.println();

        nvs.getBytes("sha256", sha256Prev, sizeof(sha256Prev));
        if (memcmp(sha256Prev, sha256, sizeof(sha256Prev))) {
            Serial.print("New firmware detected");
            if (prefs.clearNVSUpdate) {
                Serial.print(", clear NVS");
                nvs.clear();
                // remove WiFi credentials as well
                WiFi.enableSTA(true);
                WiFi.disconnect(true, true);
            }
            Serial.println();
            // save new firmware checksum
            nvs.putBytes("sha256", sha256, sizeof(sha256));
        }
    } else {
        Serial.println("Failed to read firmware's SHA256 checksum");
    }
}


// load system settings from NVS if previously saved
void startPrefs() {
    size_t prefSize;
    
    nvs.begin("prefs", false);
    checkFirmwareUpdate();
    if (nvs.getBool("saved")) {
        prefSize = nvs.getBytesLength("appPrefs");
        byte bufPrefs[prefSize];
        nvs.getBytes("appPrefs", bufPrefs, prefSize);
        memcpy(&prefs, bufPrefs, prefSize);
        Serial.println(F("Restored settings from NVS"));
    } else {
        Serial.println(F("Save compile time settings to NVS"));
        savePrefs(false);
    }
}


// save preferences to NVS
// do few sanity checks to avoid exceptions or non-working setups
void savePrefs(bool restart) {

    if (strlen(prefs.mqttUsername) <= 4 || strlen(prefs.mqttPassword) <= 6)
        prefs.mqttEnableAuth = false;

    nvs.putBytes("appPrefs", &prefs, sizeof(prefs));
    nvs.putBool("saved", true);
    if (restart) {
        Serial.println(F("Changed settings required ESP restart..."));
        nvs.end();
        ESP.restart();
    }
}