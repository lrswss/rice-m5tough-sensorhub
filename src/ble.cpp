/***************************************************************************
  Copyright (c) 2023 Lars Wessels

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
#include "ble.h"
#include "wlan.h"
#include "utils.h"
#include "rtc.h"
#include "prefs.h"
#include "display.h"


GATTServer GATT;

GATTServer::GATTServer() {
    snprintf(this->bleServerName, sizeof(this->bleServerName),
        "%s-%s", WIFI_PORTAL_SSID, getSystemID().c_str());
}


GATTServer::~GATTServer() {
    this->pServer->stopAdvertising();
    this->pServer->removeService(this->environmentalService);
    this->environmentalService->~NimBLEService();
    this->pServer->removeService(this->devInfoService);
    this->devInfoService->~NimBLEService();
    NimBLEDevice::deinit();
}


// callbacks onConnect and onDisconnect
class GATTServer::deviceCallbacks : public NimBLEServerCallbacks {

    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc *desc) {
        char statusMsg[32];

        Serial.printf("BLE: device %s connected\n", NimBLEAddress(desc->peer_ota_addr).toString().c_str());
        sprintf(statusMsg, "BLE %s", NimBLEAddress(desc->peer_ota_addr).toString().c_str());
        queueStatusMsg(statusMsg, 25, false);
    }

    void onDisconnect(NimBLEServer* pServer) {
        Serial.println("BLE: device disconnected");
        queueStatusMsg("BLE disconnected", 50, false);
        pServer->getAdvertising()->start(); // restart after disconnecting from client
    }
};


void GATTServer::begin() {
    NimBLEDescriptor *desc;

    if (!prefs.bleServer) {
        Serial.println("BLE: disabled");
        return;
    }

    M5.Lcd.clearDisplay(BLUE);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(20, 40);
    M5.Lcd.print("Starting BLE Server...");

    NimBLEDevice::init(this->bleServerName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); // +9dbm

    // BLE server and service setup
    this->pServer = NimBLEDevice::createServer();
    this->pServer->setCallbacks(new GATTServer::deviceCallbacks());

    // Setup characteristics and descriptors for device info and sensor readings
    // use standard UUIDs for known characteristics
    // https://www.bluetooth.com/specifications/assigned-numbers/
    this->devInfoService = this->pServer->createService(BLE_DEVINFO_SERVICE_UUID);
    this->modelCharacteristic = this->devInfoService->createCharacteristic(BLE_MODELNUMBER_UUID, BLE_PROP_READ);
    this->modelCharacteristic->setValue(FIRMWARE_NAME);
    this->manufacturerCharacteristic = this->devInfoService->createCharacteristic(BLE_MANUFACTURER_UUID, BLE_PROP_READ);
    this->manufacturerCharacteristic->setValue(MANUFACTURER);
    this->firmwareCharacteristic = this->devInfoService->createCharacteristic(BLE_FIRMWAREREVISION_UUID, BLE_PROP_READ);
    this->firmwareCharacteristic->setValue(FIRMWARE_VERSION);
    this->elaspedTimeCharacteristic = this->devInfoService->createCharacteristic(BLE_ELAPSEDTIME_UUID, BLE_PROP_READ);
    desc = this->elaspedTimeCharacteristic->createDescriptor(BLE_USER_DESC_UUID, BLE_PROP_READ, 32);
    desc->setValue("Total runtime (minutes)");
    if (M5.Axp.GetBatVoltage() >= 1.0) {
        this->batteryCharacteristic = this->devInfoService->createCharacteristic(BLE_BATLEVEL_UUID, BLE_PROP_READ);
        this->batteryCharacteristic->setValue(int(M5.Axp.GetBatteryLevel()));
    }

    // environmental service
    this->environmentalService = this->pServer->createService(BLE_ENVIRONMENTAL_SERVICE_UUID);
    if (mlx90614.status() || bme680.status()) {
        this->tempCharacteristic = this->environmentalService->createCharacteristic(BLE_TEMPERATURE_UUID, BLE_PROP_NOTIFY_READ);
        desc = this->tempCharacteristic->createDescriptor(BLE_USER_DESC_UUID, BLE_PROP_READ, 32);
        if (mlx90614.status())
            desc->setValue("MLX90614 IR thermometer (C)");
        else
            desc->setValue("BME680 temperature sensor (C)");
    }
    if (bme680.status() || sfa30.status()) {
        this->humCharacteristic = this->environmentalService->createCharacteristic(BLE_HUMIDITY_UUID, BLE_PROP_NOTIFY_READ);
        desc = this->humCharacteristic->createDescriptor(BLE_USER_DESC_UUID, BLE_PROP_READ, 32);
        if (bme680.status())
            desc->setValue("BME680 humidity sensor (%)");
        else
            desc->setValue("SFA30 humidity sensor (%)");
    }
    if (sfa30.status()) {
        this->hchoCharacteristic = this->environmentalService->createCharacteristic(BLE_HCHO_UUID, BLE_PROP_NOTIFY_READ);
        desc = this->hchoCharacteristic->createDescriptor(BLE_USER_DESC_UUID, BLE_PROP_READ, 32);
        desc->setValue("SFA30 formaldehyde sensor (ppb)");
    }
    if (bme680.status()) {
        this->iaqCharacteristic = this->environmentalService->createCharacteristic(BLE_IAQ_UUID, BLE_PROP_NOTIFY_READ);
        desc = this->iaqCharacteristic->createDescriptor(BLE_USER_DESC_UUID, BLE_PROP_READ, 36);
        desc->setValue("BME680 Air Quality Index (0-500)");
        this->eco2Characteristic = this->environmentalService->createCharacteristic(BLE_ECO2_UUID, BLE_PROP_NOTIFY_READ);
        desc = this->eco2Characteristic->createDescriptor(BLE_USER_DESC_UUID, BLE_PROP_READ, 32);
        desc->setValue("BME680 eCO2 estimation (ppm)");
        this->vocCharacteristic = this->environmentalService->createCharacteristic(BLE_VOC_UUID, BLE_PROP_NOTIFY_READ);
        desc = this->vocCharacteristic->createDescriptor(BLE_USER_DESC_UUID, BLE_PROP_READ, 32);
        desc->setValue("BME680 VOC estimation (ppm)");
    }

    // Start the services
    this->devInfoService->start();
    this->environmentalService->start();

    // Start advertising
    this->pAdvertising = NimBLEDevice::getAdvertising();
    this->pAdvertising->setScanResponse(true);
    this->pAdvertising->setMinPreferred(0x06); // iPhone fix?
    this->pServer->getAdvertising()->start();
    M5.Lcd.print("OK");
    Serial.println("BLE: GATT server ready, waiting for clients to connect");
    delay(1500);
}


void GATTServer::notify(sensorReadings_t data) {
    uint16_t hum;
    uint32_t runtime = SysTime.getRuntimeMinutes();

    if (!prefs.bleServer || !this->pServer->getConnectedCount())
        return;

    this->elaspedTimeCharacteristic->setValue(runtime);
    if (mlx90614.status()) {
        this->tempCharacteristic->setValue(data.mlxObjectTemp);
        this->tempCharacteristic->notify();
    } else if (bme680.status()) {
        this->tempCharacteristic->setValue(data.bme680Temp);
        this->tempCharacteristic->notify();
    }
    delay(10);
    if (bme680.status()) {
        this->humCharacteristic->setValue(data.bme680Hum);
        this->humCharacteristic->notify();
    } else if (sfa30.status()) {
        this->humCharacteristic->setValue(data.sfa30Hum);
        this->humCharacteristic->notify();
    }
    delay(10);
    if (sfa30.status()) {
        this->hchoCharacteristic->setValue(data.sfa30HCHO);
        this->hchoCharacteristic->notify();
        delay(10);
    }
    if (bme680.status() == 2) {
        this->eco2Characteristic->setValue(data.bme680eCO2);
        this->eco2Characteristic->notify();
        delay(10);
        this->vocCharacteristic->setValue(data.bme680VOC);
        this->vocCharacteristic->notify();
        delay(10);
        this->iaqCharacteristic->setValue(data.bme680Iaq);
        this->iaqCharacteristic->notify();
    }
    Serial.println("BLE: sending sensor readings");
}