#ifndef __INFLUX_H
#define __INFLUX_H

#include "../main.h"
#include "../types.h"

// https://github.com/tobiasschuerg/InfluxDB-Client-for-Arduino
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "Aranet4.h"

const char ilog_tags[] = {'A', 'E', 'W', 'I', 'D'};

InfluxDBClient* influxCreateClient(Preferences *prefs) {
  InfluxDBClient* influxClient = nullptr;

  if (prefs->getString(PREF_K_INFLUX_URL).length() < 1 || prefs->getString(PREF_K_INFLUX_BUCKET).length() < 1) {
      return influxClient;
  }

  if (prefs->getUChar(PREF_K_INFLUX_DBVER) == 2) {
    influxClient = new InfluxDBClient(
        prefs->getString(PREF_K_INFLUX_URL).c_str(),
        prefs->getString(PREF_K_INFLUX_ORG).c_str(),
        prefs->getString(PREF_K_INFLUX_BUCKET).c_str(),
        prefs->getString(PREF_K_INFLUX_TOKEN).c_str(),
        InfluxDbCloud2CACert
    );
  } else {
    Serial.print("InfluxDB: ");
    Serial.print(prefs->getString(PREF_K_INFLUX_URL));
    Serial.print(" -> ");
    Serial.println(prefs->getString(PREF_K_INFLUX_BUCKET));
    influxClient = new InfluxDBClient(prefs->getString(PREF_K_INFLUX_URL).c_str(), prefs->getString(PREF_K_INFLUX_BUCKET).c_str());
  }

  influxClient->setWriteOptions(WriteOptions().batchSize(MAX_BATCH_SIZE).bufferSize(WRITE_BUFFER_SIZE));
  return influxClient;
}

Point influxCreateStatusPoint(Preferences *prefs) {
    Point point("device_status");
    point.addTag("device", prefs->getString(PREF_K_SYS_NAME));
    point.addField("rssi", WiFi.RSSI());
    point.addField("uptime", millis());
    point.addField("heap_free", ESP.getFreeHeap());
    point.addField("heap_used", ESP.getHeapSize());
    return point;
}

Point influxCreatePoint(Preferences *prefs, AranetDevice* device, AranetData *data) {
    Point point("aranet");
    point.addTag("device", prefs->getString(PREF_K_SYS_NAME));
    point.addTag("name", device->name);

    if (data->packing == AR4_PACKING_ARANET2) {
        point.addField("temperature", data->temperature / 20.0);
        point.addField("humidity", data->humidity / 10.0);
    } else {
        point.addField("co2", data->co2);
        point.addField("temperature", data->temperature / 20.0);
        point.addField("pressure", data->pressure / 10.0);
        point.addField("humidity", data->humidity / 1.0);
    }
    point.addField("battery", data->battery);
    point.addField("interval", data->interval);
    point.addField("ago", data->ago);
    return point;
}

Point influxCreatePointWithTimestamp(Preferences *prefs, AranetDevice* device, AranetData *data, long timestamp) {
    Point point = influxCreatePoint(prefs, device, data);
    point.setTime(WRITE_PRECISION);
    return point;
}

bool influxSendPoint(InfluxDBClient *influxClient, Point pt) {
    if (influxClient != nullptr) {
        return influxClient->writePoint(pt);
    }
    return false;
}

void influxFlushBuffer(InfluxDBClient *influxClient) {
    if (influxClient != nullptr && !influxClient->isBufferEmpty()) {
        influxClient->flushBuffer();
    }
}

bool influxSendLog(InfluxDBClient *influxClient, Preferences *prefs, String str, ILog level) {
    uint16_t lvl = prefs->getUShort(PREF_K_INFLUX_LOG, 0);

    Serial.printf("%c: ", ilog_tags[(uint16_t) level]);
    Serial.println(str);

    if (lvl > 0 && lvl >= level) {
        if (str.length() > 0) {
            if (influxClient != nullptr) {
                Point point("log");
                point.addTag("device", prefs->getString(PREF_K_SYS_NAME));
                point.addField("message", str);
                point.addField("level", (uint16_t) level);
                point.setTime(WritePrecision::NoTime); // no time

                // bypass timestamp
                String line = influxClient->pointToLineProtocol(point);
                return influxClient->writeRecord(line);
            }
        }
    }
    return false;
}

#endif // __INFLUX_H