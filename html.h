#ifndef __AR4BR_HTML_H

const char* htmlHeader =
  "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\">"
  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
  "<meta http-equiv=\"X-UA-Compatible\" content=\"ie=edge\">"
  "<title>Aranet4-ESP32 Bridge</title>"
  "<style>*{margin:0 auto;padding:0;font-family:Helvetica,sans-serif}"
  "#hdr{background:#fff;color:#333333;font-size:20px;padding:16px 24px}"
  "#notes,#content{padding:16px 24px}"
  "#hdrr{float:right;}"
  "body{background:#f0f0f0;}"
  "h1{font-size:1.2em;font-weight:bold;padding:4px 0;border-bottom:solid 1px #aaa;margin:16px 0 8px 0}"
  "input:not([type='checkbox']){width:300px;height:20px;font-size:15px;}"
  "input{margin-bottom:8px;}"
  ".bt{margin-left:8px;font-size:0.6em;color:#aaa;}"
  ".imgbtn{padding:2px 4px;}"
  ".content{margin: 0 auto; max-width: 800px;}"
  "table,tr,th,td{border-collapse:collapse;border:solid 1px #bbb;margin: 16px 0;}td{padding:4px}"
  ".card{margin: 24px;background: #fff;font-color:#333;}"
  ".cardtop{height: 4px;background: #aaa;}"
  ".cardtop.en{background: #079851;}"
  ".cardbody{padding: 16px 24px;font-size: 18px;line-height: 24px;}"
  ".cardimg{vertical-align: middle;padding: 0 6px;}"
  ".cardtitle{vertical-align: middle;font-size:22px;padding: 16px 0}"
  ".cardfl{flex: 1;text-align: center;}"
  ".cardmsg{margin-top:16px;}"
  ".clickable:hover{cursor:pointer;}"
  ".co2{text-align: center;margin: 36px;}"
  ".co2>b{font-size: 64px;}"
  ".co2-none .co2-txt{color: #aaa}"
  ".co2-none>.cardtop{background: #aaa;}"
  ".co2-ok .co2-txt{color: #079851}"
  ".co2-ok>.cardtop{background: #079851;}"
  ".co2-warn .co2-txt{color: #f29401}"
  ".co2-warn>.cardtop{background: #f29401;}"
  ".co2-alert .co2-txt{color: #cf1e2e}"
  ".co2-alert>.cardtop{background: #cf1e2e;}"
  "</style>"
  "<script>"
  "function page(url) { window.location.href = url; }"
  "</script>"
  "</head>"
  "<body>"
  "<div id=\"hdr\"><div class=\"content\">"
  "Aranet<b>4</b> - ESP32 Bridge"
  "<small class=\"bt\">"
  " " VERSION_CODE " "
  "</small>"
  "<div id=\"hdrr\">" // start right
  "<a href=\"/\" class=\"imgbtn\"><img src=\"/img/home.png\" alt=\"Home\"></a>"
  "<a href=\"/settings\" class=\"imgbtn\"><img src=\"/img/settings.png\" alt=\"Settings\"></a>"
  "</div>" // end right
  "</div></div>"
  "<div class=\"content\">";

const char* htmlFooter = "</div></body></html>";

const char* cfgScript =
  "<script>"
  "function saveConfig() {"
  "document.getElementById(\"cfg\").submit();"
  "};"
  "function toggleStaticCfg(en) {"
  "let div = document.getElementById(\"ipcfg\");"
  "div.style.display = en ? \"block\" : \"none\";"
  "}"
  "let staticIpCb = document.getElementsByName(\""PREF_K_WIFI_IP_STATIC"\")[0];"
  "staticIpCb.onchange = function(e) {"
  "toggleStaticCfg(e.target.checked);"
  "};"
  "toggleStaticCfg(staticIpCb.checked);"
  "</script>";

const char* scanResultsHtml = 
  "<table id=\"results\">"
  "<tr><th>Name</th><th>RSSI</th><th>Address</th><th></th></tr>"
  "</table>";

