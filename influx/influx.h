#ifndef __INFLUX_H
#define __INFLUX_H

#include "../main.h"
#include "../types.h"

// https://github.com/tobiasschuerg/InfluxDB-Client-for-Arduino
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "Aranet4.h"

InfluxDBClient* influxCreateClient(NodeConfig *nodeCfg) {
  InfluxDBClient* influxClient = nullptr;

    if (nodeCfg->dbver == 2) {
        influxClient = new InfluxDBClient(nodeCfg->url, nodeCfg->organisation, nodeCfg->bucket, nodeCfg->token, InfluxDbCloud2CACert);
    } else {
        influxClient = new InfluxDBClient(nodeCfg->url, nodeCfg->bucket);
    }
    influxClient->setWriteOptions(WriteOptions().writePrecision(WRITE_PRECISION).bufferSize(WRITE_BUFFER_SIZE));
    return ;
}

Point influxCreateStatusPoint(NodeConfig *nodeCfg) {
    Point point("device_status");
    point.addTag("device", nodeCfg->name);
    point.addField("rssi", WiFi.RSSI());
    point.addField("uptime", millis());
    point.addField("heap_free", ESP.getFreeHeap());
    point.addField("heap_used", ESP.getHeapSize());
    return point;
}

Point influxCreatePoint(NodeConfig *nodeCfg, AranetDevice* device, AranetData *data) {
    Point point("aranet4");
    point.addTag("device", nodeCfg->name);
    point.addTag("name", device->name);
    point.addField("co2", data->co2);
    point.addField("temperature", data->temperature / 20.0);
    point.addField("pressure", data->pressure / 10.0);
    point.addField("humidity", data->humidity);
    point.addField("battery", data->battery);
    point.addField("interval", data->interval);
    point.addField("ago", data->ago);
    return point;
}

Point influxCreatePointWithTimestamp(NodeConfig *nodeCfg, AranetDevice* device, AranetData *data, long timestamp) {
    Point point = influxCreatePoint(nodeCfg, device, data);
    point.setTime(WRITE_PRECISION);
    point.setTime(timestamp);
    return point;
}

void influxSendPoint(InfluxDBClient *influxClient, Point pt) {
    if (influxClient) {
        influxClient->writePoint(pt);
    }
}

void influxFlushBuffer(InfluxDBClient *influxClient) {
    if (influxClient && !influxClient->isBufferEmpty()) {
        influxClient->flushBuffer();
    }
}

#endif // __INFLUX_H