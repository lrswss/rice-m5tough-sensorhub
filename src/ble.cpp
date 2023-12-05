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
#include "ble.h"
#include "sensors.h"
#include "wlan.h"
#include "utils.h"
#include "rtc.h"
#include "prefs.h"

// Setup characteristics and descriptors for sensor readings
// use standard UUIDs for known characteristics
// https://www.bluetooth.com/specifications/assigned-numbers/
NimBLECharacteristic tempCharacteristic(BLEUUID((uint16_t)0x2A6E), BLE_PROPERTY_NOTIFY_READ);
NimBLEDescriptor tempDescriptor(BLEUUID((uint16_t)0x2901), NIMBLE_PROPERTY::READ, 32);

NimBLECharacteristic humCharacteristic(BLEUUID((uint16_t)0x2A6F), BLE_PROPERTY_NOTIFY_READ);
NimBLEDescriptor humDescriptor(BLEUUID((uint16_t)0x2901), NIMBLE_PROPERTY::READ, 32);

NimBLECharacteristic hchoCharacteristic(BLE_UUID_HCHO, BLE_PROPERTY_NOTIFY_READ);
NimBLEDescriptor hchoDescriptor(BLEUUID((uint16_t)0x2901), NIMBLE_PROPERTY::READ, 32);

NimBLECharacteristic eco2Characteristic(BLEUUID((uint16_t)0x2BE7), BLE_PROPERTY_NOTIFY_READ);
NimBLEDescriptor eco2Descriptor(BLEUUID((uint16_t)0x2901), NIMBLE_PROPERTY::READ, 32);

NimBLECharacteristic vocCharacteristic(BLEUUID((uint16_t)0x2B8C), BLE_PROPERTY_NOTIFY_READ);
NimBLEDescriptor vocDescriptor(BLEUUID((uint16_t)0x2901), NIMBLE_PROPERTY::READ, 32);

NimBLECharacteristic iaqCharacteristic(BLE_UUID_IAQ, BLE_PROPERTY_NOTIFY_READ);
NimBLEDescriptor iaqDescriptor(BLEUUID((uint16_t)0x2901), NIMBLE_PROPERTY::READ, 36);

NimBLECharacteristic elaspedTimeCharacteristic(BLE_ELAPSEDTIME_UUID, NIMBLE_PROPERTY::READ);
NimBLEDescriptor elaspedTimeDescriptor(BLEUUID((uint16_t)0x2901), NIMBLE_PROPERTY::READ, 32);

NimBLECharacteristic modelCharacteristic(BLE_MODELNUMBER_UUID, NIMBLE_PROPERTY::READ);
NimBLECharacteristic manufacturerCharacteristic(BLE_MANUFACTURER_UUID, NIMBLE_PROPERTY::READ);
NimBLECharacteristic firmwareCharacteristic(BLE_FIRMWAREREVISION_UUID, NIMBLE_PROPERTY::READ);

static bool BLEdeviceConnected = false;

// callbacks onConnect and onDisconnect
class deviceCallbacks: public NimBLEServerCallbacks {

    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc *desc) {
        char statusMsg[32];

        sprintf(statusMsg, "BLE %s", NimBLEAddress(desc->peer_ota_addr).toString().c_str());
        displayStatusMsg(statusMsg, 25, true, BLUE, WHITE);
        Serial.printf("BLE: device %s connected\n", NimBLEAddress(desc->peer_ota_addr).toString().c_str());
        BLEdeviceConnected = true;
    }

    void onDisconnect(NimBLEServer* pServer) {
        displayStatusMsg("BLE disconnected", 50, true, BLUE, WHITE);
        Serial.println("BLE: device disconnected");
        pServer->getAdvertising()->start(); // restart after disconnecting from client
        BLEdeviceConnected = false;
    }
};


