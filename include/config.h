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

#ifndef _CONFIG_H
#define _CONFIG_H

#define MQTT_PUBLISH_INTERVAL_SECS 15
#define MQTT_BROKER_HOST "192.168.10.1"
#define MQTT_BROKER_PORT 1883
#define MQTT_TOPIC  "m5tough/state"
//#define MQTT_USER "username"
//#define MQTT_PASS "password"

// uncomment to enable (optional) Bluetooh LE GATT server
//#define BLE_SERVER

#define SENSOR_READING_INTERVAL_SECS 5
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

// uncomment/set APPKEY/APPEUI to enable (optional) LoRaWAN serial adapter
// the devEUI is derived from WiFi MAC address, see ASR6501::getDevEUI()
//#define LORAWAN_APPKEY "CEB39FE91A24CB1C0717E2859D2459AA"
//#define LORAWAN_APPEUI "0000000000000000"
#define LORAWAN_INTERVAL_SECS 60
#define LORAWAN_CONFIRM false
#define LORAWAN_TX_PIN 14
#define LORAWAN_RX_PIN 13

#define MANUFACTURER "Fraunhofer IOSB"
#define FIRMWARE_NAME "RICE-M5Tough-SensorHub"
#define FIRMWARE_VERSION "1.7.1"

// uncomment to display Logo at startup
// requires RGB565 dump as 'const unsigned short logo[]' in logo.h
#define DISPLAY_LOGO

// set your timezone and NTP server
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define LOCAL_TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_SERVER_ADDRESS "de.pool.ntp.org"

// clear NVS (and WiFi credentials) if new firmware is detected
#define CLEAR_NVS_ON_UPDATE
//#define CLEAR_WIFI_ON_UPDATE

#endif
