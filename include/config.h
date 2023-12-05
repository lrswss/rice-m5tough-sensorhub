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

#ifndef _CONFIG_H
#define _CONFIG_H

#define MQTT_PUBLISH_INTERVAL_SECS 15
#define MQTT_BROKER_HOST "192.168.10.1"
#define MQTT_BROKER_PORT 1883
#define MQTT_TOPIC  "m5tough/state"
//#define MQTT_USER "username"
//#define MQTT_PASS "password"

// uncomment to enable BLE GATT server
#define BLE_SERVER

#define SENSOR_READING_INTERVAL_SEC 5
#define DISPLAY_DIALOG_TIMEOUT_SECS 5

#define HCHO_PUBLISH_TRESHOLD 1.0
#define TEMP_PUBLISH_THRESHOLD 0.2
#define HUM_PUBLISH_THRESHOLD 1
#define GASRESISTANCE_PUBLISH_THRESHOLD 5  // kOhms

// MLX90614 IR thermometer
#define TEMP_THRESHOLD_RED 30.0
#define TEMP_THRESHOLD_ORANGE 25.0
#define TEMP_THRESHOLD_GREEN 20.0
#define TEMP_THRESHOLD_CYAN 15.0

#define MANUFACTURER "Fraunhofer IOSB"
#define FIRMWARE_NAME "RICE-M5Tough-SensorHub"
#define FIRMWARE_VERSION "1.3b2"

// clear NVS (and WiFi credentials) if new firmware is detected
#define CLEAR_NVS_ON_UPDATE
//#define CLEAR_WIFI_ON_UPDATE


#endif