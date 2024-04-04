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
#include "MikroTikBT5.h"

long nextReport = 0;

int downloadHistory(Aranet4* ar4, AranetDevice* d, int newRecords);

void setup() {
    Serial.begin(115200);
    Serial.println("Setup");

    pinMode(MODE_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT); // green LED

    spiffsOk = SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED);
    if(!spiffsOk){
        Serial.println("An Error has occurred while mounting SPIFFS");
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

    pScan->setActiveScan(false);
    pScan->setInterval(97);
    pScan->setWindow(37);

    delay(500);

    esp_task_wdt_init(15, false);
    enableCore0WDT();
    enableCore1WDT();

    setupWatchdog();

    setupWireguard();
}

void loop() {
    ws.cleanupClients();
    if (nextReport < millis()) {
        nextReport = millis() + 10000; // 10s
        Point pt = influxCreateStatusPoint(&prefs);
        pt.addField("wifi_uptime", millis() - wifiConnectedAt);
        influxSendPoint(influxClient, pt);
    }


    // We don't want to do unexpected pairing here...
    ar4callbacks.enablePairing();
    for (AranetDevice* d : ar4devices) {
        if (d->state == STATE_BEGIN_PAIR) {
            d->state = STATE_PAIRING;
            ar4.disconnect();
            ar4callbacks.providePin(-1);
            if (ar4.connect(d->addr, false) == AR4_OK) {
                ws.textAll("PAIR_PIN");
                if (ar4.secureConnection() == AR4_OK) {
                    String name = ar4.getName();
                    ws.textAll("SUCCESS:Connected to " + name);
                    d->state = STATE_PAIRED;
                    devicesSave();
                } else {
                    ws.textAll("ERROR:Pair failed");
                    d->state = STATE_NOT_PAIRED;
                }
                ar4.disconnect();
            } else {
                ws.textAll("ERROR:Device not found");
                d->state = STATE_NOT_PAIRED;
            }
        }
    }

    // Scan devices, then compare with saved devices and read data.
    // do scan and read

    Serial.println("Scanning BT devices");
    long procStart = millis();
    pScan->start(CFG_BT_SCAN_DURATION);

    NimBLEScanResults results = pScan->getResults();

    Serial.printf(" Found %u devices\n", results.getCount());

    ar4callbacks.disablePairing();
    for (int i = 0; i < results.getCount(); i++) {
        NimBLEAdvertisedDevice adv = results.getDevice(i);
        AranetDevice* d = findSavedDevice(&adv);

        std::string strManufacturerData = adv.getManufacturerData();

        uint16_t manufacturerId = 0;
        uint8_t cManufacturerData[100];
        int cLength = strManufacturerData.length();

        strManufacturerData.copy((char *)cManufacturerData, cLength, 0);
        memcpy(&manufacturerId, (void*) cManufacturerData, sizeof(manufacturerId));

        bool isMikrotik = manufacturerId == MIKROTIK_MANUFACTURER_ID;
        bool isAirvalent = adv.isAdvertisingService(UUID_Airvalent);
        bool isAranet = adv.isAdvertisingService(UUID_Aranet4);

        bool hasManufacturerData = manufacturerId == 0x0702 && cLength >= 22;
        bool hasName = adv.getName().find("Aranet") != std::string::npos;

        if (isMikrotik) {
            MikroTikBeacon beacon;
            beacon.unpack(cManufacturerData, cLength);
            if (!beacon.isValid()) continue;

            // send beacon data
            Point point("bt5-tag");
            point.addTag("device", prefs.getString(PREF_K_SYS_NAME));
            point.addTag("name", adv.getAddress().toString().c_str());
            if (beacon.hasTemperature()) {
                point.addField("temperature", beacon.temperature);
            }

            point.addField("accel_x", beacon.acceleration.x);
            point.addField("accel_y", beacon.acceleration.y);
            point.addField("accel_z", beacon.acceleration.z);

            point.addField("battery", beacon.battery);
            point.addField("flags", beacon.flags);
            point.addField("uptime", beacon.flags);

            point.addField("rssi", adv.getRSSI());

            if (!influxSendPoint(influxClient, point)) {
                Serial.println(" Upload failed.");
            }

            registerScannedDevice(&adv, "TG-BT5");
            continue;
        }

        if (isAirvalent) {
            registerScannedDevice(&adv, "Airvalent");

            if (d && d->enabled && d->state == STATE_PAIRED && d->gatt) {
                long expectedUpdateAt = d->updated + ((d->data.interval) * 1000);
                bool readCurrent = !(millis() < expectedUpdateAt && d->updated > 0);

                if (!readCurrent) continue;

                Serial.println("[Airvalent] Connecting");
                startWatchdog(30);

                // disconenct from old
                long disconnectTimeout = millis() + 1000;
                while (ar4.isConnected() && disconnectTimeout > millis()) task_sleep(10);

                if(airv.connect(&adv) != AIRV_OK) {
                    if (ar4callbacks.pairWasdenied()) {
                        // clear paired flag.
                        d->state = STATE_NOT_PAIRED;
                        Serial.printf("[Airvalent] clear paired flag\n");
                    }
                    Serial.println("[Airvalent] Failed.");
                    airv.disconnect();
                    continue;
                }

                Serial.println("[Airvalent] Connected!");
                AirvalentData data = airv.getCurrentReadings();

                d->data.packing = AR4_PACKING_AIRVALENT;
                d->data.co2 = data.co2;
                d->data.temperature = data.temperature;
                d->data.humidity = data.humidity;
                d->data.pressure = data.pressure;
                d->data.interval = airv.getInterval();
                d->data.battery = airv.getBattery() & 0x7F;

                if (data.co2 > 0 && data.co2 < 0xFFFF && data.temperature < 1000) {
                    d->updated = millis();

                    Serial.println("Uploading data... ");
                    Point pt = influxCreateAirvalentPoint(&prefs, d, &d->data);
                    pt.addField("rssi", adv.getRSSI());
                    if (!influxSendPoint(influxClient, pt)) {
                        Serial.println(" Upload failed.");
                    }
                    if (!d->mqttReported) {
                        mqttSendConfig(&mqttClient, &prefs, d);
                        d->mqttReported = true;
                    }
                    mqttSendPoint(&mqttClient, &prefs, d, &d->data);
                }

                airv.disconnect();
                cancelWatchdog();
            }
            continue;
        }

        if (!hasName && !hasManufacturerData) {
            continue;
        }

        if (hasName)
            registerScannedDevice(&adv, nullptr);
        else
            registerScannedDevice(&adv, "Aranet");

        if (!d) {
            Serial.printf("Dont read from %s. Not saved\n",adv.getAddress().toString().c_str());
            break; // device not saved
        }

        if (!d->enabled) {
            Serial.printf("Dont read from %s. Not enabled\n",adv.getAddress().toString().c_str());
            break;
        }

        long expectedUpdateAt = d->updated + ((d->data.interval - d->data.ago) * 1000);
        bool readCurrent = !(millis() < expectedUpdateAt && d->updated > 0);
        bool readHistory = d->history && d->pending > 0;

        if(!readCurrent && !readHistory) {
            Serial.println("No new data");
            break; // no new measurements
        }

        startWatchdog(30);

        // disconenct from old
        long disconnectTimeout = millis() + 1000;
        while (ar4.isConnected() && disconnectTimeout > millis()) task_sleep(10);

        bool dataOk = false;
        bool didRead = false;

        if (hasManufacturerData) {
            // read from beacon
            Serial.print("[Aranet4] Read from beacon data ");
            Serial.println(d->name);

            d->data.parseFromAdvertisement(cManufacturerData);

            dataOk = true;
            didRead = true;
        } else if (d->gatt) { // gatt must be enabled to allow reading by cinencting
            // read from gatt
            Serial.print("[Aranet4] Connecting to ");
            Serial.print(d->name);

            ar4_err_t status = ar4.connect(adv.getAddress(), d->state == STATE_PAIRED);
            didRead = true;

            if (status == AR4_OK) {
                d->data = ar4.getCurrentReadings();
                dataOk = ar4.getStatus() == AR4_OK;
            } else {
                // Try insecure
                if (ar4callbacks.pairWasdenied()) {
                    // clear paired flag.
                    d->state = STATE_NOT_PAIRED;
                    Serial.printf("[Aranet4] clear paired flag\n");
                }
                Serial.printf("[Aranet4] connect failed: (%i)\n", status);
                break;
            }
        }

        if (didRead && dataOk) {
            Serial.printf("  CO2: %i\n  T:  %.2f\n", d->data.co2, d->data.temperature / 20.0);

            // Check how many records might have been skipped
            if (d->history) {
                long mStart = millis();
                int newRecords = d->pending;
                if (newRecords == 0) newRecords = (((mStart - d->updated) / 1000) / d->data.interval) - 1;

                if (d->updated != 0 && newRecords > 0) {
                    Serial.printf("I see missed %i records\n", newRecords);

                    int result = downloadHistory(&ar4, d, newRecords);
                    if (result >= 0) {
                        Serial.printf("Dwonlaoded %d logs\n", result);
                    } else {
                        Serial.println("Couldn't read history");
                    }
                }
            }

            d->updated = millis();

            Serial.print("Uploading data... ");
            Point pt = influxCreatePoint(&prefs, d, &d->data);
            pt.addField("rssi", adv.getRSSI());
            if (!influxSendPoint(influxClient, pt)) {
                Serial.println(" Upload failed.");
            }
            if (!d->mqttReported) {
                mqttSendConfig(&mqttClient, &prefs, d);
                d->mqttReported = true;
            }
            mqttSendPoint(&mqttClient, &prefs, d, &d->data);
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
    ar4callbacks.enablePairing();

    if ((millis() - procStart) < (CFG_BT_SCAN_DURATION * 900)) {
        Serial.println("Scan failed.");
        log("Scan failed. Rebooting.", ERROR);
        influxFlushBuffer(influxClient);
        long to = millis() + 10000;
        while (!influxClient->isBufferEmpty() && to < millis()) {
            task_sleep(1000);
        }
        ESP.restart();
    }

    pScan->clearResults();
    cleanupScannedDevices();

    influxFlushBuffer(influxClient);
}

int downloadHistory(Aranet4* ar4, AranetDevice* d, int newRecords) {
    int result = 0;
    AranetData adata;
    adata.ago = 0;
    adata.battery = d->data.battery;
    adata.interval = d->data.interval;

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
    long timestamp = tnow - (d->data.interval * newRecords);

    while (newRecords > 0 && ar4->isConnected()) {
        uint16_t logCount = CFG_HISTORY_CHUNK_SIZE;
        if (newRecords < CFG_HISTORY_CHUNK_SIZE) logCount = newRecords;

        // reset watchdog (1s per log, at least 30s)
        cancelWatchdog();
        startWatchdog(max((int) logCount, 30));

        uint8_t params = 0;
        uint8_t packing = -1;

        if (ar4->isAranet4()) {
            params = AR4_PARAM_FLAGS;
            packing = AR4_PACKING_ARANET4;
        } else if (ar4->isAranet2()) {
            params = AR2_PARAM_FLAGS;
            packing = AR4_PACKING_ARANET2;
        }

        Serial.printf("Will read %i results from %i @ %i\n", logCount, start, NimBLEDevice::getMTU());
        ar4->getHistory(start, logCount, logs, params);

        // Sometimes aranet might disconect, before full history is received
        // Set last update time to latest received timestamp;
        if (!ar4->isConnected()) {
            break;
        } else {
            start += logCount;
            newRecords -= logCount;
            d->pending = newRecords;
        }

        Serial.printf("Sending %i logs. %i remm\n", logCount, newRecords);

        result += logCount;

        for (uint16_t k = 0; k < logCount; k++) {
            if (k % 10 == 0) {
                Serial.print(k/10);
            } else {
                Serial.print(".");
            }

            adata.packing = packing;
            adata.co2 = logs[k].co2;
            adata.temperature = logs[k].temperature;
            adata.pressure = logs[k].pressure;
            adata.humidity = logs[k].humidity;

            Point pt = influxCreatePointWithTimestamp(&prefs, d, &adata, timestamp);
            influxSendPoint(influxClient, pt);
            timestamp += d->data.interval;
        }
        Serial.println();
        influxFlushBuffer(influxClient);
    }

    return result;
}
