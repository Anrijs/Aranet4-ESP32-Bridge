/*
 *  Bluetooth + WiFi stack takes a lot of space...
 *  
 *  Upload files to SPIFFS using https://github.com/me-no-dev/arduino-esp32fs-plugin/releases/
 *  
 *  Also make sure esp32 board library is up to date https://github.com/espressif/arduino-esp32
 *  As of writing this 1.0.6 is latest and tested.
 *  Upgrading from 1.0.4 fixed some BLE stack RAM issues
 */

#include "main.h"
#include "esp_task_wdt.h"

long nextReport = 0;

int downloadHistory(Aranet4* ar4, AranetDeviceStatus* s, int newRecords);

void setup() {
    Serial.begin(115200);
    Serial.println("Setup");

    pinMode(MODE_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT); // green LED

    if(!SPIFFS.begin()){
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    configLoad();
    devicesLoad();

    isAp = getBootWiFiMode();

    if (!setupWifiAndWebserver()) {
        Serial.println("Failed to initialize.");
         return;
    }

    createInfluxClient();

    char* rstReason0 = getResetReason(rtc_get_reset_reason(0));
    char* rstReason1 = getResetReason(rtc_get_reset_reason(1));

    String resetMsg = String("CPU0: ") + String(rstReason0) + String(", CPU1: ") + String(rstReason1);
    log(resetMsg, ILog::INFO);

    startNtpSyncTask();

    // Set up bluettoth security and callbacks
    Aranet4::init();
    ar4.setConnectTimeout(CFG_BT_CONNECT_TIMEOUT);

    pScan->setActiveScan(true);

    delay(500);

    esp_task_wdt_init(15, false);
    enableCore0WDT();
    enableCore1WDT();

    setupWatchdog();
}

void loop() {
    if (isAp) {
      task_sleep(1000);
      return;
    }

    if (nextReport < millis()) {
        nextReport = millis() + 10000; // 10s
        Point pt = influxCreateStatusPoint(&prefs);
        pt.addField("wifi_uptime", millis() - wifiConnectedAt);
        influxSendPoint(influxClient, pt);
    }

    // Scan devices, then compare with saved devices and read data.

    int count = ar4devices.size;

    while (isScanOpen()) {
        Serial.println("Waiting for scan to close");
        // don't read data, while scan page is opened
        task_sleep(1000);
    }

    // do scan and read
    if (count > 0) {
        bool doScan = false;
        for (int j=0; j<count; j++) {
            AranetDeviceStatus* s = s = &ar4status[j];
            long expectedUpdateAt = s->updated + ((s->data.interval - s->data.ago) * 1000);
            doScan |= !(millis() < expectedUpdateAt && s->updated > 0);
            if (doScan) break;
        }

        if (doScan) {
            runBtScan();
            task_sleep(500);

            while (pScan->isScanning()) {
                Serial.print(".");
                task_sleep(1000);
            }

            NimBLEScanResults results = pScan->getResults();

            Serial.printf(" Found %u devices\n", results.getCount());

            for (int i = 0; i < results.getCount(); i++) {
                NimBLEAdvertisedDevice adv = results.getDevice(i);

                std::string strManufacturerData = adv.getManufacturerData();

                uint16_t manufacturerId = 0;
                uint8_t cManufacturerData[100];
                int cLength = strManufacturerData.length();

                strManufacturerData.copy((char *)cManufacturerData, cLength, 0);
                memcpy(&manufacturerId, (void*) cManufacturerData, sizeof(manufacturerId));

                bool hasManufacturerData = manufacturerId == 0x0702 && cLength >= 22;

                if (adv.getName().find("Aranet") == std::string::npos && !hasManufacturerData) {
                    continue;
                }

                // find saved aranet device
                AranetDeviceStatus* s = findSavedDevice(&adv);

                if (!s) break; // device not saved

                AranetDevice* d = s->device;
                long expectedUpdateAt = s->updated + ((s->data.interval - s->data.ago) * 1000);
                bool readCurrent = !(millis() < expectedUpdateAt && s->updated > 0);
                bool readHistory = s->pending > 0;

                if(readCurrent && !readHistory) break; // no new measurements

                startWatchdog(30);

                // disconenct from old
                long disconnectTimeout = millis() + 1000;
                while (ar4.isConnected() && disconnectTimeout > millis()) task_sleep(10);

                bool dataOk = false;
                if (hasManufacturerData) {
                    // read from beacon
                    Serial.print("Read from beacon data ");
                    Serial.println(d->name);

                    s->data = AranetData();
                    uint16_t len = sizeof(AranetData);
                    memcpy(&s->data, (void*) cManufacturerData + 10, len);
                    dataOk = true;
                } else {
                    // read from gatt
                    Serial.print("Connecting to ");
                    Serial.println(d->name);

                    ar4_err_t status = ar4.connect(d->addr);

                    if (status == AR4_OK) {
                        s->data = ar4.getCurrentReadings();
                        dataOk = ar4.getStatus() == AR4_OK;
                    } else {
                        Serial.printf("Aranet4 connect failed: (%i)\n", status);
                        break;
                    }
                }

                if (dataOk && s->data.co2 != 0) {
                    Serial.printf("  CO2: %i\n  T:  %.2f\n", s->data.co2, s->data.temperature / 20.0);

                    // Check how many records might have been skipped
                    long mStart = millis();
                    int newRecords = s->pending;
                    if (newRecords == 0) newRecords = (((mStart - s->updated) / 1000) / s->data.interval) - 1;

                    if (s->updated != 0 && newRecords > 0) {
                        Serial.printf("I see missed %i records\n", newRecords);

                        int result = downloadHistory(&ar4, s, newRecords);
                        if (result >= 0) {
                            Serial.printf("Dwonlaoded %d logs\n", result);
                        } else {
                            Serial.println("Couldn't read history");
                        }
                    }

                    s->updated = millis();

                    Serial.print("Uploading data... ");
                    Point pt = influxCreatePoint(&prefs, d, &s->data);
                    if (!influxSendPoint(influxClient, pt)) {
                        Serial.println(" Upload failed.");
                    }
                    if (!s->mqttReported) {
                        mqttSendConfig(&mqttClient, &prefs, d);
                        s->mqttReported = true;
                    }
                    mqttSendPoint(&mqttClient, &prefs, d, &s->data);
                } else {
                    Serial.print("Read failed.");
                }

                if (ar4.isConnected()) {
                    ar4.disconnect();
                    Serial.println("Disconnected.");
                }

                cancelWatchdog();
                influxFlushBuffer(influxClient);
            }
            Serial.println("Waiting for next scan");
        }
        Serial.print(".");
    }
    influxFlushBuffer(influxClient);
    task_sleep(5000);
}

int downloadHistory(Aranet4* ar4, AranetDeviceStatus* s, int newRecords) {
    int result = 0;
    AranetDevice* d = s->device;
    AranetData adata;
    adata.ago = 0;
    adata.battery = s->data.battery;
    adata.interval = s->data.interval;

    // if info was received from beacon, ar4 will be disconencted
    if (!ar4->isConnected()) {
        if (ar4->connect(d->addr) != AR4_OK) {
            return -1;
        }
    }

    int totalLogs = ar4->getTotalReadings();
    if (newRecords > totalLogs) newRecords = totalLogs;

    int start = totalLogs - newRecords;
    if (start < 1) start = 1;

    time_t tnow = time(nullptr); // should be seconds
    long timestamp = tnow - (s->data.interval * newRecords);

    while (newRecords > 0 && ar4->isConnected()) {
        uint16_t logCount = CFG_HISTORY_CHUNK_SIZE;
        if (newRecords < CFG_HISTORY_CHUNK_SIZE) logCount = newRecords;

        // reset watchdog (1s per log, at least 30s)
        cancelWatchdog();
        startWatchdog(max((int) logCount, 30));

        Serial.printf("Will read %i results from %i @ %i\n", logCount, start, NimBLEDevice::getMTU());
        ar4->getHistory(start, logCount, logs);

        // Sometimes aranet might disconect, before full history is received
        // Set last update time to latest received timestamp;
        if (!ar4->isConnected()) {
            break;
        } else {
            start += logCount;
            newRecords -= logCount;
            s->pending = newRecords;
        }

        Serial.printf("Sending %i logs. %i remm\n", logCount, newRecords);

        result += logCount;

        for (uint16_t k = 0; k < logCount; k++) {
            if (k % 10 == 0) {
                Serial.print(k/10);
            } else {
                Serial.print(".");
            }

            adata.co2 = logs[k].co2;
            adata.temperature = logs[k].temperature;
            adata.pressure = logs[k].pressure;
            adata.humidity = logs[k].humidity;

            Point pt = influxCreatePointWithTimestamp(&prefs, d, &adata, timestamp);
            influxSendPoint(influxClient, pt);
            timestamp += s->data.interval;
        }
        Serial.println();
        influxFlushBuffer(influxClient);
    }

    return result;
}
