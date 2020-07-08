/*
 * OPENSRV-NODE01
 * --------------
 * 
 * EN: ESP-Temperature-, Humidity-, Airpressure- and GPS-Module
 * DE: ESP-Temperatur-, Luftfeuchtigkeits-, Luftdruck- und GPS-Modul
 * 
 * Hardware: 
 * - ESP-12F (ESP8266, "Wemos D1 Mini"-Clone)
 * - BME280 (temperature, humidity and air pressure)
 * - DS18B20 (waterproof for outdoor measurement)
 * - ublox Neo-6M (GPS, date, time)
 * 
 * Software:
 * - wifi8266 (integrated in esp8266)
 * - EspMQTTClient (aka PubSubClient) 1.8.0
 * - Over-The-Air-Update (ArduinoOTA)
 * - OneWire (OneWire - use 2.3.0 for GPIO16/D0!)
 * - TinyGPSPlus-1.0.2b (http://arduiniana.org/libraries/tinygpsplus/)
 * - Cactus.io BME280 (http://cactus.io/projects/weather/arduino-weather-station-bme280-sensor)
 * 
 * Setup:
 * - Change wifi- and MQTT-broker-settings in node-config.h
 * - Change DS18B20-Pin (ONE_WIRE_BUS)
 * - Change Software-Serial-Pins (RXPin, TXPin)
 * 
 * Pinmapping Wemos D1 Mini:
 * Board  Arduino/GPIO  Special     In use
 * D0     16                        DS18B20 onewire
 * D1     5             SCL - i2c   SCL - i2c
 * D2     4             SDA - i2c   SDA - i2c
 * D3     0                         RX - Neo6m GPS
 * D4     2                         TX - Neo6m GPS
 * D5     14            SCK - SPI
 * D6     12            MISO - SPI
 * D7     13            MOSI - SPI
 * D8     15            SS - SPI
 * TX     1
 * RX     3
 * 
 * Author: René Brixel <mail@campingtech.de>
 * Date: 2020-07-08
 * Web: https://campingtech.de/opensrv
 * GitHub: https://github.com/rbrixel
 */

/*
 * Include config-file
 */
#include "node-config.h"

/*
 * Wifi-setup
 */
#include <ESP8266WiFi.h> // ESP8266

/*
 * MQTT-client-setup
 */
#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient client(espClient);

/*
 * OTA-update-setup
 */
#include <ESP8266mDNS.h> // ESP8266
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

/*
 * Webserver-setup
 */
#include <ESP8266WebServer.h>
ESP8266WebServer server(80); // listen on port 80

/*
 * BME280-setup
 */
#include <cactus_io_BME280_I2C.h>
BME280_I2C bme(0x76); // uint i2c-address

/*
 * ds18b20-setup
 */
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 16 // GPIO16 - D0
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18sensors(&oneWire);

/*
 * GPS-setup
 */
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
static const int RXPin = 0, TXPin = 2; // RXPin = D3, TXPin = D4;
static const uint32_t GPSBaud = 9600; // Standard 9600, alternative 4800
TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);

/*
 * Non-blocking-code variables
 */
unsigned long nbcPreviousMillis = 0; // holds last timestamp
const long nbcInterval = 5000; // interval in milliseconds (1000 milliseconds = 1 second)

/*
 * Wifi-setup-function
 */
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA); // set explicit as wifi-client - important that it not acts as an accesspoint AND client!
  WiFi.hostname(host); // set hostname of module
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);
}

/*
 * OTA-update-setup-function
 */
