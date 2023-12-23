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

#include "mlx90614.h"
#include "sensors.h"
#include "display.h"
#include "utils.h"


MLX90614 mlx90614;  // create instance

// constructor for MLX90614 sensor
MLX90614::MLX90614() {
    this->mlx = Adafruit_MLX90614();
    this->ready = false;
    this->error = true;
}


// initialize MLX90614 IR temperature sensor on I2C bus
bool MLX90614::setup() {
    if (!this->mlx.begin()) {
        displayStatusMsg("MLX90614 failed", 70, false, WHITE, RED);
        Serial.println("MLX90614: failed to detect sensor");
        delay(3000);
        return false;
    } else {
        displayStatusMsg("Sensor MLX90614 ready", 30, false, WHITE, DARKGREEN);
        Serial.print("MLX90614: sensor ready, emissivity ");
        Serial.println(this->mlx.readEmissivity());
        this->ready = true;
        this->read(); // set mlx96014_error
        delay(1500);
        return true;
    }
}


// returns true if MLX90614 sensor is ready to send readings
uint8_t MLX90614::status() {
    return (uint8_t)(this->ready && !this->error);
}


// get current readings from MLX90614 IR temperature sensor
bool MLX90614::read() {
    if (!this->ready)
        return false;
    readings.mlxObjectTemp = int(this->mlx.readObjectTempC() * 10) / 10.0;
    readings.mlxAmbientTemp = int(this->mlx.readAmbientTempC() * 10) / 10.0;
    if (readings.mlxObjectTemp == NAN)
        this->error = true;
    else if (readings.mlxObjectTemp == NAN)
        this->error = true;
    else
        this->error = false;
    return !this->error;
}


// returns true if temperature of object in focus has changed significantly
bool MLX90614::changed() {
    static float lastObjTemp = 0.0;
    bool changed = false;

    if (!this->status())
        return false;
    if (abs(lastObjTemp - readings.mlxObjectTemp) >= TEMP_PUBLISH_THRESHOLD)
        changed = true;
    lastObjTemp = readings.mlxObjectTemp;

    return changed;
}


// display MLX90614 readings on M5 Tough's OLED display
void MLX90614::display() {
    uint16_t color = BLUE;

    if (readings.mlxObjectTemp <= TEMP_THRESHOLD_CYAN)
        color = NAVY;
    else if (readings.mlxObjectTemp >= TEMP_THRESHOLD_RED)
        color = RED;
    else if (readings.mlxObjectTemp >= TEMP_THRESHOLD_ORANGE)
        color = ORANGE;
    else if (readings.mlxObjectTemp >= TEMP_THRESHOLD_GREEN)
        color = DARKGREEN;
    else if (readings.mlxObjectTemp >= TEMP_THRESHOLD_CYAN)
        color = DARKCYAN;

    M5.Lcd.fillRect(0, 0, 320, 205, color);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setFreeFont(&FreeSansBold36pt7b);
    M5.Lcd.setCursor(65, 70);
    if (this->status()) {
        M5.Lcd.print(readings.mlxObjectTemp, 1);
    } else {
        M5.Lcd.setCursor(85, 70);
        M5.Lcd.print("n/a");
    }
    printDegree(color);
    M5.Lcd.print(" C");
  
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(15, 105);
    M5.Lcd.print("Ambiant: ");
    if (!bme680.status()) { // prefer BME680 for ambient temperature reading
        if (this->status())
            M5.Lcd.print(readings.mlxAmbientTemp, 1);
        else
            M5.Lcd.print("n/a");
    } else {
        M5.Lcd.print(readings.bme680Temp, 1);
    }
}


// print sensor status/readings on serial console
void MLX90614::console() {
    Serial.print("MLX90614: ");
    if (this->status()) {
        Serial.print("objectTemperature(");
        Serial.print(readings.mlxObjectTemp, 1);
        Serial.print(" C), ambientTemperature(");
        Serial.print(readings.mlxAmbientTemp, 1);
        Serial.println(" C)");
    } else {
        Serial.println("sensor not ready!");
    }
}