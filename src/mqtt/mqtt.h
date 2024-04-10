#ifndef __MQTT_H
#define __MQTT_H

#include "../main.h"
#include "../types.h"

// https://github.com/arduino-libraries/ArduinoMqttClient/
#include <ArduinoMqttClient.h>

#include "Aranet4.h"


const char* mqttConfigTemplate = "{\"device_class\": \"%s\", \"name\": \"%s %s\", \"state_topic\": \"%s\", \"unit_of_measurement\": \"%s\", \"uniq_id\":\"sensor.%s\"}";


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

    String topic = "aranet4bridge/sensor/" + mqttGetAranetName(device);

    if (data->type == AranetType::ARANET2) {
        client->beginMessage(topic + "/temperature");
        client->print(data->temperature / 20.0);
        client->endMessage();

        client->beginMessage(topic + "/humidity");
        client->print(data->humidity / 10.0);
        client->endMessage();
    } else if (data->type == AranetType::ARANET4) {
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
    }

    client->beginMessage(topic + "/battery");
    client->print(data->battery);
    client->endMessage();
}

/*
    Send home asssistant compatible config to mqtt server
*/

void mqttSendConfig(MqttClient* client, Preferences *prefs, AranetDevice* device) {
    if (!client->connected()) mqttConnect(client, prefs);

    String deviceNameStr = mqttGetAranetName(device);
    String topic = "aranet4bridge/sensor/" + deviceNameStr;
    const char* deviceName = deviceNameStr.c_str();


    char buf[256];

    sprintf(buf, mqttConfigTemplate, "carbon_dioxide", deviceName, "CO2", (topic + "/co2").c_str(), "ppm", (deviceNameStr + ".co2").c_str(),(deviceNameStr + ".co2").c_str());
    client->beginMessage(topic + "-co2/config");
    client->print(buf);
    client->endMessage();

    sprintf(buf, mqttConfigTemplate, "temperature", deviceName, "Temperature", (topic + "/temperature").c_str(), "C", (deviceNameStr + ".temperature").c_str(),(deviceNameStr + ".temperature").c_str());
    client->beginMessage(topic + "-t/config");
    client->print(buf);
    client->endMessage();

    sprintf(buf, mqttConfigTemplate, "pressure", deviceName, "Pressure", (topic + "/pressure").c_str(), "hPa", (deviceNameStr + ".pressure").c_str(),(deviceNameStr + ".pressure").c_str());
    client->beginMessage(topic + "-p/config");
    client->print(buf);
    client->endMessage();

    sprintf(buf, mqttConfigTemplate, "humidity", deviceName, "Humidity", (topic + "/humidity").c_str(), "%", (deviceNameStr + ".humidity").c_str(),(deviceNameStr + ".humidity").c_str());
    client->beginMessage(topic + "-h/config");
    client->print(buf);
    client->endMessage();

    sprintf(buf, mqttConfigTemplate, "battery", deviceName, "Battery", (topic + "/battery").c_str(), "%", (deviceNameStr + ".battery").c_str(), (deviceNameStr + ".battery").c_str());
    client->beginMessage(topic + "-b/config");
    client->print(buf);
    client->endMessage();
}


#endif