const char* scanResultsScript = 
  "<script>"
  "var failed = 0;"

  "let table = document.getElementById(\"results\");"
  "let status = document.getElementById(\"status\");"

  "function fetchResults() {"
    "fetch(\"/scanresults\")"
        ".then(response => response.text()) "
        ".then((dataStr) => {"
            "let rows = table.rows;"
            "var i = rows.length;"
            "while (--i) {"
                "rows[i].parentNode.removeChild(rows[i]);"
            "}"
            "let lines = dataStr.split(\"\\n\");"
            "let running = lines[0] == \"running\";"
            "status.innerHTML = lines[0];"
            "lines.shift();"
            "for (let line of lines) {"
                "let parts = line.split(\";\");"
                "let row = table.insertRow();"
                "row.insertCell(0).innerHTML = parts[3];"
                "row.insertCell(1).innerHTML = parts[2];"
                "row.insertCell(2).innerHTML = parts[1];"
                "let txt4 = (parts[4] == 1) ? 'Paired' : running ? 'scanning...' : '<a href=\"#\" onclick=\"pairDevice(`' + parts[1] + '`);\">Pair device</a>';"
                "row.insertCell(3).innerHTML = txt4;"
            "}"
            "setTimeout(fetchResults, 1000);"
        "})"
        ".catch(function(err) {"
            "if (failed++ < 2) {"
                "console.log('fetch failed. retry.');"
                "setTimeout(fetchResults, 1000);"
            "} else {"
                "status.innerHTML = \"stopped (conn reset)\";"
            "}"
        "});"
  "}"

  "function pairDevice(devicemac) {"
      "fetch('/pair', {method: 'POST', headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: \"devicemac=\" + devicemac})"
      ".then(response => response.text())"
      ".then((dataStr) => {"
          "if (dataStr != 'OK') {"
              "alert(dataStr);"
              "return;"
          "}"
          "let pin = prompt(\"Enter PIN\");"
          "if (pin > 0) {"
              "fetch('/pair', {method: 'POST', headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: \"devicemac=\" + devicemac + \"&pin=\" + pin})"
              ".then(response => response.text())"
              ".then((dataStr) => {"
                  "alert(dataStr);"
                  "fetchResults();"
              "});"
          "}"
      "});"
  "}"

  "setTimeout(fetchResults, 100);"
  
  "</script>"
  ;

String printHtmlLabel(char *name, char* title) {
    char tmp[128];
    sprintf(tmp, "<label for=\"%s\">%s</label>",name,title);
    return String(tmp);
}

String printHtmlTextInput(char *name, char* title, String value, uint8_t maxlen) {
  char tmp[256];
  sprintf(tmp, "<br><input type=\"text\" name=\"%s\" maxlength=\"%i\" value=\"%s\"></br>", name, maxlen, value.c_str());
  return printHtmlLabel(name, title) + String(tmp);
}

String printHtmlNumberInput(char *name, char* title, uint16_t value, uint32_t max) {
  char tmp[256];
  sprintf(tmp, "<br><input type=\"number\" name=\"%s\" max=\"%i\" value=\"%i\"></br>", name, max, value);
  return printHtmlLabel(name, title) + String(tmp);
}

String printHtmlCheckboxInput(char *name, char* title, uint32_t value) {
  char tmp[256];
  sprintf(tmp, "<input type=\"checkbox\" name=\"%s\" %s>", name, value ? "checked" : "");
  return String(tmp) + printHtmlLabel(name, title) + String("<br>");
}

String printCard(String title, String body, String cardimg = "", String fn = "", uint8_t color = 0) {
  String card = "<div ";
  
  if (fn.length() > 0) {
    card += "class=\"card clickable\" onclick=\"" + fn + "\">";
  } else {
    card += "class=\"card\">";
  }

  String klass = "";
  if (color == 1) {
    klass += " en";
  }

  card += "<div class=\"cardtop" + klass+ "\"></div><div class=\"cardbody\"><div>";

  if (cardimg.length() > 0) {
    card += "<img src=\"" + cardimg + "\" class=\"cardimg\">";
  }

  card += "<span class=\"cardtitle\">" + title + "</span></div>";
  if (body.length() > 0) {
    card += "<div class=\"cardmsg\">" + body + "</div>";
  }
  card += "</div></div>";

  return card;
}

