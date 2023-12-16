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
#include "lorawan.h"
#include "sensors.h"
#include "prefs.h"
#include "utils.h"
#include "rtc.h"

CayenneLPP lpp(LORAWAN_LPP_SIZE);
ASR6501 LoRaWAN;
#ifdef MEMORY_DEBUG_INTERVAL_SECS
UBaseType_t stackWmJoinTask, stackWmQueueTask;
#endif


ASR6501::ASR6501() {
    this->deviceState = NONE;
    this->msgQueue = xQueueCreate(1, sizeof(sensorReadings_t));
}


ASR6501::ASR6501(HardwareSerial* serialPort, uint8_t rxPin, uint8_t txPin) {
    this->begin(serialPort, rxPin, txPin);
    this->deviceState = NONE;
    this->msgQueue = xQueueCreate(1, sizeof(sensorReadings_t));
}


ASR6501::~ASR6501() {
    vQueueDelete(this->msgQueue);
    vTaskDelete(this->joinTaskHandle);
    vTaskDelete(this->queueTaskHandle);
    this->serial->end();
    lpp.~CayenneLPP();
}


bool ASR6501::setupFailed(const char* msg) {
    M5.Lcd.print("ERR");
    delay(1500);
    M5.Lcd.clearDisplay(RED);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(20,70);
    M5.Lcd.print("Failed to setup serial");
    M5.Lcd.setCursor(50,100);
    M5.Lcd.print("LoRaWAN adapter");
    Serial.println(msg);
    this->deviceState = ERROR;
    delay(3000);
    return false;
}


// returns current state of LoRaWAN adapter
lorawanState ASR6501::status() {
    return this->deviceState;
}


// send command to serial LoRaWAN adapter and returns response as string
const char* ASR6501::sendCmd(const char* cmd, uint16_t timeout) {
    static char buf[128], c;
    bool dataRead = false;
    time_t startRead;
    lorawanState prevState; 
    uint8_t i = 0;

    if (xSemaphoreTake(this->SerialLock, SEMAPHORE_BLOCKTIME_MS) == pdTRUE) {
        prevState = this->deviceState;
        this->deviceState = COMMAND;

#ifdef LORAWAN_DEBUG_SERIAL_CMDS
        Serial.printf(">>> %s\n", cmd);
#endif
        snprintf(buf, sizeof(buf), "%s\r\n", cmd);
        this->serial->print(buf);
        vTaskDelay(100/portTICK_PERIOD_MS);

        memset(buf, 0, sizeof(buf));
        startRead = millis(); i = 0;
        while (!dataRead && tsDiff(startRead) <= timeout) {
            while (this->serial->available() > 0) {
                if (i < sizeof(buf)) {
                    c = this->serial->read();
                    if (c == 13) c = 32; // CR -> space
                    if (c >= 32 && c <= 126) { // only printable chars
                        buf[i++] = c;
                        dataRead = true;
                    }
                } else {
                    break;
                }
                vTaskDelay(1/portTICK_PERIOD_MS);
            }
            esp_task_wdt_reset();
            vTaskDelay(100/portTICK_PERIOD_MS);
        }
        xSemaphoreGive(this->SerialLock);
        this->deviceState = prevState;

#ifdef LORAWAN_DEBUG_SERIAL_CMDS
        if (i > 0)
            Serial.printf("<<< %s (%d bytes, %ld ms)\n", buf, i, tsDiff(startRead));
        else
            Serial.printf("<<< [timeout] (%ld ms)\n", tsDiff(startRead));
#endif
        return buf;

    } else {
        Serial.println("LoRaWAN: serial connection busy");
        return NULL;
    }
}


const char* ASR6501::sendCmd(const char* cmd) {
    return this->sendCmd(cmd, LORAWAN_COMMAND_TIMEOUT_MS);
}


