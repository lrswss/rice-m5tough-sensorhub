/***************************************************************************
  Copyright (c) 2023-2024 Lars Wessels

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

#ifndef _BLE_H
#define _BLE_H

#include <NimBLEDevice.h>
#include "bme680.h"
#include "sfa30.h"
#include "mlx90614.h"

// UUIDs for GATT services
// https://www.bluetooth.com/specifications/assigned-numbers/
#define BLE_ENVIRONMENTAL_SERVICE_UUID (BLEUUID((uint16_t)0x181A))
#define BLE_TEMPERATURE_UUID (BLEUUID((uint16_t)0x2A6E))
#define BLE_HUMIDITY_UUID (BLEUUID((uint16_t)0x2A6F))
#define BLE_ECO2_UUID (BLEUUID((uint16_t)0x2B8C))
#define BLE_VOC_UUID (BLEUUID((uint16_t)0x2BE7))
#define BLE_DEVINFO_SERVICE_UUID (BLEUUID((uint16_t)0x180A))
#define BLE_MANUFACTURER_UUID (BLEUUID((uint16_t)0x2A29))
#define BLE_FIRMWAREREVISION_UUID (BLEUUID((uint16_t)0x2A26))
#define BLE_HARDWAREREVISION_UUID (BLEUUID((uint16_t)0x2A27))
#define BLE_MODELNUMBER_UUID (BLEUUID((uint16_t)0x2A24))
#define BLE_ELAPSEDTIME_UUID (BLEUUID((uint16_t)0x2BF2))
#define BLE_BATLEVEL_UUID (BLEUUID((uint16_t)0x2A19))

// custom UUIDs for characteristic missing in above assigned numbers specs
#define BLE_IAQ_UUID "3118ab5a-c9e6-48d1-91c2-3ca1652a61c6"  // Air Quality Index
#define BLE_HCHO_UUID "f202b62d-3794-4388-987c-509780b18326"  // Formaldehyde (ppb)

#define BLE_USER_DESC_UUID (BLEUUID((uint16_t)0x2901))
#define BLE_PROP_NOTIFY_READ (NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ)
#define BLE_PROP_READ NIMBLE_PROPERTY::READ


class GATTServer {
    public:
        GATTServer();
        bool begin();
        void notify(sensorReadings_t data);
        ~GATTServer();
    private:
        char bleServerName[32];
        NimBLEServer *pServer;
        NimBLEService *devInfoService;
        NimBLEService *environmentalService;
        NimBLEAdvertising *pAdvertising;
        NimBLECharacteristic *tempCharacteristic;
        NimBLECharacteristic *humCharacteristic;
        NimBLECharacteristic *hchoCharacteristic;
        NimBLECharacteristic *iaqCharacteristic;
        NimBLECharacteristic *eco2Characteristic;
        NimBLECharacteristic *vocCharacteristic;
        NimBLECharacteristic *modelCharacteristic;
        NimBLECharacteristic *manufacturerCharacteristic;
        NimBLECharacteristic *firmwareCharacteristic;
        NimBLECharacteristic *elaspedTimeCharacteristic;
        NimBLECharacteristic *batteryCharacteristic;
        class deviceCallbacks;
};

extern GATTServer GATT;
#endif