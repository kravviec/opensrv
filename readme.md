Info: English below

# OpenSRV (DE)

Ziel von OpenSRV soll es sein, im Wohnmobil oder Wohnwagen, Smarthome-ähnliche Funktionen nachzurüsten.

## Geplante Funktionen

Teils schon umgesetzt, teils noch in Planung sind:

* Temperatur
* Luftfeuchtigkeit
* GPS
* automatische Dachfenster-Lüfter-Steuerung

## Projektseite

Auf meiner Webseite dokumentiere ich diverse Fortschritte: [OpenSRV auf CampingTech.de](https://campingtech.de/opensrv)

Wenn Du mitwirken willst oder schneller Informationen haben möchtest, dann schreibe dich kostenlos in meiner Facebook-Gruppe ein: [OpenSRV-Facebook-Gruppe](https://www.facebook.com/groups/opensrv/)

## Block-Diagramm

Version 1.1

![Block-Diagramm V1.1 von OpenSRV](https://github.com/rbrixel/opensrv/blob/master/OpenSRV-block-diagram.jpg)

## Erklärung der einzelnen Nodes

Diverse Funktionen und Programmteile teile ich auf unterschiedliche Hardware aus. Teils aus Mangel an Ports, teils aus Kostengründen und teils aus Positionierung im Fahrzeug.

### Node01

**Hardware**

ESP8266 (Wemos D1 mini Klon)

**Sensoren**

* BME280 (Temperatur, Luftfeuchigkeit, Luftdruck)
* ds18b20 (Temperatur, wasserdicht)
* ublox Neo-6m (GPS, Datum, Uhrzeit, Höhe über Null)

---

# OpenSRV (EN)

The aim of OpenSRV should be to retrofit smart home-like functions in mobile homes or caravans/rv.

## Planned features

Some have already been implemented, some are still being planned:

* Temperature
* Humidity
* GPS
* Automatic roof window fan control

## project page

On my website I document various progress: [OpenSRV on CampingTech.de] (https://campingtech.de/opensrv) (only german)

If you want to participate or have information faster, then sign up for free in my Facebook group: [OpenSRV-Facebook group] (https://www.facebook.com/groups/opensrv/) (all languages are welcome)

## Block diagram

Only in german. Sorry!

## Explanation of the individual nodes

I distribute various functions and program parts to different hardware. Partly due to lack of ports, partly due to cost reasons and partly due to positioning in the vehicle.

### Node01

*Hardware*
ESP8266 (Wemos D1 mini clone)

* Sensors *
* BME280 (temperature, humidity, air pressure)
* ds18b20 (temperature, waterproof)
* ublox Neo-6m (GPS, date, time, altitude above zero)