void setup_otau() {
  // ArduinoOTA.setPort(8266); // Port defaults to 8266
  ArduinoOTA.setHostname(host); // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setPassword("campingtech"); // No authentication by default
  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

/*
 * Webserver: not-found-handle
 */
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

/*
 * Webserver: root-handle
 * TODO: Output variables
 */
void handleRoot() {
  char temp[400];
  int uptime = millis();

  snprintf(temp, 400,
           "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>node01 raw output</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <p>Uptime: %02d</p>\
  </body>\
</html>", uptime);
  server.send(200, "text/html", temp);
}

/*
 * MQTT-reconnect (for publishing only)
 */
void reconnect() {
  while (!client.connected()) {
    Serial.print("Reconnecting...");
    if (!client.connect(host)) {
      // To connect with credetials: boolean connect (clientID, [username, password], [willTopic, willQoS, willRetain, willMessage], [cleanSession])
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 3 seconds");
      delay(3000);
    }
  }
  Serial.println("MQTT Connected!");
}

/*
 * Main-setup
 */
void setup() {
  // Start serial-connection for esp
  Serial.begin(115200);

  // Start BME280
  if (!bme.begin()) {
    Serial.println("Could not find BME280 sensor, check wiring");
    while (1);
  }
  bme.setTempCal(-1);// Temp was reading high so subtract 1 degree

  // Start DS18B20-sensor
  ds18sensors.begin();

  // Start wifi and connect to access point
  setup_wifi();

  // Start over-the-air-update
  setup_otau();

  // Start MQTT-publisher
  client.setServer(MQTT_BROKER, 1883); // ip-address, port 1883

  // Start Software-serial for gps
  ss.begin(GPSBaud);

  // Start webserver
  if (MDNS.begin(host)) {
    Serial.println("MDNS responder started");
  }
  server.on("/", handleRoot); // routing
  server.onNotFound(handleNotFound);
  server.begin();
}

/*
 * Main-loop
 */
void loop() {
  // --- realtime code ---

  // Arduino-OTA:
  ArduinoOTA.handle();
  
  // MQTT:
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Webserver:
  server.handleClient();
  MDNS.update();

  // GPS:
  float gpsLat = 0.0; // double lat() / Latitude
  float gpsLon = 0.0; // double lng() / Longitude
  unsigned int gpsSat = 0.0; // uint32_t value() / Satellites
  float gpsAlt = 0.0; // double meters() / Altitude
  unsigned int gpsYear = 0; // uint16_t year() / Year
  unsigned int gpsMonth = 0; // uint8_t month() / Month
  unsigned int gpsDay = 0; // uint8_t day() / Day
  unsigned int gpsHour = 0; // uint8_t hour() / Hour
  unsigned int gpsMinute = 0; // uint8_t hour() / Minute
  unsigned int gpsSecond = 0; // uint8_t second() / Second
  unsigned int gpsCentiSecond = 0; // uint8_t centisecond() / Centisecond
  // A variable declared as "unsigned" can only stores positive values!

  while (ss.available() > 0) {
    if (gps.encode(ss.read())) {
      if (gps.location.isValid() && gps.location.isUpdated()) {
        gpsLat = gps.location.lat();
        gpsLon = gps.location.lng();
      }
      if (gps.satellites.isValid() && gps.satellites.isUpdated()) {
        gpsSat = gps.satellites.value();
      }
      if (gps.altitude.isValid() && gps.altitude.isUpdated()) {
        gpsAlt = gps.altitude.meters();
      }
      if (gps.time.isValid() && gps.time.isUpdated()) {
        gpsHour = gps.time.hour();
        gpsMinute = gps.time.minute();
        gpsSecond = gps.time.second();
        gpsCentiSecond = gps.time.centisecond();
      }
      if (gps.date.isValid() && gps.date.isUpdated()) {
        gpsYear = gps.date.year();
        gpsMonth = gps.date.month();
        gpsDay = gps.date.day();
      }
    }
  }
  
  // --- non-blocking-code, every x sec. publishing ---
  unsigned long nbcCurrentMillis = millis(); // get actual timestamp
  if (nbcCurrentMillis - nbcPreviousMillis >= nbcInterval) {
    nbcPreviousMillis = nbcCurrentMillis;

    // MQTT-Message-Variable
    char msg[50];

    Serial.println("- - - - -");
    Serial.println("Uptime (ms): " + String(millis()));

    // DS18B20:
    if (ds18sensors.getDS18Count() == 0) {
      Serial.println("ds18b20 not found.");
      client.publish("/climate/outdoor/temperature", "error");
    } else {
      ds18sensors.requestTemperatures(); // Send the command to get temperature readings 
      float outTemp = ds18sensors.getTempCByIndex(0); // read first sensor for outdoor temperature
      
      snprintf (msg, 50, "%.2f", outTemp);
      client.publish("/climate/outdoor/temperature", msg);
      Serial.println("/climate/outdoor/temperature: " + String(outTemp) + " (Celsius)");
    }

    // BME280:
    bme.readSensor();
    float bTemp = bme.getTemperature_C(); // Temperature in Celsius
    float bHumi = bme.getHumidity(); // Humidity in Precent
    float bHp = bme.getPressure_HP(); // pressure in pascals
    float bMb = bme.getPressure_MB(); // pressure in millibars

    snprintf (msg, 50, "%.2f", bTemp);
    client.publish("/climate/indoor/floor/temperature", msg);
    snprintf (msg, 50, "%.2f", bHumi);
    client.publish("/climate/indoor/floor/humidity", msg);
    snprintf (msg, 50, "%.2f", bHp);
    client.publish("/climate/indoor/floor/pressure-p", msg);
    snprintf (msg, 50, "%.2f", bMb);
    client.publish("/climate/indoor/floor/pressure-mb", msg);

    Serial.println("/climate/indoor/floor/temperature: " + String(bTemp) + " (Celsius)");
    Serial.println("/climate/indoor/floor/humidity: " + String(bHumi) + " (%)");
    Serial.println("/climate/indoor/floor/pressure-p: " + String(bHp) + " (pascals)");
    Serial.println("/climate/indoor/floor/pressure-mb: " + String(bMb) + " (millibars)");

    // GPS:
    if (gpsLat == 0.0) {
      client.publish("/position/latitude", "error");
    } else {
      snprintf (msg, 50, "%.2f", gpsLat);
      client.publish("/position/latitude", msg);
    }

    if (gpsLon == 0.0) {
      client.publish("/position/latitude", "error");
    } else {
      snprintf (msg, 50, "%.2f", gpsLon);
      client.publish("/position/longitude", msg);
    }

    if (gpsSat == 0) {
      client.publish("/position/latitude", "error");
    } else {
      snprintf (msg, 50, "%0d", gpsSat);
      client.publish("/position/satellites", msg);
    }

    if (gpsAlt == 0.0) {
      client.publish("/position/latitude", "error");
    } else {
      snprintf (msg, 50, "%.2f", gpsAlt);
      client.publish("/position/altitude", msg);
    }

    if (gpsYear == 0) {
      client.publish("/position/date/year", "error");
    } else {
      snprintf (msg, 50, "%0d", gpsYear);
      client.publish("/position/date/year", msg);
    }

    if (gpsMonth == 0) {
      client.publish("/position/date/month", "error");
    } else {
      snprintf (msg, 50, "%0d", gpsMonth);
      client.publish("/position/date/month", msg);
    }

    if (gpsDay == 0) {
      client.publish("/position/date/day", "error");
    } else {
      snprintf (msg, 50, "%0d", gpsDay);
      client.publish("/position/date/day", msg);
    }

    if (gpsHour == 0) {
      client.publish("/position/time/hour", "error");
    } else {
      snprintf (msg, 50, "%0d", gpsHour);
      client.publish("/position/time/hour", msg);
    }

    if (gpsMinute == 0) {
      client.publish("/position/time/minute", "error");
    } else {
      snprintf (msg, 50, "%0d", gpsMinute);
      client.publish("/position/time/minute", msg);
    }

    if (gpsSecond == 0) {
      client.publish("/position/time/second", "error");
    } else {
      snprintf (msg, 50, "%0d", gpsSecond);
      client.publish("/position/time/second", msg);
    }

    if (gpsCentiSecond == 0) {
      client.publish("/position/time/centisecond", "error");
    } else {
      snprintf (msg, 50, "%0d", gpsCentiSecond);
      client.publish("/position/time/centisecond", msg);
    }
    
    Serial.println("/position/latitude: " + String(gpsLat) + " (N, Breitengrad)");
    Serial.println("/position/longitude: " + String(gpsLon) + " (E, Längengrad)");
    Serial.println("/position/satellites: " + String(gpsSat) + " (number)");
    Serial.println("/position/altitude: " + String(gpsAlt) + " (meters)");
    Serial.println("/position/date/year: " + String(gpsYear));
    Serial.println("/position/date/month: " + String(gpsMonth));
    Serial.println("/position/date/day: " + String(gpsDay));
    Serial.println("/position/time/hour: " + String(gpsHour));
    Serial.println("/position/time/minute: " + String(gpsMinute));
    Serial.println("/position/time/second: " + String(gpsSecond));
    Serial.println("/position/time/centisecond: " + String(gpsCentiSecond));
  }
}
