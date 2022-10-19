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
    "table{width:100%}"
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

const char* savedDevicesHtml =
    "<table>"
    "<tr><th>Address</th><th>Name</th><th>RSSI</th><th>Last seen</th><th></th></tr>"
    "<tbody id=\"saved\"></tbody>"
    "</table>";

const char* newDevicesHtml =
    "<table>"
    "<tr><th>Address</th><th>Name</th><th>RSSI</th><th>Last seen</th><th></th></tr>"
    "<tbody id=\"new\"></tbody>"
    "</table>";

const char* indexScript = R"(
    <script>
    function updateCards() {
        fetch("/data")
            .then(response => response.text())
            .then((dataStr) => {
            let lines = dataStr.split("\n");
            var nextUpdate = 300;
            for (let ln of lines) {
                let pt = ln.split(";");
                if (pt.length < 11) {
                    continue;
                }

                let uid = pt[0];
                let en = pt[1];

                if (!en.includes("e")) continue;

                let card = document.querySelector(`[data-id*="` + uid + `"]`);

                if (!card) continue;

                let co2 = pt[4];
                let batt = pt[8];

                var klass = "co2-none";
                if (co2 == 0) kalss="co2-none";
                else if (co2 < 1000) klass="co2-ok";
                else if (co2 < 1400) klass="co2-warn";
                else klass = "co2-alert";

                var batimg = "";

                if (batt > 90) batimg ="100";
                else if (batt > 80) batimg ="90";
                else if (batt > 70) batimg ="80";
                else if (batt > 60) batimg ="70";
                else if (batt > 50) batimg ="60";
                else if (batt > 40) batimg ="50";
                else if (batt > 30) batimg ="40";
                else if (batt > 20) batimg ="30";
                else if (batt > 10) batimg ="20";
                else batimg = "10";
                var btimg = "bluetooth";
                if (pt[11] > (parseInt(pt[9])+5)) {
                    btimg = "bluetoothred";
                    klass = "co2-none";
                }
                card.getElementsByClassName("cardimg")[0].src = "/img/" + btimg + ".png";
                card.getElementsByClassName("batt-val")[0].src = "/img/battery_" + batimg + ".png";
                card.getElementsByClassName("batt-val")[0].title = batt + "%";
                card.getElementsByClassName("co2-txt")[0].textContent = co2;
                card.getElementsByClassName("temp-val")[0].textContent = pt[5];
                card.getElementsByClassName("humi-val")[0].textContent = pt[7];
                card.getElementsByClassName("pres-val")[0].textContent = pt[6];
                card.className="card " + klass;

                let u = pt[9] - pt[11]; // 9 = interval, 10 = ago, 11 = real ago
                if (u<nextUpdate)nextUpdate=u;
            }
            setTimeout(updateCards, (nextUpdate+10)*1000);
            });
    }

    setTimeout(updateCards, 5000);
    </script>
)";

