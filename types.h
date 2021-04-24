#ifndef __AR4BR_TYPES_H
#define __AR4BR_TYPES_H

#include "config.h"
#include "Aranet4.h"

#pragma pack(push, 1)
typedef struct NodeConfig { // 1:1 with eeprom bytes
    uint16_t version = CFG_VER;

    // system
    char name[65];

    // connectivity
    char ssid[33];
    char password[33];

    uint8_t  extras =  0;

    // Static IP settings
    uint32_t ip_addr = 0;
    uint32_t netmask = 0;
    uint32_t gateway = 0;
    uint32_t dns = 0;

    // influx
    uint8_t dbver = 2;
    char url[129];
    char organisation[17];
    char token[129];
    char bucket[33];
} NodeConfig;
#pragma pack(pop)

// Saved device container
#pragma pack(push, 1)
typedef struct AranetDevice {
    uint8_t addr[6];
    char name[24];
};
#pragma pack(pop)

// Saved device config
#pragma pack(push, 1)
typedef struct AranetDeviceConfig {
    uint16_t version = CFG_DEV_VER;
    uint16_t size = 0;
    uint8_t _reserved[32]; 
    AranetDevice devices[CFG_MAX_DEVICES];
};
#pragma pack(pop)

typedef struct AranetDeviceStatus {
    AranetDevice* device;
    AranetData data;
    long updated = 0;
};

#endif
