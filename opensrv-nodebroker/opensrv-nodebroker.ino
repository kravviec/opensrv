/*
 * uMQTTBroker demo for Arduino (C++-style)
 * 
 * The program defines a custom broker class with callbacks, 
 * starts it, subscribes locally to anything, and publishs a topic every second.
 * Try to connect from a remote client and publish something - the console will show this as well.
 * 
 * https://github.com/martin-ger/uMQTTBroker
 */

#include <ESP8266WiFi.h>
#include "uMQTTBroker.h"

/*
 * Your WiFi config here
 */
char ssid[] = "*****";     // your network SSID (name)
char pass[] = "*****"; // your network password

/*
 * Custom broker class with overwritten callback functions
 */
class myMQTTBroker: public uMQTTBroker
{
public:
    virtual bool onConnect(IPAddress addr, uint16_t client_count) {
      Serial.println(addr.toString()+" connected");
      return true;
    }
    
    virtual bool onAuth(String username, String password) {
      Serial.println("Username/Password: "+username+"/"+password);
      return true;
    }
    
    virtual void onData(String topic, const char *data, uint32_t length) {
      char data_str[length+1];
      os_memcpy(data_str, data, length);
      data_str[length] = '\0';
      
      Serial.println("received topic '"+topic+"' with data '"+(String)data_str+"'");
    }
};

myMQTTBroker myBroker;

/*
 * WiFi init stuff
 */
void startWiFiClient()
{
  Serial.println("Connecting to "+(String)ssid);
  WiFi.mode(WIFI_STA); // set explicit as wifi-client - important that it not acts as an accesspoint AND client!
  WiFi.hostname("nodebroker"); // set hostname of module
  WiFi.begin(ssid, pass);

  // static ip address
  IPAddress ip(192,168,111,199);   
  IPAddress gateway(192,168,111,1);   
  IPAddress subnet(255,255,255,0);   
  WiFi.config(ip, gateway, subnet);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  
  Serial.println("WiFi connected");
  Serial.println("IP address: " + WiFi.localIP().toString());
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // Start WiFi
  startWiFiClient();

  // Start the broker
  Serial.println("Starting MQTT broker");
  myBroker.init();

  /*
   * Subscribe to anything
   */
  myBroker.subscribe("#");
}

//int counter = 0;

void loop()
{
  /*
   * Publish the counter value as String
   */
  //myBroker.publish("broker/counter", (String)counter++);
   
  // wait a second
  //delay(1000);
}
