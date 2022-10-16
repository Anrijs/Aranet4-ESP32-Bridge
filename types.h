#ifndef __AR4BR_TYPES_H
#define __AR4BR_TYPES_H

#include "config.h"
#include "Aranet4.h"
#include "utils.h"

enum DeviceType {
  UNKNOWN = 0,
  ARANET4 = 1
};

typedef struct AranetDevice {
    uint8_t addr[6];
    char name[24];
    bool paired;
    bool enabled;
    bool gatt;
    bool history;

    void saveConfig(DynamicJsonDocument &doc) {
        JsonArray devices;

        if (doc.containsKey("devices")) {
            devices = doc["devices"];
        } else {
            devices = doc.createNestedArray("devices");
        }

        char buf[24];
        mac2str(addr, buf, true);

        JsonObject device = devices.createNestedObject();
        device["mac"] = buf;
        device["name"] = name;

        JsonObject settings = device.createNestedObject("settings");
        settings["paired"] = paired;
        settings["enabled"] = enabled;
        settings["gatt"] = gatt;
        settings["history"] = history;
    }
};

// Saved device config
typedef struct AranetDeviceConfig {
    uint16_t version = CFG_DEV_VER;
    uint16_t size = 0;
    uint8_t _reserved[32]; 
    AranetDevice devices[CFG_MAX_DEVICES];
};

typedef struct AranetDeviceStatus {
    AranetDevice* device;
    AranetData data;
    long updated = 0;
    uint16_t pending = 0;
    bool mqttReported = false;
};

// scanned device info
typedef struct ScanCache {
    uint64_t umac = 0;
    int rssi = -1;
    char name[24];
    DeviceType type;
    long lastSeen = 0;
    uint8_t connectable : 1,
            beacon      : 1;
};

#endif
