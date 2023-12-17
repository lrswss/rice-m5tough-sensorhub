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

// IR temparature sensor
static Adafruit_MLX90614 mlx = Adafruit_MLX90614();
static bool mlx90614_ready = false;
static bool mlx90614_error = true;


// initialize MLX90614 IR temperature sensor on I2C bus
bool mlx90614_init() {
    if (!mlx.begin()) {
        displayStatusMsg("MLX90614 failed", 55, false, WHITE, RED);
        Serial.println("MLX90614: failed to detect sensor");
        delay(3000);
        return false;
    } else {
        displayStatusMsg("Sensor MLX90614 ready", 30, false, WHITE, DARKGREEN);
        Serial.print("MLX90614: sensor ready, emissivity ");
        Serial.println(mlx.readEmissivity());
        mlx90614_ready = true;
        mlx90614_read(); // set mlx96014_error
        delay(1500);
        return true;
    }
}


bool mlx90614_status() {
    return mlx90614_ready && !mlx90614_error;
}


// get current readings from MLX90614 IR temperature sensor
bool mlx90614_read() {
    if (!mlx90614_ready)
        return false;
    sensors.mlxObjectTemp = int(mlx.readObjectTempC() * 10) / 10.0;
    sensors.mlxAmbientTemp = int(mlx.readAmbientTempC() * 10) / 10.0;
    if (sensors.mlxObjectTemp == NAN)
        mlx90614_error = true;
    else if (sensors.mlxObjectTemp == NAN)
        mlx90614_error = true;
    else
        mlx90614_error = false;
    return !mlx90614_error;
}


// returns true if temperature of object in focus has changed significantly
bool mlx90614_changed() {
    static float lastObjTemp = 0.0;
    bool changed = false;

    if (!mlx90614_status())
        return false;
    if (abs(lastObjTemp - sensors.mlxObjectTemp) >= TEMP_PUBLISH_THRESHOLD)
        changed = true;
    lastObjTemp = sensors.mlxObjectTemp;

    return changed;
}


// display MLX90614 readings on M5 Tough's OLED display
void mlx90614_display() {
    uint16_t color = BLUE;

    if (sensors.mlxObjectTemp <= TEMP_THRESHOLD_CYAN)
        color = NAVY;
    else if (sensors.mlxObjectTemp >= TEMP_THRESHOLD_RED)
        color = RED;
    else if (sensors.mlxObjectTemp >= TEMP_THRESHOLD_ORANGE)
        color = ORANGE;
    else if (sensors.mlxObjectTemp >= TEMP_THRESHOLD_GREEN)
        color = DARKGREEN;
    else if (sensors.mlxObjectTemp >= TEMP_THRESHOLD_CYAN)
        color = DARKCYAN;

    M5.Lcd.fillRect(0, 0, 320, 205, color);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setFreeFont(&FreeSansBold36pt7b);
    M5.Lcd.setCursor(65, 70);
    if (mlx90614_status())
        M5.Lcd.print(sensors.mlxObjectTemp, 1);
    else
        M5.Lcd.print("n/a");
    printDegree(color);
    M5.Lcd.print(" C");
  
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(15, 105);
    M5.Lcd.print("Ambiant: ");
    if (!bme680_status()) { // prefer BME680 for ambient temperature reading
        if (mlx90614_status())
            M5.Lcd.print(sensors.mlxAmbientTemp, 1);
        else
            M5.Lcd.print("n/a");
    } else {
        M5.Lcd.print(sensors.bme680Temp, 1);
    }
}


// print sensor status/readings on serial console
void mlx90614_console() {
    Serial.print("MLX90614: ");
    if (mlx90614_status()) {
        Serial.print("objectTemperature(");
        Serial.print(sensors.mlxObjectTemp, 1);
        Serial.print(" C), ambientTemperature(");
        Serial.print(sensors.mlxAmbientTemp, 1);
        Serial.println(" C)");
    } else {
        Serial.println("sensor not ready!");
    }
}