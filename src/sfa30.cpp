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

// SFA30 (Formaldehyd)
static SensirionI2CSfa3x SFA30;
static bool sfa30_ready = false;
static uint16_t sfa30_error = 0;
static char sfa30_errormsg[256];


// initialize formaldehyde sensor SFA30 (I2C)
bool sfa30_init() {
    char statusMsg[64];

    SFA30.begin(Wire);
    sfa30_error = SFA30.startContinuousMeasurement();
    if (sfa30_error != 0) {
        snprintf(statusMsg, sizeof(statusMsg), "SFA30 failed, error %d", sfa30_error);
        displayStatusMsg(statusMsg, 30, false, WHITE, RED);
        errorToString(sfa30_error, sfa30_errormsg, 256);
        Serial.printf("SFA30: failed to initialize sensor, %s\n", sfa30_errormsg);
        delay(3000);
        return false;
    } else {
        displayStatusMsg("Sensor SFA30 ready", 50, false, WHITE, DARKGREEN);
        Serial.println("SFA30: sensor ready, starting continuous measurement");
        sfa30_ready = true;
        delay(1500);
        return true;
    }
}


bool sfa30_status() {
    return sfa30_ready && (sfa30_error == 0);
}


// get currents readings from formaldehyde sensor SFA30
bool sfa30_read() {
    int16_t hcho = 0, hum = 0, temp = 0;

    if (!sfa30_status())
        return false;
    sfa30_error = SFA30.readMeasuredValues(hcho, hum, temp);
    if (!sfa30_error) {
        sensors.sfa30HCHO = hcho / 5.0;
        sensors.sfa30Hum = int(hum / 100.0);
        sensors.sfa30Temp = temp / 200.0;
    }

    return !sfa30_error;
}


// returns true if formaldehyde reading has changed significantly
bool sfa30_changed() {
    static float lastHCHO = 0.0;
    bool changed = false;

    if (!sfa30_status())
        return false;
    if (abs(lastHCHO - sensors.sfa30HCHO) >= HCHO_PUBLISH_TRESHOLD)
        changed = true;
    lastHCHO = sensors.sfa30HCHO;

    return changed;
}


// display SFA30 readings on M5 Tought's OLED display
void sfa30_display() {
    if (!sfa30_status()) {
        M5.Lcd.setCursor(175, 105);
        M5.Lcd.print("Humidity: ");
        if (bme680_status() > 0)
            M5.Lcd.print(sensors.bme680Hum);
        else
            M5.Lcd.print("n/a");
        M5.Lcd.setCursor(15, 150);
        M5.Lcd.print("HCHO: n/a");
    } else {
        M5.Lcd.setCursor(175, 105);
        M5.Lcd.print("Humidity: ");
        if (bme680_status() > 0) // prefer BME680 humidity reading
            M5.Lcd.print(sensors.bme680Hum);
        else
            M5.Lcd.print(sensors.sfa30Hum);
        M5.Lcd.setCursor(15, 150);
        M5.Lcd.print("HCHO: ");
        M5.Lcd.print(sensors.sfa30HCHO, 1);
    }
}


// print sensor status/readings on serial console
void sfa30_console() {
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
        Serial.println("sensor not ready!");
    }
}