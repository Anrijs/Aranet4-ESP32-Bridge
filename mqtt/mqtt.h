#ifndef __MQTT_H
#define __MQTT_H

#include "../main.h"
#include "../types.h"

// https://github.com/arduino-libraries/ArduinoMqttClient/
#include <ArduinoMqttClient.h>

#include "Aranet4.h"


String mqttGetDeviceId() {
    byte mac[6];
    WiFi.macAddress(mac);
    char buf[28];
    sprintf(buf, "aranet4bridge-%02X%02X%02X",
        mac[3],mac[4],mac[5]
    );

    return String(buf);
}

String mqttGetAranetName(AranetDevice* device) {
    char devname[24];
    memcpy(devname, device->name, 24);

    for (uint8_t i=0; i < 24; i++) {
        char c = devname[i];
        if (c == 0) break;
        else if (c == ' ') devname[i] = '-';
        else if (c >= 'A' && c <= 'Z') {
            devname[i] = c + 32;
        }
    }

    return String(devname);
}

/*
   Connectto mqtt server
*/
int mqttConnect(MqttClient* client, Preferences* prefs) {
    String usr = prefs->getString(PREF_K_MQTT_USER);
    String pwd = prefs->getString(PREF_K_MQTT_PASSWORD);

    client->setUsernamePassword(usr,pwd);
    client->setId(mqttGetDeviceId());

    uint16_t port = prefs->getUShort(PREF_K_MQTT_PORT, CFG_DEF_MQTT_PORT);
    uint32_t addr = prefs->getUInt(PREF_K_MQTT_SERVER);
    if (addr != 0 && port != 0) {
        if (addr != 0) {
            char mqttIpAddr[16];
            ip2str(addr, mqttIpAddr);
            return client->connect(mqttIpAddr, port);
        }
    }
    return 0;
}

/*
    Send point to mqtt server
*/
void mqttSendPoint(MqttClient* client, Preferences *prefs, AranetDevice* device, AranetData *data) {
    if (!client->connected()) mqttConnect(client, prefs);

    String topic = "aranet4bridge/" + mqttGetAranetName(device);

    client->beginMessage(topic + "/co2");
    client->print(data->co2);
    client->endMessage();

    client->beginMessage(topic + "/temperature");
    client->print(data->temperature / 20.0);
    client->endMessage();

    client->beginMessage(topic + "/pressure");
    client->print(data->pressure / 10.0);
    client->endMessage();

    client->beginMessage(topic + "/humidity");
    client->print(data->humidity);
    client->endMessage();

    client->beginMessage(topic + "/battery");
    client->print(data->battery);
    client->endMessage();
}

#endif