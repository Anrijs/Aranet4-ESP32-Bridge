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
InfluxDBClient* influxClient = nullptr;

void setup() {
    Serial.begin(115200);
    Serial.println("Setup");

    pinMode(MODE_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT); // green LED

    if(!SPIFFS.begin()){
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
    
    //wipe(CFG_DEVICE_OFFSET);

    configLoad();
    devicesLoad();

    isAp = getBootWiFiMode();

    if (!setupWifiAndWebserver()) {
         Serial.println("Failed to initialize.");
         return;
    }

    // Start NTP sync task
    xTaskCreatePinnedToCore(
        NtpSyncTaskCode,  /* Task function. */
        "NtpSyncTask",    /* name of task. */
        2048,             /* Stack size of task */
        NULL,             /* parameter of the task */
        1,                /* priority of the task */
        &NtpSyncTask,     /* Task handle to keep track of created task */
        0);               /* pin task to core 0 */

    influxClient = influxCreateClient(&nodeCfg);

    // Set up bluettoth security and callbacks
    Aranet4::init();

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
        Point pt = influxCreateStatusPoint(&nodeCfg);
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
        if(millis() < expectedUpdateAt) continue;

        Serial.print("Connecting to ");
        Serial.println(d->name);

        ar4_err_t status = ar4.connect(d->addr);

        if (status == AR4_OK) {
            s->data = ar4.getCurrentReadings();
            Serial.printf("  CO2: %i\n  T:  %.2f\n", s->data.co2, s->data.temperature / 20.0);
            if (ar4.getStatus() == AR4_OK && s->data.co2 != 0) {
                s->updated = millis();

                Serial.print("Uploading data ...");
                Point pt = influxCreatePoint(&nodeCfg, d, &s->data);
                influxSendPoint(influxClient, pt);
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
