#ifndef __AR4BR_CONFIG_H
#define __AR4BR_CONFIG_H

#define VERSION_CODE "v1.0 &#128640;"

// Config defaults
#define CFG_DEF_NAME "Aranet4-ESP-bridge"
#define CFG_DEF_URL "http://10.0.1.100:8086"
#define CFG_DEF_NTP "time.nist.gov"

#define CFG_VER 2

#define CFG_BT_SCAN_DURATION    5 // seconds
#define CFG_BT_CONNECT_TIMEOUT  5 // seconds
#define CFG_BT_TIMEOUT_DELAY   15 // seconds

#define CFG_DEV_VER 1
#define CFG_MAX_DEVICES 4
#define CFG_DEVICE_OFFSET 512 // eeprom byte offset from node cfg

#define CFG_HISTORY_CHUNK_SIZE 120

#define CFG_NTP_SYNC_INTERVAL 60 // (minutes)

#define CFG_DEF_LOGIN_USER "admin"
#define CFG_DEF_LOGIN_PASSWORD ""

const char *ssid = "Aranet4-ESP32 Bridge";
const char *password = "Ar@net4Br1dge";

// influxdb
#define WRITE_BUFFER_SIZE 30
#define WRITE_PRECISION WritePrecision::S

// mqtt
#define CFG_DEF_MQTT_PORT 1883

// Keystore keys
#define PREF_K_SYS_NAME       "sys_name"

#define PREF_K_LOGIN_USER     "sys_user"
#define PREF_K_LOGIN_PASSWORD "sys_password"

#define PREF_K_NTP_URL        "ntp_url"

#define PREF_K_WIFI_SSID      "wifi_ssid"
#define PREF_K_WIFI_PASSWORD  "wifi_password"

#define PREF_K_WIFI_IP_STATIC "wifi_ip_static"
#define PREF_K_WIFI_IP_ADDR   "wifi_ip_addr"
#define PREF_K_WIFI_IP_MASK   "wifi_ip_mask"
#define PREF_K_WIFI_IP_GW     "wifi_ip_gw"
#define PREF_K_WIFI_IP_DNS    "wifi_ip_dns"

#define PREF_K_INFLUX_URL     "influx_url"
#define PREF_K_INFLUX_ORG     "influx_org"
#define PREF_K_INFLUX_TOKEN   "influx_token"
#define PREF_K_INFLUX_BUCKET  "influx_bucket"
#define PREF_K_INFLUX_DBVER   "influx_dbver"

#define PREF_K_MQTT_SERVER    "mqtt_server_ip"
#define PREF_K_MQTT_PORT      "mqtt_port"
#define PREF_K_MQTT_USER      "mqtt_user"
#define PREF_K_MQTT_PASSWORD  "mqtt_password"

#endif