String printAranetCard(AranetDevice* device, int id) {
  String klass = "co2-none";
  if (device->data.co2 == 0) {
    klass = "co2-none";
  }else if (device->data.co2 < 1000) {
    klass = "co2-ok";
  } else if (device->data.co2 < 1400) {
    klass = "co2-warn";
  } else {
    klass = "co2-alert";
  }
  
  String card = "<div class=\"card " + klass + "\" data-id=\"" + String(id) + "\">";

  char buf0[6];
  char buf1[6];
  char buf2[6];
  char buf3[6];
  
  sprintf(buf0, "%i", device->data.co2);
  sprintf(buf1, "%.1f", device->data.temperature / 20.0);
  sprintf(buf2, "%i", device->data.humidity);
  sprintf(buf3, "%i", device->data.pressure);
  sprintf(buf3, "%i", device->data.pressure);
  sprintf(buf3, "%i", device->data.pressure);

  String batimg = "";
  
  if (device->data.battery > 90) { batimg = "100"; }
  else if (device->data.battery > 80) { batimg = "90"; }
  else if (device->data.battery > 70) { batimg = "80"; }
  else if (device->data.battery > 60) { batimg = "70"; }
  else if (device->data.battery > 50) { batimg = "60"; }
  else if (device->data.battery > 40) { batimg = "50"; }
  else if (device->data.battery > 30) { batimg = "40"; }
  else if (device->data.battery > 20) { batimg = "30"; }
  else if (device->data.battery > 10) { batimg = "20"; }
  else { batimg = "10"; }

  String btimg = "bluetooth";
  // 
  long updatedAgo = (millis() - device->updated) / 1000;
  if (updatedAgo > device->data.interval + 5) {
    btimg = "bluetoothred";
  }
  
  card += "<div class=\"cardtop " + klass + "\"></div>";
  card += "<div class=\"cardbody\">";
  card +=   "<div>";
  card +=     "<img src=\"/img/"+btimg+".png\" class=\"cardimg\">";
  card +=     "<span class=\"cardtitle\">" + String(device->name) + "</span>";
  card +=       "<span style=\"float:right;\">";
  card +=         "<img class=\"batt-val\" src=\"/img/battery_" + batimg + ".png\" class=\"cardimg\" title=\"" + String(device->data.battery) + "%\">";
  //card +=         "<img src=\"/img/graph.png\" class=\"cardimg\">";
  //card +=         "<img src=\"/img/settings_dash.png\" class=\"cardimg\">";
  card +=       "</span>";
  card +=     "</div>";
  card +=     "<div class=\"co2\">";
  card +=       "<img style=\"margin-right: 16px; height:32px;\" src=\"/img/co2.png\" alt=\"CO2\"><b class=\"co2-val co2-txt\"\">" + String(buf0) + "</b><span class=\"co2-txt\">ppm</span>";
  card +=     "</div>";
  card +=     "<div style=\"display: flex;\">";
  card +=       "<div class=\"cardfl\"><img src=\"/img/temp.png\" alt=\"Temperature\"><br><b class=\"temp-val\" style=\"font-size: 28px;\">" + String(buf1) + "</b>°C</div>";
  card +=       "<div class=\"cardfl\"><img src=\"/img/humidity.png\" alt=\"Humidity\"><br><b class=\"humi-val\" style=\"font-size: 28px;\">" + String(buf2) + "</b>%</div>";
  card +=       "<div class=\"cardfl\"><img src=\"/img/pressure.png\" alt=\"Pressure\"><br><b class=\"pres-val\" style=\"font-size: 28px;\">" + String(buf3) + "</b>hPa</div>";
  card +=     "</div>";
  card +=   "</div>";
  card += "</div>";

  return card;
}

String printHtmlIndex(std::vector<AranetDevice*> devices) {
  String page = String(htmlHeader);
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  int index = 0;
  for (AranetDevice* d : devices) {
    page += printAranetCard(d, index++);
  }

  page += "<div class=\"card clickable\" onclick=\"page('scan')\">";
  page +=   "<div class=\"cardtop\"></div>";
  page +=   "<div class=\"cardbody\">";
  page +=     "<img src=\"/img/plus.png\" class=\"cardimg\"> <span class=\"cardimg\">Add new device</span>";
  page +=   "</div>";
  page += "</div>";

  page += String("<script src=\"js/index.js\"></script>");

  page += String(htmlFooter);
  return page;
}