void ble_init() {
    char bleServerName[32];

    if (!prefs.bleServer) {
        Serial.println("BLE: disabled");
        return;
    }

    snprintf(bleServerName, sizeof(bleServerName), "%s-%s", WIFI_PORTAL_SSID, getSystemID().c_str());
    NimBLEDevice::init(bleServerName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); // +9dbm

    M5.Lcd.clearDisplay(BLUE);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(20, 40);
    M5.Lcd.print("Starting BLE Server...");

    // BLE server and service setup
    NimBLEServer *pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new deviceCallbacks());
    NimBLEService *devInfoService = pServer->createService(BLE_DEVINFO_SERVICE_UUID);
    NimBLEService *environmentalService = pServer->createService(BLE_ENVIRONMENTAL_SERVICE_UUID);

    devInfoService->addCharacteristic(&modelCharacteristic);
    modelCharacteristic.setValue(FIRMWARE_NAME);
    devInfoService->addCharacteristic(&manufacturerCharacteristic);
    manufacturerCharacteristic.setValue(MANUFACTURER);
    devInfoService->addCharacteristic(&firmwareCharacteristic);
    firmwareCharacteristic.setValue(FIRMWARE_VERSION);
    devInfoService->addCharacteristic(&elaspedTimeCharacteristic);
    elaspedTimeCharacteristic.addDescriptor(&elaspedTimeDescriptor);
    elaspedTimeDescriptor.setValue("Total runtime (minutes)");

    if (mlx90614_status() || bme680_status()) {
        environmentalService->addCharacteristic(&tempCharacteristic);
        tempCharacteristic.addDescriptor(&tempDescriptor);
        if (mlx90614_status())
            tempDescriptor.setValue("MLX90614 IR thermometer (C)");
        else
            tempDescriptor.setValue("BME680 temperature sensor (C)");
    }
    if (bme680_status() || sfa30_status()) {
        environmentalService->addCharacteristic(&humCharacteristic);
        humCharacteristic.addDescriptor(&humDescriptor);
        if (bme680_status())
            humDescriptor.setValue("BME680 humidity sensor (%)");
        else
            humDescriptor.setValue("SFA30 humidity sensor (%)");
    }
    if (sfa30_status()) {
        environmentalService->addCharacteristic(&hchoCharacteristic);
        hchoCharacteristic.addDescriptor(&hchoDescriptor);
        hchoDescriptor.setValue("SFA30 formaldehyde sensor (ppb)");
    }
    if (bme680_status()) {
        environmentalService->addCharacteristic(&iaqCharacteristic);
        iaqCharacteristic.addDescriptor(&iaqDescriptor);
        iaqDescriptor.setValue("BME680 Air Quality Index (0-500)");
        environmentalService->addCharacteristic(&eco2Characteristic);
        eco2Characteristic.addDescriptor(&eco2Descriptor);
        eco2Descriptor.setValue("BME680 eCO2 estimation (ppm)");
        environmentalService->addCharacteristic(&vocCharacteristic);
        vocCharacteristic.addDescriptor(&vocDescriptor);
        vocDescriptor.setValue("BME680 VOC estimation (ppm)");
    }

    // Start the services
    devInfoService->start();
    environmentalService->start();

    // Start advertising
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // iPhone fix?
    pServer->getAdvertising()->start();
    M5.Lcd.print("OK");
    Serial.println("BLE: server ready, waiting for clients to connect");
    delay(1500);
}


void ble_notify() {
    uint16_t hum;
    uint32_t runtime = getRuntimeMinutes();

    if (!prefs.bleServer || !BLEdeviceConnected)
        return;

    elaspedTimeCharacteristic.setValue(runtime);
    if (mlx90614_status()) {
        tempCharacteristic.setValue(sensors.mlxObjectTemp);
        tempCharacteristic.notify();
    } else if (bme680_status()) {
        tempCharacteristic.setValue(sensors.bme680Temp);
        tempCharacteristic.notify(); 
    }
    delay(10);
    if (bme680_status() || sfa30_status()) {
        if (bme680_status())
            hum = sensors.bme680Hum;
        else
            hum = sensors.sfa30Hum;
        humCharacteristic.setValue(hum);
        humCharacteristic.notify();
        delay(10);
    }
    if (sfa30_status()) {
        hchoCharacteristic.setValue(sensors.sfa30HCHO);
        hchoCharacteristic.notify();
        delay(10);
    }
    if (bme680_status() == 2) {
        eco2Characteristic.setValue(sensors.bme680eCO2);
        eco2Characteristic.notify();
        delay(10);
        vocCharacteristic.setValue(sensors.bme680VOC);
        vocCharacteristic.notify();
        delay(10);
        iaqCharacteristic.setValue(sensors.bme680Iaq);
        iaqCharacteristic.notify();
    }
    Serial.println("BLE: sending sensor readings");
}