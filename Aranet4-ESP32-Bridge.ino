/*
 *  Bluetooth + WiFi stack takes a lot of space.
 *  Most ESP32 boards uses 4MB SPI Flash and You will need to change 
 *  partition scheme to [No OTA (2MB APP/2MB SPIFFS)]
 *  
 *  Upload files to SPIFFS using https://github.com/me-no-dev/arduino-esp32fs-plugin/releases/
 *  
 *  Also make sure esp32 board library is up to date https://github.com/espressif/arduino-esp32
 *  As of writing this 1.0.6 is latest and tested.
 *  Upgrading from 1.0.4 fixed some BLE stack DRAM issues
 */

#include "main.h"
#include "esp_task_wdt.h"

long nextReport = 0;

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
    //setupMqttClient();

    startNtpSyncTask();

    // Set up bluettoth security and callbacks
    Aranet4::init();
    ar4.setConnectTimeout(CFG_BT_CONNECT_TIMEOUT);

    delay(500);

    esp_task_wdt_init(15, false);
    enableCore0WDT();
    enableCore1WDT();
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

    for (int j=0; j<count; j++) {
        AranetDeviceStatus* s = s = &ar4status[j];
        AranetDevice* d = d = s->device;

        long expectedUpdateAt = s->updated + ((s->data.interval - s->data.ago) * 1000);
        if(millis() < expectedUpdateAt && s->updated > 0) continue;

        Serial.print("Connecting to ");
        Serial.println(d->name);

        long disconnectTimeout = millis() + 1000;
        while (ar4.isConnected() && disconnectTimeout > millis()) task_sleep(10);

        ar4_err_t status = ar4.connect(d->addr);

        if (status == AR4_OK) {
            s->data = ar4.getCurrentReadings();
            Serial.printf("  CO2: %i\n  T:  %.2f\n", s->data.co2, s->data.temperature / 20.0);
            if (ar4.getStatus() == AR4_OK && s->data.co2 != 0) {
                // Check how many records might have been skipped
                long mStart = millis();
                int newRecords = s->pending;
                if (newRecords == 0) newRecords = (((mStart - s->updated) / 1000) / s->data.interval) - 1;

                if (s->updated != 0 && newRecords > 0) {
                    Serial.printf("I see missed %i records\n", newRecords);
                    AranetDataCompact* logs = (AranetDataCompact*) malloc(sizeof(AranetDataCompact) * CFG_HISTORY_CHUNK_SIZE);
                    AranetData adata;
                    adata.ago = 0;
                    adata.battery = s->data.battery;
                    adata.interval = s->data.interval;

                    int totalLogs = ar4.getTotalReadings();
                    int start = totalLogs - newRecords;
                    if (start < 1) start = 1;

                    // TODO: is timestamp in seconds or milliseconds?
                    time_t tnow = time(nullptr); // should be seconds
                    long timestamp = tnow - (s->data.interval * newRecords);

                    while (newRecords > 0 && ar4.isConnected()) {
                        uint16_t logCount = CFG_HISTORY_CHUNK_SIZE;
                        if (newRecords < CFG_HISTORY_CHUNK_SIZE) logCount = newRecords;

                        Serial.printf("Will read %i results from %i\n", logCount, start);
                        ar4.getHistory(start, logCount, logs);

                        // Sometimes aranet might disconect, before full history is received
                        // Set last update time to latest received timestamp;
                        if (!ar4.isConnected()) {
                            break;
                        } else {
                            start += logCount;
                            newRecords -= logCount;
                            s->pending = newRecords;
                        }

                        Serial.printf("Sending %i logs\n", logCount);

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
                }

                s->updated = millis();

                Serial.print("Uploading data ...");
                Point pt = influxCreatePoint(&prefs, d, &s->data);
                influxSendPoint(influxClient, pt);
                if (!s->mqttReported) {
                    mqttSendConfig(&mqttClient, &prefs, d);
                    s->mqttReported = true;
                }
                mqttSendPoint(&mqttClient, &prefs, d, &s->data);
            } else {
                Serial.print("Read failed.");       
            }
        } else {
            Serial.printf("Aranet4 connect failed: (%i)\n", status);
        }

        ar4.disconnect();
        Serial.println("Disconnected.");
    }
    influxFlushBuffer(influxClient);

    Serial.print("mem: ");
    Serial.print(ESP.getFreeHeap());
    Serial.print(" / ");
    Serial.println(ESP.getHeapSize());

    task_sleep(2500);
}
