/*
 * Skylight Fan Control
 * René Brixel <mail@campingtech.de>
 * 26.06.2020 - v0.2
 * 09.02.2020 - v0.1
 * 
 * Hardware used:
 * --------------
 * Arduino Pro Mini Clone (in final version a nano or esp32)
 * internal EEPROM
 * DHT22 temperature and humidity sensor
 * 3x Push-Buttons with external Pull-Up-Resistors
 * i2c-OLED-Display 0.96 inch (128 * 32 px)
 * 
 * Libs used:
 * ----------
 * DHT sensor library v1.3.10 by Adafruit
 * Adafruit SSD1306 v2.3.0 by Adafruit
 * Adafruit GFX Library v1.9.0 by Adafruit
 */

// WIFI
#include <ESP8266WiFi.h> // ESP8266
#ifndef STASSID
#define STASSID "*****" // your wifi-name
#define STAPSK  "*****" // your wifi-password
#endif
const char* host = "node02"; // hostname of module
const char* ssid = STASSID;
const char* password = STAPSK;

// MQTT-Client
#include <PubSubClient.h>
const char* MQTT_BROKER = "192.168.111.199"; // ip-address of your mqtt-broker
WiFiClient espClient;
PubSubClient client(espClient);

// OTA-Update
#include <ESP8266mDNS.h> // ESP8266
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// BME280
#include <cactus_io_BME280_I2C.h>
BME280_I2C bme(0x76); // uint i2c-address

// EEPROM
#include <EEPROM.h>
#define EFANTEMP  0 // Set temperature when fan should start
#define EFANTIME  1 // duration in minutes how long the fan should spin
#define EFANSMAX  2 // max rpm in percent
#define EFANTMAX  3 // duration how long it took the fan to max rpm

// OLED-Display over i2c
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH  128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET    4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define OLED_I2C      0x3C // i2c-address of display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Interval-values (for non blocking code)
unsigned long nbcPreviousMillis = 0; // holds last timestamp
const long nbcInterval = 2000; // interval in milliseconds (1000 milliseconds = 1 second)

// ==[ CONFIG: OWN VARS ]==
#define LEDINT  13 // internal LED on "D13"
#define TMENU   6 // Button Menu
#define TPLUS   5 // Button Plus
#define TMINUS  3 // Button Minus (Pin 4 doesn't work)
#define BTNDEBOUNCE 200 // button-debounce; value in milliseconds
int posMenu = 0;      // var for menu-position
float fltHumi = 0.0;  // var for humidity
float fltTemp = 0.0;  // var for temperature
int eFanTemp = 0;     // var for EEPROM-value; Set temperature when fan should start
int eFanTime = 0;     // var for EEPROM-value; duration in minutes how long the fan should spin
int eFanSMax = 0;     // var for EEPROM-value; max rpm in percent
int eFanTMax = 0;     // var for EEPROM-value; duration how long it took the fan to max rpm

// ==[ SETUP-FUNCTION ]==
void setup() {
  // read EEPROM values
  eFanTemp = EEPROM.read(EFANTEMP);
  eFanTime = EEPROM.read(EFANTIME);
  eFanSMax = EEPROM.read(EFANSMAX);
  eFanTMax = EEPROM.read(EFANTMAX);

  // if on first startup EEPROM-value empty (e.g. 255) then set default values and store it in EEPROM
  if (eFanTemp == 255) {
    eFanTemp = 25;
    EEPROM.update(EFANTEMP, eFanTemp);
  }
  if (eFanTime == 255) {
    eFanTime = 30;
    EEPROM.update(EFANTIME, eFanTime);
  }
  if (eFanSMax == 255) {
    eFanSMax = 100;
    EEPROM.update(EFANSMAX, eFanSMax);
  }
  if (eFanTMax == 255) {
    eFanTMax = 5;
    EEPROM.update(EFANTMAX, eFanTMax);
  }

  // Set the pwm-frequency
  // with standard prescaler you can hear the pwm-frenquency from the fans
  TCCR0A = _BV(COM0A1) | _BV(COM0B1) | _BV(WGM01) | _BV(WGM00); // Fast PWM
  TCCR0B = _BV(CS01); // Prescaler 8; lowest prescaler near standard value (1) without sounding fan
  // "...To adjust millis(), micros(), delay(),... accordingly,
  // You can modify a line in the wiring.c function in the Arduino program files
  // hardware\arduino\cores\arduino\wiring.c
  // #define MICROSECONDS_PER_TIMER0_OVERFLOW (clockCyclesToMicroseconds(PRESCALE_FACTOR* 256))
  // #define MICROSECONDS_PER_TIMER0_OVERFLOW (clockCyclesToMicroseconds(8 * 256))
  // ..."; Source: https://playground.arduino.cc/Main/TimerPWMCheatsheet/

  // Start DHTxx-sensor
  dht.begin();

  // initialize pinmodes (input, output, pullup, pulldown, ...)
  pinMode(LEDINT, OUTPUT); // activate internal led
  pinMode(TMENU, INPUT);
  pinMode(TPLUS, INPUT);
  pinMode(TMINUS, INPUT);
  // TODO: Check internal pullup-resistors, if they work properly
  // Use instead: pinMode (pinX, INPUT_PULLUP);
  // and remove external pullup-resistors

  // initialize i2c-oled
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.display();
  display.clearDisplay();
}

