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

    bool isAp = getBootWiFiMode();
    bool started = startWebserver(isAp);

    if (started) {
        Serial.println("WiFi and Web server started");
    } else {
        Serial.println("Failed to start WiFi");
        return;
    }

    influxClient = influxCreateClient(&nodeCfg);

    Aranet4::init(ar4callbacks);

    delay(500);

    esp_task_wdt_init(15, false);
    enableCore0WDT();
    enableCore1WDT();
}

void loop() {
    if (nextReport < millis()) {
        nextReport = millis() + 30000; // 30s
        Point pt = influxCreateStatusPoint(&nodeCfg);
        influxSendPoint(influxClient, pt);
    }

    int count = ar4devices.size;

    for (int i=0; i<count; i++) {
        esp_ble_addr_type_t type = BLE_ADDR_TYPE_RANDOM;
        uint8_t retry = 0;
        AranetDeviceStatus* s = &ar4status[i];
        AranetDevice* d = s->device;

        long expectedUpdateAt = s->updated + ((s->data.interval - s->data.ago) * 1000);
        if (millis() < expectedUpdateAt) {
            // not yet for update
            continue;
        }

        // dev stub
        if (memcmp(d->name, "Aranet4 0000", 12) == 0) {
              type = BLE_ADDR_TYPE_PUBLIC;
        }
        
        Serial.print("Conencting to: ");
        Serial.println(d->name);
        
        Aranet4* ar4 = new Aranet4();
        ar4_err_t status = ar4->connect(d->addr, type);

        while (retry++ < 3 && status != AR4_OK) {
            ar4->disconnect();
            task_sleep(100);
            status = ar4->connect(d->addr, type);
        }

        if (status == AR4_OK) {
            Serial.print("Read OK: ");
            s->data = ar4->getCurrentReadings();
            Serial.printf("  CO2: %i\n  T:  %.2f\n", s->data.co2, s->data.temperature / 20.0);
            s->updated = millis();

            if (s->data.co2 != 0) {
                Point pt = influxCreatePoint(&nodeCfg, d, &s->data);
                influxSendPoint(influxClient, pt);
            }
        } else {
              Serial.printf("Failed: %i\n", status);
        }

        ar4->disconnect();
        task_sleep(500);
        delete(ar4);
    }

    Serial.print("mem: ");
    Serial.print(ESP.getFreeHeap());
    Serial.print(" / ");
    Serial.println(ESP.getHeapSize());
    
    task_sleep(1000);
}
