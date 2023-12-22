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

#include "sfa30.h"
#include "sensors.h"
#include "display.h"
#include "utils.h"


SFA30 sfa30;

// constructor for SFA30 formaldehyde sensor
SFA30::SFA30() {
    this->sfa = SensirionI2CSfa3x();
    this->ready = false;
    this->error = 0;
}


// initialize formaldehyde sensor SFA30 (I2C)
bool SFA30::setup() {
    char statusMsg[64];

    this->sfa.begin(Wire);
    this->error = this->sfa.startContinuousMeasurement();
    if (this->error != 0) {
        snprintf(statusMsg, sizeof(statusMsg), "SFA30 failed, error %d", this->error);
        displayStatusMsg(statusMsg, 30, false, WHITE, RED);
        errorToString(this->error, this->errormsg, 256);
        Serial.printf("SFA30: failed to initialize sensor, %s\n", this->errormsg);
        delay(3000);
        return false;
    } else {
        displayStatusMsg("Sensor SFA30 ready", 50, false, WHITE, DARKGREEN);
        Serial.println("SFA30: sensor ready, starting continuous measurement");
        this->ready = true;
        delay(1500);
        return true;
    }
}


uint8_t SFA30::status() {
    return (uint8_t)(this->ready && (this->error == 0));
}


// get currents readings from formaldehyde sensor SFA30
bool SFA30::read() {
    int16_t hcho = 0, hum = 0, temp = 0;

    if (!this->status())
        return false;
    this->error = this->sfa.readMeasuredValues(hcho, hum, temp);
    if (!this->error) {
        readings.sfa30HCHO = hcho / 5.0;
        readings.sfa30Hum = int(hum / 100.0);
        readings.sfa30Temp = temp / 200.0;
    }

    return !this->error;
}


// returns true if formaldehyde reading has changed significantly
bool SFA30::changed() {
    static float lastHCHO = 0.0;
    bool changed = false;

    if (!this->status())
        return false;
    if (abs(lastHCHO - readings.sfa30HCHO) >= HCHO_PUBLISH_TRESHOLD)
        changed = true;
    lastHCHO = readings.sfa30HCHO;

    return changed;
}


// display SFA30 readings on M5 Tought's OLED display
void SFA30::display() {
    if (!this->status()) {
        M5.Lcd.setCursor(175, 105);
        M5.Lcd.print("Humidity: ");
        if (bme680.status() > 0)
            M5.Lcd.print(readings.bme680Hum);
        else
            M5.Lcd.print("n/a");
        M5.Lcd.setCursor(15, 150);
        M5.Lcd.print("HCHO: n/a");
    } else {
        M5.Lcd.setCursor(175, 105);
        M5.Lcd.print("Humidity: ");
        if (bme680.status() > 0) // prefer BME680 humidity reading
            M5.Lcd.print(readings.bme680Hum);
        else
            M5.Lcd.print(readings.sfa30Hum);
        M5.Lcd.setCursor(15, 150);
        M5.Lcd.print("HCHO: ");
        M5.Lcd.print(readings.sfa30HCHO, 1);
    }
}


// print sensor status/readings on serial console
void SFA30::console() {
    Serial.print("SFA30: ");
    if (this->status()) {
        Serial.print("Formaldehyde(");
        Serial.print(readings.sfa30HCHO, 1);
        Serial.print(" ppb), Humidity(");
        Serial.print(readings.sfa30Hum);
        Serial.print(" %), Temperature(");
        Serial.print(readings.sfa30Temp, 1);
        Serial.println(" C)");
    } else {
        Serial.println("sensor not ready!");
    }
}