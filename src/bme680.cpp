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

#include "bme680.h"
#include "sensors.h"
#include "prefs.h"
#include "display.h"
#include "utils.h"

// BME680 (Temp, Hum, Pres, eCO2, VOC)
BME680 bme680;
static const std::string iaq_accurracy_verbose[4] = {
    "stabilizing",
    "uncertain",
    "calibrating",
    "calibrated"
};
static bsec_virtual_sensor_t sensorList[7] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_RUN_IN_STATUS
};
/* Configure the BSEC library with information about the sensor
    18v/33v = Voltage at Vdd. 1.8V or 3.3V
    3s/300s = BSEC operating mode, BSEC_SAMPLE_RATE_LP or BSEC_SAMPLE_RATE_ULP
    4d/28d = Operating age of the sensor in days
    generic_18v_3s_4d
    generic_18v_3s_28d
    generic_18v_300s_4d
    generic_18v_300s_28d
    generic_33v_3s_4d
    generic_33v_3s_28d
    generic_33v_300s_4d
    generic_33v_300s_28d
*/
static const uint8_t bsec_config_iaq[] = {
#include "config/generic_33v_3s_4d/bsec_iaq.txt"  // LP sample rate, 4 days calibration backlog
};
static bool endButtonWaitLoop = false;


// constructor for BME680 sensor
BME680::BME680() {
    this->bsec = Bsec();
    this->ready = false;
    this->error = true;
}


void BME680::loadState(Bsec bsec) {
    uint8_t newState[BSEC_MAX_STATE_BLOB_SIZE] = { 0 };

    if (prefs.bsecState[0] == BSEC_MAX_STATE_BLOB_SIZE) {
        Serial.print("BME680: restore BSEC state (");
        for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
            newState[i] = prefs.bsecState[i+1];
            Serial.print(newState[i], HEX);
        }
        Serial.print(")...");
        bsec.setState(newState);
        if (status(bsec) > 0)
            Serial.println("OK");
    } else {
        Serial.println("BME680: no previously saved BSEC state found");
        memset(prefs.bsecState, 0, BSEC_MAX_STATE_BLOB_SIZE+1);
        savePrefs(false);
    }
}


// Save current BSEC state to flash if IAQ accuracy
// reaches 3 for the first time or peridically if
// BME680_STATE_SAVE_PERIOD has passed
bool BME680::updateState(Bsec bsec) {
    uint8_t currentState[BSEC_MAX_STATE_BLOB_SIZE] = { 0 };
    static time_t lastStateUpdate = 0;

    if ((lastStateUpdate == 0 && readings.bme680IaqAccuracy >= 3) ||
        tsDiff(lastStateUpdate) >= BME680_STATE_SAVE_PERIOD) {

        bsec.getState(currentState);
        if (status(bsec)) {
            Serial.print("BME680: writing BSEC state to flash (");
            prefs.bsecState[0] = BSEC_MAX_STATE_BLOB_SIZE;
            for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
                Serial.print(currentState[i], HEX);
                prefs.bsecState[i+1] = currentState[i];
            }
            Serial.print(")...");
            savePrefs(false);
            Serial.println("OK");
            lastStateUpdate = millis();
            return true;
        } else {
            Serial.println("BME680: failed to read BSEC state");
            return false;
        }
    }
    return false;
}


// catch 'yes' event from dialogResetBSEC()
void BME680::eventResetBSEC(Event& e) {
    endButtonWaitLoop = true;
    if (!strcmp(e.objName(), "Yes")) {
        M5.Lcd.clearDisplay(BLUE);
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.setFreeFont(&FreeSans12pt7b);
        M5.Lcd.setCursor(20, 40);
        Serial.println("BME680: forcing reset of BSEC calibration data");
        M5.Lcd.print("Reset BSEC data...");
        prefs.bsecState[0] = 0; // invalidate bsec settings
        loadState(bme680.bsec);
        M5.Lcd.print("OK");
        delay(1500);
    }
}


