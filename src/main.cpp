/***************************************************************************
  Copyright (c) 2023-2024 Lars Wessels, Fraunhofer IOSB

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
    Serial.println("(c) 2023 Lars Wessels, Fraunhofer IOSB");
    Serial.printf("Firmware %s v%s\n", FIRMWARE_NAME, FIRMWARE_VERSION);
    Serial.printf("Compiled on %s, %s\n", __DATE__, __TIME__);

    // display warning message and enter deep sleep
    // if battery level <= BATTERY_SHUTDOWN_LEVEL
    lowBatteryCheck();

    // queue for status bar at bottom of LCD
    statusMsgQueue = xQueueCreate(STATUS_MESSAGE_QUEUE_SIZE, sizeof(StatusMsg_t));

    startWatchdog();
#ifdef DISPLAY_LOGO
    displayLogo();
#endif
    displaySplashScreen();
    startPrefs();

    Sensors::init();
    swipeRight.addHandler(confirmRestart, E_GESTURE);
    WifiUplink.begin();
    SysTime.begin();
    Publisher.begin();

    GATT.begin();
    displayPowerStatus(true);
    LoRaWAN.begin(&Serial2, LORAWAN_RX_PIN, LORAWAN_TX_PIN);
}


void loop() {
    static time_t lastReading = 0, lastMqttPublish = 0;

    M5.update();
    bme680.read(); // calculates readings every 3 seconds and calibrates sensors

    // read sensor data every READING_INTERVAL_SEC
    if (tsDiff(lastReading) > (prefs.readingsIntervalSecs * 1000)) {
        lastReading = millis();
        mlx90614.read();
        sfa30.read();
        displayPowerStatus(false);

        // display and publish sensor readings on significant changes
        // or if mqtt publishing interval has passed
        if (mlx90614.changed() || sfa30.changed() || bme680.changed() || Publisher.schedule()) {

            // display full screen warning message every BATTERY_LEVEL_INTERVAL_SECS
            // when battery level is below BATTERY_WARNING_LEVEL
            lowBatteryCheck();

            mlx90614.display();  // sets initial LCD screen layout
            mlx90614.console();

            sfa30.display();
            sfa30.console();

            bme680.display();
            bme680.console();

            updateStatusBar();

            // send off current sensor data (MQTT, BLE, LoRaWAN)
            GATT.notify(readings);
            Publisher.queue(readings);
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
