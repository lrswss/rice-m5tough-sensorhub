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
#include "wlan.h"
#include "utils.h"
#include "mqtt.h"
#include "rtc.h"
#include "sensors.h"
#include "prefs.h"
#include "ble.h"


void setup() {
    M5.begin(true, false, true, true, kMBusModeOutput);
    delay(1000);
    Serial.println("(c) 2023 Fraunhofer IOSB");
    Serial.printf("Firmware %s v%s\n", FIRMWARE_NAME, FIRMWARE_VERSION);
    Serial.printf("Compiled on %s, %s\n", __DATE__, __TIME__);

    startWatchdog();
    displayLogo();
    displaySplashScreen();
    startPrefs();

    mlx90614_init();
    sfa30_init();
    bme680_init();
    bme680_dialogResetBSEC();

    wifi_dialogStartPortal();
    ble_init();
    ntp_init();
    mqtt_init();
}


void loop() {
    static time_t lastReading = 0, lastTimeUpdate = 0, mqttRetry = 0, lastMqttPublish = 0;
    char statusMsg[128];

    bme680_read(); // calculates readings every 3 seconds and calibrates sensors

    // read sensor data every READING_INTERVAL_SEC
    if (millis() - lastReading > (READING_INTERVAL_SEC * 1000)) {
        lastReading = millis();
        mlx90614_read();
        sfa30_read();

        // display and publish sensor readings on significant changes
        // or if mqtt publishing interval has passed
        if (mlx90614_changed() || sfa30_changed() || bme680_changed() ||
            (millis() - lastMqttPublish) > (MQTT_PUBLISH_INTERVAL_SECS * 1000)) {

            mlx90614_display();
            Serial.print("MLX90614: ");
            if (mlx90614_status()) {
                Serial.print("objectTemperature(");
                Serial.print(sensors.mlxObjectTemp, 1);
                Serial.print(" C), ambientTemperature(");
                Serial.print(sensors.mlxAmbientTemp, 1);
                Serial.println(" C)");
            } else {
                Serial.println(" sensor not ready!");
            }

            sfa30_display();
            Serial.print("SFA30: ");
            if (sfa30_status()) {
                Serial.print("Formaldehyde(");
                Serial.print(sensors.sfa30HCHO, 1);
                Serial.print(" ppb), Humidity(");
                Serial.print(sensors.sfa30Hum);
                Serial.print(" %), Temperature(");
                Serial.print(sensors.sfa30Temp, 1);
                Serial.println(" C)");
            } else {
                Serial.println(" sensor not ready!");
            }

            bme680_display();
            Serial.print("BME680: ");
            if (bme680_status() > 0) {
                if (bme680_status() > 1) {
                    Serial.printf("IAQ(%d, %s), eCO2(%d ppm), VOC(", 
                        sensors.bme680Iaq, bme680_accuracy(), sensors.bme680eCO2);
                    Serial.print(sensors.bme680VOC, 1);
                    Serial.print(" ppm), ");
                } else {
                    Serial.print("gas sensor warmup, ");
                }
                Serial.printf("Gas(%d kOhm), Humdity(%d %%), Temperature(",
                    sensors.bme680GasResistance, sensors.bme680Hum);
                Serial.print(sensors.bme680Temp, 1);
                Serial.println(" C)");
            } else {
                Serial.println(" sensor not ready!");
            }

            ble_notify();

            // make sure Wifi is up before trying to publish data
            if (WiFi.status() != WL_CONNECTED) {
                displayStatusMsg("No WiFi connection", 65, false, WHITE, RED);

            } else if (!mqttRetry || millis() > mqttRetry) {
                displayStatusMsg("MQTT publish", 80, true, BLUE, WHITE);
                if (mqtt_publish()) {
                    lastMqttPublish = millis();
                    mqttRetry = 0;
                    delay(500);
                } else {
                    mqttRetry = millis() + (MQTT_RETRY_SECS * 1000);
                }
            }
      
            // display error messag in status line if MQTT failed
            if (mqttRetry > 0 && millis() < mqttRetry) {
                snprintf(statusMsg, sizeof(statusMsg), "MQTT failed (error %d)", mqtt_state());
                displayStatusMsg(statusMsg, 45, false, WHITE, RED);
            }
        }
    }

    // update Date/Time in status line on bottom of the screen
    if (!mqttRetry)
        displayDateTime();

    // check wifi connection and try to reconnect if down
    wifi_reconnect();

#ifdef MEMORY_DEBUG_INTERVAL_MIN
    memoryDebug();
#endif

    esp_task_wdt_reset(); // feed the dog...
}
