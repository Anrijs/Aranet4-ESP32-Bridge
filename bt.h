#ifndef __AR4BR_BT_H
#define __AR4BR_BT_H

#include "Aranet4.h"
#include "utils.h"

#define DEVICE_NAME_MAX_LEN  18

typedef struct BtAdvertisement {
    uint8_t addr[ESP_BD_ADDR_LEN];
    char name[DEVICE_NAME_MAX_LEN]; // with 0 at end
    int rssi;
};

static void __attribute__((unused)) remove_all_bonded_devices(void) {
    int dev_num = esp_ble_get_bond_device_num();
  
    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);
    for (int i = 0; i < dev_num; i++) {
        esp_ble_remove_bond_device(dev_list[i].bd_addr);
    }
  
    free(dev_list);
}

class MyAranet4Callbacks: public Aranet4Callbacks {
    uint32_t pin = -1;

    uint32_t onPinRequested() {
        Serial.println("PIN Requested. Waiting for web or timeout");
        long timeout = millis() + 30000;
        while (pin == -1 && timeout > millis())
            vTaskDelay(500 / portTICK_PERIOD_MS);

        uint32_t ret = pin;
        pin = -1;

        return  ret;
    }
public:
    bool providePin(uint32_t newpin) {
        if (pin == -1) {
            pin = newpin;
            return true;
        }

        return false;
    }
};

class BtScanAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
private:
    bool running = false;
    bool aborted = false;
    std::vector<BtAdvertisement> advertisements;

    /**
       Called for each advertising BLE server.
    */
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        Serial.print("BLE Advertised Device found:: ");
        Serial.println(advertisedDevice.toString().c_str());

        bool hasSrvc = advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(UUID_Aranet4);
        bool hasName = advertisedDevice.getName().find(std::string("Aranet4")) != std::string::npos;

        if (hasSrvc || hasName) {
            esp_bd_addr_t* addr = advertisedDevice.getAddress().getNative();

            // Check if exists
            for (BtAdvertisement a : advertisements) {
                if (memcmp(addr, a.addr, ESP_BD_ADDR_LEN) == 0) {
                    a.rssi = advertisedDevice.getRSSI();
                    return;
                }
            }
            BtAdvertisement adv;
            std::string name = advertisedDevice.getName();
            int len = name.length();
            if (len > DEVICE_NAME_MAX_LEN) len = DEVICE_NAME_MAX_LEN;
            memcpy(adv.name, name.c_str(), len);
            adv.name[len] = 0;

            memcpy(adv.addr, addr, ESP_BD_ADDR_LEN);

            adv.rssi = advertisedDevice.getRSSI();
            advertisements.push_back(adv);
        }
    }
public:
    bool isRunning() {
      return running;
    }

    void setRunning(bool status) {
        aborted = false;
        running = status;
        Serial.print("Set running: ");
        Serial.println(status);
    }
    void clearResults() {
        advertisements.clear();
    }

    void abort() {
        BLEDevice::getScan()->stop();
        aborted = true;
        Serial.println("SCAN ABORT");
        running = false;
    }

    String printDevices() {
        String page = aborted ? String("aborted") : running ? String("running") : String("stopped");
        char buf[DEVICE_NAME_MAX_LEN];

        int pos = 0;
        for (BtAdvertisement a : advertisements) {
            mac2str(a.addr, buf, false);
            page += "\n";
            page += String(pos++);
            page += ";";
            page += String(buf);
            page += ";";
            page += String(a.rssi);
            page += ";";
            page += String(a.name);
            page += ";";
            page += Aranet4::isPaired(a.addr) ? "1" : "0";
        }
      return page;
    }

    int getDeviceCount() {
        return advertisements.size();
    }

    void getAddress(int deviceid, uint8_t* addr) {
        memcpy(addr, &advertisements.at(deviceid).addr, ESP_BD_ADDR_LEN);
    }
};

#endif
