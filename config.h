#ifndef __AR4BR_CONFIG_H
#define __AR4BR_CONFIG_H

#define VERSION_CODE "v1.0 &#128640;"

// Config defaults
#define CFG_DEF_NAME "Aranet4-ESP-bridge"
#define CFG_DEF_URL "http://10.0.1.100:8086"
#define CFG_DEF_NTP "time.nist.gov"

#define CFG_VER 2

#define CFG_BT_SCAN_DURATION 5 // seconds

#define CFG_DEV_VER 1
#define CFG_MAX_DEVICES 4
#define CFG_DEVICE_OFFSET 512 // eeprom byte offset from node cfg

const char *ssid = "Aranet4-ESP32 Bridge";
const char *password = "Ar@net4Br1dge";
const char* www_username = "admin";

// influxdb
#define WRITE_BUFFER_SIZE 30

#endif
