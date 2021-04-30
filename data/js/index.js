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
      let card = document.querySelector(`[data-id*="` + uid + `"]`);

      if (!card) {
        continue;
      }

      let co2 = pt[3];
      let batt = pt[7];

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
      if (pt[10] > (parseInt(pt[8])+5)) {
        btimg = "bluetoothred";
        klass = "co2-none";
      }
      card.getElementsByClassName("cardimg")[0].src = "/img/" + btimg + ".png";
      card.getElementsByClassName("batt-val")[0].src = "/img/battery_" + batimg + ".png";
      card.getElementsByClassName("co2-txt")[0].textContent = co2;
      card.getElementsByClassName("temp-val")[0].textContent = pt[4];
      card.getElementsByClassName("humi-val")[0].textContent = pt[6];
      card.getElementsByClassName("pres-val")[0].textContent = pt[5];
      card.className="card " + klass;

      let u = pt[8] - pt[9];
      if (u<nextUpdate)nextUpdate=u;
    }
    setTimeout(updateCards, (nextUpdate+10)*1000);
  });
}

setTimeout(updateCards, 5000);