const char* devicesScript = R"(
    <script>
        var failed = 0;
        const savedDevices = document.getElementById("saved");
        const newDevices = document.getElementById("new");

        function addNewDevice(parts) {
            //	Address	 Name  RSSI	Last seen  []
            let row = newDevices.insertRow();

            row.insertCell(0).innerHTML = parts[1];
            row.insertCell(1).innerHTML = parts[2];
            row.insertCell(2).innerHTML = parts[3];
            row.insertCell(3).innerHTML = parts[4] / 1000 + 's ago';
            row.insertCell(4).innerHTML = `<button onclick="addDevice('${parts[1]}');">Add device</button>`;
        }

        function addSavedDevice(parts) {
            // Enabled	Address   Name	RSSI	Last seen   []
            let row = savedDevices.insertRow();

            row.insertCell(0).innerHTML = parts[1];
            row.insertCell(1).innerHTML = parts[2];
            row.insertCell(2).innerHTML = parts[3];
            row.insertCell(3).innerHTML = parts[4] / 1000 + 's ago';


            let utils = "";
            if (!parts[5].includes("p")) {
                utils += '<button onclick="pairDevice(`' + parts[1] + '`);">Pair device</button><br>';
            } else {
                utils += `<label><input type="checkbox" checked disabled> Paired </label><br>`;
            }

            let chk = parts[5].includes("e") ? "checked" : "";
            utils += `<label><input ${chk} type="checkbox" onclick="toggleParam('enabled', '${parts[1]}', '${chk}') "> Enabled </label><br>`;

            chk = parts[5].includes("g") ? "checked" : "";
            utils += `<label><input ${chk} type="checkbox" onclick="toggleParam('gatt', '${parts[1]}', '${chk}') "> GATT </label><br>`;

            chk = parts[5].includes("h") ? "checked" : "";
            utils += `<label><input ${chk} type="checkbox" onclick="toggleParam('history', '${parts[1]}', '${chk}') "> History </label><br>`;

            row.insertCell(4).innerHTML = utils;
        }

        function addDevice(devicemac) {
            fetch("/devices_add", { method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded" }, body: "devicemac=" + devicemac })
                .then((response) => response.text())
                .then((dataStr) => {
                    if (dataStr != "OK") {
                        alert(dataStr);
                        return;
                    } else {
                        // refresh now
                        clearTimeout(refreshTimeout);
                        fetchResults();
                    }
                });
        }

        function toggleParam(param, devicemac, state) {
            let val = state == "checked" ? 0 : 1; // invert
            fetch("/devices_set", { method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded" }, body: "devicemac=" + devicemac + "&" + param + "=" + val })
                .then((response) => response.text())
                .then((dataStr) => {
                    if (dataStr != "OK") {
                        alert(dataStr);
                        return;
                    } else {
                        // refresh now
                        clearTimeout(refreshTimeout);
                        fetchResults();
                    }
                });
        }

        function fetchResults() {
            fetch("/devices_list")
                .then((response) => response.text())
                .then((dataStr) => {
                    // split new and saved
                    let groups = dataStr.split("#");

                    // clear tables
                    savedDevices.innerHTML = "";
                    newDevices.innerHTML = "";

                    for (let i=0;i<groups.length;i++) {
                        let g = groups[i];
                        if (!g) continue;

                        let lines = g.split("\n");
                        if (!lines) continue;

                        let gname = lines[0];
                        lines.shift();

                        for (let line of lines) {
                            let parts = line.split(";");
                            if (parts.length < 6) continue;
                            if (gname == "new") addNewDevice(parts);
                            else if (gname == "saved") addSavedDevice(parts);
                        }
                    }

                    refreshTimeout = setTimeout(fetchResults, 2000);
                })
                .catch(function (err) {
                    if (failed++ < 2) {
                        console.log("fetch failed. retry.");
                        console.log(err);
                        refreshTimeout = setTimeout(fetchResults, 2000);
                    } else {
                        console.log("fetch failed.");
                    }
                });
        }

        function pairDevice(devicemac) {
            fetch("/devices_pair", { method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded" }, body: "devicemac=" + devicemac })
                .then((response) => response.text())
                .then((dataStr) => {
                    if (dataStr != "OK") {
                        alert(dataStr);
                        return;
                    }
                    let pin = prompt("Enter PIN");
                    if (pin > 0) {
                        fetch("/devices_pair", { method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded" }, body: "devicemac=" + devicemac + "&pin=" + pin })
                            .then((response) => response.text())
                            .then((dataStr) => {
                                alert(dataStr);
                                fetchResults();
                            });
                    }
                });
        }

        let refreshTimeout = setTimeout(fetchResults, 100);
            </script>
)";

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
    char buf3[8];

    sprintf(buf0, "%i", device->data.co2);
    sprintf(buf1, "%.1f", device->data.temperature / 20.0);
    sprintf(buf2, "%i", device->data.humidity);
    sprintf(buf3, "%.1f", device->data.pressure / 10.0);

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

    page += "<div class=\"card clickable\" onclick=\"page('devices')\">";
    page +=   "<div class=\"cardtop\"></div>";
    page +=   "<div class=\"cardbody\">";
    page +=     "<img src=\"/img/plus.png\" class=\"cardimg\"> <span class=\"cardimg\">Manage devices</span>";
    page +=   "</div>";
    page += "</div>";

    page += String(indexScript);
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

String printDevicesPage() {
    String page = String(htmlHeader);
    page += printCard(
        "Saved devices",
        savedDevicesHtml
    );
        page += printCard(
        "Discovered devices",
        newDevicesHtml
    );
    page += String(devicesScript);
    page += String(htmlFooter);

    return page;
}

#endif