String printHtmlConfig(Preferences* prefs, bool updated = false) {
  String page = String(htmlHeader);

  page += "<form id=\"cfg\" method=\"post\">";

  char ipAddr[16];
  char netmask[16];
  char gateway[16];
  char dns[16];
  char mqttIpAddr[16];

  ip2str(prefs->getUInt(PREF_K_WIFI_IP_ADDR), ipAddr);
  ip2str(prefs->getUInt(PREF_K_WIFI_IP_MASK), netmask);
  ip2str(prefs->getUInt(PREF_K_WIFI_IP_GW), gateway);
  ip2str(prefs->getUInt(PREF_K_WIFI_IP_DNS), dns);
  ip2str(prefs->getUInt(PREF_K_MQTT_SERVER), mqttIpAddr);

  if (updated) {
    page += printCard(
      "Configuration updated", "",
      "/img/ok.png",
      "",
      1
    );
  }

  page += printCard("System", printHtmlTextInput(PREF_K_SYS_NAME, "Device Name", prefs->getString(PREF_K_SYS_NAME), 32)
                            + printHtmlTextInput(PREF_K_NTP_URL, "NTP Server", prefs->getString(PREF_K_NTP_URL), 47));

  page += printCard("Connectivity", printHtmlTextInput(PREF_K_WIFI_SSID, "Wi-Fi SSID", prefs->getString(PREF_K_WIFI_SSID), 32)
                                  + printHtmlTextInput(PREF_K_WIFI_PASSWORD, "Wi-Fi Password", prefs->getString(PREF_K_WIFI_PASSWORD), 63)
                                  + printHtmlCheckboxInput(PREF_K_WIFI_IP_STATIC, "Set static IP address", prefs->getBool(PREF_K_WIFI_IP_STATIC))
                                  + "<div id=\"ipcfg\">"
                                  + printHtmlTextInput(PREF_K_WIFI_IP_ADDR, "IP address", ipAddr, 15)
                                  + printHtmlTextInput(PREF_K_WIFI_IP_MASK, "Network mask", netmask, 15)
                                  + printHtmlTextInput(PREF_K_WIFI_IP_GW, "Gateway", gateway, 15)
                                  + printHtmlTextInput(PREF_K_WIFI_IP_DNS, "DNS", dns, 15)
                                  + "</div>");

  page += printCard("Influx DB", printHtmlTextInput(PREF_K_INFLUX_URL, "Url", prefs->getString(PREF_K_INFLUX_URL), 128)
                               + printHtmlTextInput(PREF_K_INFLUX_ORG, "Organisation", prefs->getString(PREF_K_INFLUX_ORG), 16)
                               + printHtmlTextInput(PREF_K_INFLUX_TOKEN, "Token", prefs->getString(PREF_K_INFLUX_TOKEN), 128)
                               + printHtmlTextInput(PREF_K_INFLUX_BUCKET, "Bucket/Database", prefs->getString(PREF_K_INFLUX_BUCKET), 32)
                               + printHtmlCheckboxInput(PREF_K_INFLUX_DBVER, "InfluxDB v2", prefs->getUChar(PREF_K_INFLUX_DBVER) == 2)
                               + printHtmlNumberInput(PREF_K_INFLUX_LOG, "Log level", prefs->getUShort(PREF_K_INFLUX_LOG), 4));

  page += printCard("MQTT Client", printHtmlTextInput(PREF_K_MQTT_SERVER, "Server IP address", mqttIpAddr, 15)
                               + printHtmlNumberInput(PREF_K_MQTT_PORT, "Port", prefs->getUShort(PREF_K_MQTT_PORT), 65535)
                               + printHtmlTextInput(PREF_K_MQTT_USER, "User", prefs->getString(PREF_K_MQTT_USER), 128)
                               + printHtmlTextInput(PREF_K_MQTT_PASSWORD, "Password", prefs->getString(PREF_K_MQTT_PASSWORD), 128));

  page += printCard(
    "Save", "",
    "/img/ok.png",
    "saveConfig()"
  );
  
  page += printCard(
    "Restart", "",
    "/img/restart.png",
    "page('/restart')"
  );

  page += printCard(
    "Remove bonded devices", "",
    "/img/restart.png",
    "page('/clrbnd')"
  );

  page += String("</form>");
  page += String(cfgScript);
  page += String(htmlFooter);

  return page;
}

String printScanPage() {
  String page = String(htmlHeader);
  page += printCard(
    "<b>Scan status: </b><span id=\"status\">unknown</span>",
    scanResultsHtml
  );
  page += String(scanResultsScript);
  page += String(htmlFooter);

  return page;
}

#endif