// draw yes/no dialog on LCD asking to reset the BSEC state data
void BME680::dialogResetBSEC() {
    uint16_t timeout = 0;

    if (!prefs.bsecState[0]) // no bsec state data saved so far
        return;

    ButtonColors onColor = {RED, WHITE, WHITE};
    ButtonColors offColor = {DARKGREEN, WHITE, WHITE};
    Button bYes(35, 130, 120, 60, false, "Yes", offColor, onColor, MC_DATUM);
    Button bNo(165, 130, 120, 60, false, "No", offColor, onColor, MC_DATUM);

    M5.Lcd.clearDisplay(WHITE);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(45,70);
    M5.Lcd.print("Reset BME680/BSEC");
    M5.Lcd.setCursor(70,100);
    M5.Lcd.print("calibration data?");

    M5.Buttons.draw();
    bYes.addHandler(eventResetBSEC, E_RELEASE);
    bNo.addHandler(eventResetBSEC, E_RELEASE);
    while (timeout++ < (DISPLAY_DIALOG_TIMEOUT_SECS * 1000) && !endButtonWaitLoop) {
        M5.update();
        delay(1);
    }
}


// 0: error, 1: gas sensor warmup, 2: all sensor readings available
uint8_t BME680::status(Bsec bsec) {
    if (bsec.status < BSEC_OK) {
        Serial.printf("BME680: BSEC library error (%d)\n", bsec.status);
        return 0;
    } else if (bsec.status > BSEC_OK) {
        Serial.printf("BME680: BSEC library warning (%d)\n", bsec.status);
    }

    if (bsec.bme680Status < BME680_OK) {
        Serial.printf("BME680: sensor code (%d)\n", bsec.bme680Status);
        return 0;
    } else if (bsec.bme680Status > BME680_OK) {
        Serial.printf("BME680: sensor warning (%d)\n", bsec.bme680Status);
    }

    if (bsec.runInStatus < 1) // gas sensor warmup
        return 1;

    return 2;  // ready, delivering all readings
}


// public method for static status() function call
uint8_t BME680::status() {
    return status(this->bsec);
}


// return current accuracy of eCO2 and VOC readings as string value
const char* BME680::accuracy(uint8_t data) {
    return iaq_accurracy_verbose[data].c_str();
}


// initialize BME680 sensor (Temp, Hum, Pres, eCO2, VOC) on I2C bus
bool BME680::setup() {
    bsec_version_t bsec_version;
    char statusMsg[64];

    this->bsec.begin(BME680_I2C_ADDR_PRIMARY, Wire);
    if (this->status()) {
        this->bsec.setConfig(bsec_config_iaq);
        if (!this->status()) {
            Serial.println("ERROR: Failed to set BME680 configuration");
        } else {
            this->loadState(this->bsec);
            this->bsec.updateSubscription(sensorList, 7, BSEC_SAMPLE_RATE_LP); // see bsec_config_iaq[]
            if (!this->status())
                Serial.println("ERROR: Failed to subscribe to BME680 sensors");
        }
    }
    if (!this->status()) {
        snprintf(statusMsg, sizeof(statusMsg), "BME680 failed, error %d", this->bsec.bme680Status);
        displayStatusMsg(statusMsg, 40, false, WHITE, RED);
        Serial.printf("BME680: failed to initialize sensor");
        delay(3000);
        return false;
    } else {
        displayStatusMsg("Sensor BME680 ready", 40, false, WHITE, DARKGREEN);
        bsec_get_version(&bsec_version);
        Serial.printf("BME680: sensor ready, sample rate 3s, BSEC v%d.%d.%d.%d\n", bsec_version.major,
            bsec_version.minor, bsec_version.major_bugfix, bsec_version.minor_bugfix);
        delay(1500);
        this->dialogResetBSEC();
        return true;
    }
}


