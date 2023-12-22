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
#include "lorawan.h"
#include "display.h"


void setup() {
    M5.begin(true, false, true, true, kMBusModeOutput);
    delay(1000);
    Serial.println("(c) 2023 Fraunhofer IOSB");
    Serial.printf("Firmware %s v%s\n", FIRMWARE_NAME, FIRMWARE_VERSION);
    Serial.printf("Compiled on %s, %s\n", __DATE__, __TIME__);

    // queue for status bar at bottom of LCD
    statusMsgQueue = xQueueCreate(STATUS_MESSAGE_QUEUE_SIZE, sizeof(StatusMsg_t));

    startWatchdog();
#ifdef DISPLAY_LOGO
    displayLogo();
#endif
    displaySplashScreen();
    startPrefs();

    Sensors::init();
    wifi_init();
    ntp_init();
    mqtt_init();

    ble_init();
    LoRaWAN.begin(&Serial2, LORAWAN_RX_PIN, LORAWAN_TX_PIN);
}


void loop() {
    static time_t lastReading = 0, lastMqttPublish = 0;

    bme680.read(); // calculates readings every 3 seconds and calibrates sensors

    // read sensor data every READING_INTERVAL_SEC
    if (tsDiff(lastReading) > (SENSOR_READING_INTERVAL_SEC * 1000)) {
        lastReading = millis();
        mlx90614.read();
        sfa30.read();

        // display and publish sensor readings on significant changes
        // or if mqtt publishing interval has passed
        if (mlx90614.changed() || sfa30.changed() || bme680.changed() ||
            tsDiff(lastMqttPublish) > (MQTT_PUBLISH_INTERVAL_SECS * 1000)) {

            mlx90614.display();  // sets initial LCD screen layout
            mlx90614.console();

            sfa30.display();
            sfa30.console();

            bme680.display();
            bme680.console();

            updateStatusBar();

            if (mqtt_publish(readings))
                lastMqttPublish = millis();
            ble_notify(readings);
            LoRaWAN.queue(readings);
        }
    }

    // update Date/Time in status line on bottom of the screen
    updateStatusBar();

#ifdef MEMORY_DEBUG_INTERVAL_SECS
    printFreeHeap();
#endif

    esp_task_wdt_reset(); // feed the dog...
}