// returns true if connected to ASR 6501 serial LoRaWAN adapter
bool ASR6501::connected() {
    if (strstr(this->sendCmd("AT+CGMI?"), "ASR") != NULL &&
        strstr(this->sendCmd("AT+CGMM?"), "6501") != NULL) {
        return true;
    } else {
        this->deviceState = ERROR;
        vTaskDelay(500/portTICK_PERIOD_MS);
        return false;
    }
}


// DevEUI generator using ESP32 mac address
const char* ASR6501::getDevEUI() {
    static char devEUI[17];
    uint8_t mac[6];

    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(devEUI, sizeof(devEUI), "%02X%02X%02XFFFF%02X%02X%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return devEUI;
}


// configure device for Over-The-Air-Authentication (OTAA)
bool ASR6501::setOTAA(const char* appEUI, const char* appKey) {
    char cmd[48];

    if (strstr(this->sendCmd("AT+CJOINMODE=0"), "OK") == NULL)
        return false;
    snprintf(cmd, 28, "AT+CDEVEUI=%s", this->getDevEUI());
    if (strstr(this->sendCmd(cmd), "OK") == NULL)
        return false;
    snprintf(cmd, 28,"AT+CAPPEUI=%s", appEUI);
    if (strstr(this->sendCmd(cmd), "OK") == NULL)
        return false;
    snprintf(cmd, 44, "AT+CAPPKEY=%s", appKey);
    if (strstr(this->sendCmd(cmd), "OK") == NULL)
        return false;

    return true;
}


// triggers join to LoRaWAN network
void ASR6501::join() {
    if (this->deviceState != IDLE)
        return;

    Serial.printf("LoRaWAN: joining network with DevEUI %s...\n", this->getDevEUI());
    // the M5 LoRaWAN module doesn't seem to care about settings for number of max. retries
    // if join fails it keeps trying forever instead of bailing out with '+CJOIN:FAIL'
    if (strstr(this->sendCmd("AT+CJOIN=1,0,8,3"), "OK") != NULL) {
        this->deviceState = JOINING;
    } else {
        Serial.println("LoRaWAN: command 'AT+CJOIN' failed");
        this->deviceState = ERROR;
    }
}


// returns true if device is joined to LoRaWAN network
bool ASR6501::joined() {
    static char resp[64];

    if (this->deviceState == JOINING || this->deviceState == JOINFAIL)
        return false;

    strlcpy(resp, this->sendCmd("AT+CSTATUS?"), sizeof(resp));
    if (strstr(resp, "+CSTATUS:") != NULL) {
        if (strstr(resp, "03") != NULL || strstr(resp, "07") != NULL || strstr(resp, "08") != NULL) {
            this->deviceState = JOINED;
            return true;
        } else {
            this->deviceState = IDLE;
            return false;
        }
    } else {
        Serial.println("LoRaWAN: command 'AT+CSTATUS' failed");
        this->deviceState = ERROR;
        return false;
    }
}


// background task to (re)join LoRaWAN network
void ASR6501::joinTask() {
    static char buf[64], c;
    bool dataRead = false;
    time_t startRead = 0;
    uint8_t i = 0, retryWait = 0;
#ifdef MEMORY_DEBUG_INTERVAL_SECS
    uint16_t loopCounter = 0;
#endif

    while (true) {
        if (this->deviceState == ERROR) {
            Serial.println("LoRaWAN: serial command failed");
            queueStatusMsg("LoRaWAN command", 40, true);
            this->deviceState == JOINFAIL;

        } else if ((this->deviceState == JOINFAIL) && (retryWait <= LORAWAN_JOIN_RETRY_SECS)) {
            if (retryWait == LORAWAN_JOIN_RETRY_SECS) {
                this->deviceState = IDLE;
                retryWait = 0;
            } else if (!retryWait++) {
                Serial.printf("LoRaWAN: join failed, retry in %d seconds\n", LORAWAN_JOIN_RETRY_SECS);
                queueStatusMsg("LoRaWAN nojoin", 65, true);
                if (strstr(this->sendCmd("AT+CSAVE"), "OK") == NULL ||
                    strstr(this->sendCmd("AT+IREBOOT=0"), "OK") == NULL)
                    this->deviceState = ERROR;
            }

        } else if (this->deviceState == IDLE) {
            this->join(); // takes about 5 secs

        } else if (this->deviceState == JOINING) {
            memset(buf, 0, sizeof(buf)); 
            startRead = millis();
            while (!dataRead && tsDiff(startRead) <= (LORAWAN_JOIN_TIMEOUT_SECS * 1000)) {
                while (this->serial->available() > 0) {
                    c = this->serial->read();
                    if (i < sizeof(buf) && c >= 32 && c <= 126) {  // only printable chars
                        buf[i++] = c;
                        dataRead = true;
                        if (strstr(buf, "+CJOIN:OK") != NULL) {
                            Serial.println("LoRaWAN: joined network");
                            queueStatusMsg("LoRaWAN joined", 60, false);
                            this->deviceState = JOINED;
                            break;

                        // never reached, M5 LoRaWAN adapter tries to join forever...
                        } else if (strstr(buf, "+CJOIN:FAIL") != NULL) {
                            this->deviceState = JOINFAIL;
                            retryWait = 0;
                            break;
                        }
                    }
                    if (this->serial->available() <= 5)
                        vTaskDelay(2/portTICK_PERIOD_MS);
                }
                esp_task_wdt_reset();
                vTaskDelay(500/portTICK_PERIOD_MS);
            }
            if (tsDiff(startRead) >= (LORAWAN_JOIN_TIMEOUT_SECS * 1000)) {
                this->deviceState = JOINFAIL;
                retryWait = 0;
            }
        }
#ifdef MEMORY_DEBUG_INTERVAL_SECS
        if (loopCounter++ >= MEMORY_DEBUG_INTERVAL_SECS) {
            stackWmJoinTask = printFreeStackWatermark("joinTask");
            loopCounter = 0;
        }
#endif
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}


void ASR6501::joinTaskWrapper(void* _this) {
    static_cast<ASR6501*>(_this)->joinTask();
}


// background task to check send queue and transmit data every 'lorawanIntervalSecs'
// based on FIFO send queue will only the most recent sensor readings
void ASR6501::queueTask() {
    static char cmd[128], payload[96];
    time_t lastRun = 0;
#ifdef MEMORY_DEBUG_INTERVAL_SECS
    uint16_t loopCounter = 0;
#endif

    while (true) {
        if (this->deviceState == JOINED && this->deviceState != SENDING && 
                tsDiff(lastRun) > (prefs.lorawanIntervalSecs * 1000)) {
            lastRun = millis();
            if (xQueueReceive(this->msgQueue, &sensors, 0) == pdTRUE) {
                strlcpy(payload, this->encodeLPP(sensors), sizeof(payload));
                if (strlen(payload) > 1) {
                    deviceState = SENDING;
                    Serial.printf("LoRaWAN: sending payload%s...", 
                        prefs.lorawanConfirm ? " (confirmed)" : "");
                    snprintf(cmd, sizeof(cmd), "AT+DTRX=%d,3,%d,%s", 
                        prefs.lorawanConfirm ? 1 : 0, strlen(payload), payload);
                    if (strstr(this->sendCmd(cmd), "OK+SEND:") != NULL) {
                        Serial.println("OK");
                        queueStatusMsg("LoRaWAN uplink", 65, false);
                    } else {
                        Serial.printf("ERROR");
                        queueStatusMsg("LoRaWAN failed", 65, true);
                    }
                    vTaskDelay(500/portTICK_PERIOD_MS);
                    // set state back to 'JOINED' if device is online or 'IDLE' if offline
                    this->joined();
                }
            }
        }
#ifdef MEMORY_DEBUG_INTERVAL_SECS
        if (loopCounter++ >= MEMORY_DEBUG_INTERVAL_SECS) {
            stackWmQueueTask = printFreeStackWatermark("queueTask");
            loopCounter = 0;
        }
#endif
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}


void ASR6501::queueTaskWrapper(void* _this) {
    static_cast<ASR6501*>(_this)->queueTask();
}


// encodes sensor data as CayenneLPP and returns payload as hex string
const char* ASR6501::encodeLPP(sensorReadings_t sensors) {
    static char payload[96];

    lpp.reset();
    if (mlx90614_status())
        lpp.addTemperature(1, sensors.mlxObjectTemp);
    if (bme680_status() || sfa30_status())
        lpp.addRelativeHumidity(2, bme680_status() ? sensors.bme680Hum : sensors.sfa30Hum);
    if (sfa30_status())
        lpp.addConcentration(3, sensors.sfa30HCHO*10); // ppb*10
    if (bme680_status() > 1) {
        lpp.addGenericSensor(4, sensors.bme680Iaq);
        lpp.addConcentration(5, sensors.bme680eCO2); // ppm
        lpp.addConcentration(6, sensors.bme680VOC*10); // ppm*10
    }
    lpp.addGenericSensor(7, getRuntimeMinutes());

    if (lpp.getError() || ((lpp.getSize() * 2) >= sizeof(payload)-1)) {
        Serial.println("LoRaWAN: CayenneLPP encoding failed");
        queueStatusMsg("LoRaWAN encoding", 45, true);
        return "-";  // empty
    }
    
    memset(payload, 0, sizeof(payload));
    array2string(lpp.getBuffer(), lpp.getSize(), payload);
    Serial.printf("LoRaWAN: encoded sensor data (%s, %d bytes)\n", payload, lpp.getSize());

    return payload;
}


// initialize serial M5 LoRaWAN module, configure for OTAA,
// join network and and start background task to send data placed in queue
bool ASR6501::begin(HardwareSerial* serialPort, uint8_t rxPin, uint8_t txPin) { 
    bool adapterReady = false;
    uint16_t timeout = 0;

    if (!prefs.lorawanEnable) {
        Serial.println("LoRaWAN: disabled");
        return false;
    } else {
        M5.Lcd.clearDisplay(BLUE);
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.setFreeFont(&FreeSans12pt7b);
        M5.Lcd.setCursor(20, 35);
        M5.Lcd.print("LoRaWAN device settings");
        M5.Lcd.setCursor(20, 65);
        M5.Lcd.print("DevEUI:");
        M5.Lcd.setCursor(20, 95);
        M5.Lcd.print(this->getDevEUI());
        M5.Lcd.setCursor(20, 125);
        M5.Lcd.print("JoinEUI:");
        M5.Lcd.setCursor(20, 155);
        M5.Lcd.printf(prefs.lorawanAppEUI);
        M5.Lcd.setCursor(20, 185);
        M5.Lcd.print("AppKey:");
        M5.Lcd.setCursor(20, 215);
        M5.Lcd.printf("%.16s...", prefs.lorawanAppKey);
        delay(3000);
    }

    this->serial = serialPort;
    if (this->deviceState != NONE)
        serial->end();
    this->serial->begin(115200, SERIAL_8N1, rxPin, txPin);
    this->SerialLock = xSemaphoreCreateMutex();

    M5.Lcd.clearDisplay(BLUE);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(20, 35);    
    M5.Lcd.print("LoRaWAN adapter...");
    Serial.print("LoRaWAN: connecting to serial adapter ASR 6501...");
    while (!adapterReady && timeout++ < (LORAWAN_MODULE_TIMEOUT_SECS * 2))
        adapterReady = this->connected();
    if (adapterReady) {
        M5.Lcd.print("OK");
        Serial.println("OK");
    } else {
        return this->setupFailed("N/A");
    }
    delay(500);

    M5.Lcd.setCursor(20, 65);
    M5.Lcd.print("Restore defaults...");
    Serial.print("LoRaWAN: restore defaults, reboot...");
    if (strstr(this->sendCmd("AT+CRESTORE"), "OK") != NULL &&
        strstr(this->sendCmd("AT+CSAVE"), "OK") != NULL &&
        strstr(this->sendCmd("AT+IREBOOT=0"), "OK")) {
        M5.Lcd.print("OK");
        Serial.println("OK");
    } else {
        return this->setupFailed("ERROR");
    }
    delay(500);

    M5.Lcd.setCursor(20, 95);
    M5.Lcd.print("Configure OTAA...");
    Serial.print("LoRaWAN: configure OTAA...");
    if (this->setOTAA(prefs.lorawanAppEUI, prefs.lorawanAppKey)) {  
        M5.Lcd.print("OK");
        Serial.println("OK");
    } else {
        return this->setupFailed("ERROR");
    }
    delay(500);

    M5.Lcd.setCursor(20, 125);
    M5.Lcd.print("Enable ADR, class C...");
    Serial.print("LoRaWAN: enable ADR, set class C mode...");
    if (strstr(this->sendCmd("AT+CCLASS=2"), "OK") != NULL &&  // 0: Class A, 1: Class B, 2: Class C
        strstr(this->sendCmd("AT+CADR=1"), "OK") != NULL) {  // ADR
        M5.Lcd.print("OK");
        Serial.println("OK");
    } else {
        return this->setupFailed("ERROR");
    }
    delay(500);

    // 868 MHz, channels 0-7, RX2: 869.525MHz on SF9BW125
    M5.Lcd.setCursor(20, 155);
    M5.Lcd.print("Set 8 channels, RX2...");
    Serial.print("LoRaWAN: configure 8 channels, RX2 window...");
    if (strstr(this->sendCmd("AT+CWORKMODE=2"), "OK") != NULL &&
        strstr(this->sendCmd("AT+CFREQBANDMASK=0001"), "OK") != NULL &&
        strstr(this->sendCmd("AT+CRXP=0,0,869525000"), "OK") != NULL) {
        M5.Lcd.print("OK");
        Serial.println("OK");
    } else {
        return this->setupFailed("ERROR");
    }
    delay(500);

    M5.Lcd.setCursor(20, 185);
    M5.Lcd.printf("%s confirm data...", prefs.lorawanConfirm ? "Enable" : "Disable");
    Serial.printf("LoRaWAN: %s confirm data transmission...", prefs.lorawanConfirm ? "enable" : "disable");
    if (prefs.lorawanConfirm) {
        if (strstr(this->sendCmd("AT+CCONFIRM=1"), "OK") != NULL &&
            strstr(this->sendCmd("AT+CNBTRIALS=1,3"), "OK") != NULL) {
            M5.Lcd.print("OK");
            Serial.println("OK");
        } else {
            return this->setupFailed("ERROR");
        }
    } else {
        if (strstr(this->sendCmd("AT+CCONFIRM=0"), "OK") != NULL &&
            strstr(this->sendCmd("AT+CNBTRIALS=0,1"), "OK") != NULL) {
            M5.Lcd.print("OK");
            Serial.println("OK");
        } else {
            return this->setupFailed("ERROR");
        }
    }
    delay(500);

    if (strstr(this->sendCmd("AT+CSAVE"), "OK") == NULL)
        return this->setupFailed("ERROR");
    this->deviceState = IDLE;

    M5.Lcd.setCursor(20, 215);
    M5.Lcd.print("Joining network...");

    // join LoRaWAN network, deviceState is 'JOINED' if successful and 'IDLE' if failed
    xTaskCreatePinnedToCore(this->joinTaskWrapper,
        "joinTask", 2560, this, 5, &this->joinTaskHandle, 0);

    // start checking send queue for sensor data
    xTaskCreatePinnedToCore(this->queueTaskWrapper,
        "queueTask", 2560, this, 5, &this->queueTaskHandle, 1);

    return true;
}


// place current sensor reading in send queue
bool ASR6501::queue(sensorReadings_t data) {
    if (this->deviceState == JOINED) {
        Serial.println("LoRaWAN: queuing sensor data");
        return (xQueueOverwrite(this->msgQueue, (void*)&data) == pdTRUE);
    } else {
        return false;
    }
}