// get current readings from BME680 and copy them to sensor struct 
bool BME680::read() {
    if (this->bsec.run() && this->status()) {
        readings.bme680Temp = this->bsec.temperature;
        readings.bme680Hum = int(this->bsec.humidity);
        readings.bme680Iaq = int(this->bsec.iaq);
        readings.bme680IaqAccuracy = int(this->bsec.iaqAccuracy);
        readings.bme680GasResistance = int(this->bsec.gasResistance/1000); // kOhm
        readings.bme680eCO2 = int(this->bsec.co2Equivalent);
        readings.bme680VOC = this->bsec.breathVocEquivalent;
        return true;
    } else {
        return false;
    }
}


// returns true if either BME680's temperature or gas 
// resistance readings have changed significantly
bool BME680::changed() {
    static float lastGasRes = 0.0, lastTemp = 0.0;
    static uint8_t lastHum;
    bool changed = false;

    if (!this->status())
        return false;
    if (abs(lastGasRes - readings.bme680GasResistance) >= GASRESISTANCE_PUBLISH_THRESHOLD) // kOhm
        changed = true;
    if (abs(lastTemp - readings.bme680Temp) >= TEMP_PUBLISH_THRESHOLD)
        changed = true;
    if (abs(lastHum - readings.bme680Hum) >= HUM_PUBLISH_THRESHOLD)
        changed = true;

    lastHum = readings.bme680Hum;
    lastTemp = readings.bme680Temp;
    lastGasRes = readings.bme680GasResistance;

    return changed;
}


// display BME680 readings an M5 Tough's OLED display if available
void BME680::display() {
    if (this->status() > 0) {
        if (this->bsec.runInStatus > 0) {
            M5.Lcd.setCursor(175, 150);
            M5.Lcd.print("VOC: ");  // shown right after HCHO on display
            M5.Lcd.print(readings.bme680VOC, 1);
            M5.Lcd.setCursor(15, 180); // new line on display with IAQ/eCO2 readings
            M5.Lcd.print("IAQ: ");
            M5.Lcd.print(readings.bme680Iaq);
            M5.Lcd.print("/");
            M5.Lcd.print(readings.bme680IaqAccuracy);
            M5.Lcd.setCursor(175, 180);
            M5.Lcd.print("eCO2: ");
            M5.Lcd.print(readings.bme680eCO2);
        } else {
            M5.Lcd.setCursor(175, 150);
            M5.Lcd.print("VOC: --.-"); // placed on display after HCHO reading
            M5.Lcd.setCursor(15, 180);
            M5.Lcd.print("IAQ: --/-");
            M5.Lcd.setCursor(175, 180);
            M5.Lcd.print("eCO2: ---");
        }
        updateState(this->bsec);
    } else {
        M5.Lcd.setCursor(175, 150);
        M5.Lcd.print("VOC: n/a"); // first row after HCHO
        M5.Lcd.setCursor(15, 180);
        M5.Lcd.print("IAQ: n/a"); // second row
        M5.Lcd.setCursor(175, 180);
        M5.Lcd.print("eCO2: n/a");
    }
}


// print sensor status/readings on serial console
void BME680::console() {
    Serial.print("BME680: ");
    if (this->status() > 0) {
        if (this->status() > 1) {
            Serial.printf("IAQ(%d, %s), eCO2(%d ppm), VOC(", 
                readings.bme680Iaq, accuracy(readings.bme680IaqAccuracy), readings.bme680eCO2);
            Serial.print(readings.bme680VOC, 1);
            Serial.print(" ppm), ");
        } else {
            Serial.print("gas sensor warmup, ");
        }
        Serial.printf("Gas(%d kOhm), Humdity(%d %%), Temperature(",
            readings.bme680GasResistance, readings.bme680Hum);
        Serial.print(readings.bme680Temp, 1);
        Serial.println(" C)");
    } else {
        Serial.println("sensor not ready!");
    }  
}