void loop() {
  // Non-blocking-Code; only executed when interval is reached
  unsigned long nbcCurrentMillis = millis(); // get actual timestamp
  if (nbcCurrentMillis - nbcPreviousMillis >= nbcInterval) {
    nbcPreviousMillis = nbcCurrentMillis;
    
    // read DHTxx-sensor
    fltHumi = dht.readHumidity();
    fltTemp = dht.readTemperature();
  }

  // ----- Here starts the realtimecode -----

  // --[ menu-positionscounter ]--
  int intTMenu = 0;
  intTMenu = digitalRead(TMENU);
  if (intTMenu == 1) {
    posMenu += 1;
    // 0 = main-display
    // 1 = Set temperature when fan should start
    // 2 = duration in minutes how long the fan should spin
    // 3 = max rpm in percent
    // 4 = duration how long it took the fan to max rpm
    if (posMenu > 4) { // overflow
      posMenu = 0;
    }

    // update EEPROm if menu is pressed
    EEPROM.update(EFANTEMP, eFanTemp);
    EEPROM.update(EFANTIME, eFanTime);
    EEPROM.update(EFANSMAX, eFanSMax);
    EEPROM.update(EFANTMAX, eFanTMax);
    
    delay(BTNDEBOUNCE);
  }

  // --[ menu-output ]--
  switch (posMenu) {
    case 0:
      menuMain();
      break;
    case 1:
      menuTemp();
      break;
    case 2:
      menuWerte();
      break;
    default:
      // if nothing else matches, do the default
      // default is optional
      break;
  }

  // --[ Set temperature ]--
  if (posMenu == 1) {
    int intTPlus = 0;
    intTPlus = digitalRead(TPLUS);
    int intTMinus = 0;
    intTMinus = digitalRead(TMINUS);

    if (intTPlus == 1) {
      eFanTemp += 1;
    }
    if (intTMinus == 1) {
      eFanTemp -= 1;
    }

    if (eFanTemp < 10) { // underflow
      eFanTemp = 10;
    }
    if (eFanTemp > 50) { // overflow
      eFanTemp = 50;
    }

    delay(BTNDEBOUNCE); // debounce
  }
}

void menuHeader() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  // output the menu-header
  display.setCursor(120, 0);
  display.print(posMenu);
}

void menuDrawHead(String strTitle) {
  display.setCursor(0, 0);
  display.print(strTitle); // max 21 chars and 3 lines
  display.drawLine(0, 8, display.width()-1, 8, SSD1306_WHITE);
}

void menuMain () {
  menuHeader();
  menuDrawHead("FANCONTROL");
  display.setCursor(0, 13);
  display.print(F("Raumklima:"));
  display.setCursor(0, 25);
  display.print(String(fltTemp, 1) + " *C / " + String(fltHumi, 1) + " %");
  display.display();
}

void menuTemp () {
  menuHeader();
  menuDrawHead("Temp. einstellen");
  display.setCursor(0, 13); // x, y
  display.print("Akt. Wert: " + String(eFanTemp) + " *C");
  display.setCursor(0, 25); // x, y
  display.print("Temp.: 10 bis 50 *C");
  display.display();
}

void menuWerte () {
  menuHeader();
  menuDrawHead("Systemwerte");
  display.setCursor(0, 13);
  display.print("Temp:" + String(eFanTemp) + " / Time: " + String(eFanTime));
  display.setCursor(0, 25);
  display.print("SMax:" + String(eFanSMax) + " / TMax: " + String(eFanTMax));
  display.display();
}
