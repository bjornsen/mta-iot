  /*
   Based on the LiquidCrystal Library - Hello World
   
   Demonstrates the use a Winstar 16x2 OLED display.  These are similar, but
   not quite 100% compatible with the Hitachi HD44780 based LCD displays. 
   
   This sketch prints "Hello OLED World" to the LCD
   and shows the time in seconds since startup.
   
 There is no need for the contrast pot as used in the LCD tutorial
 
 Library originally added 18 Apr 2008
 by David A. Mellis
 library modified 5 Jul 2009
 by Limor Fried (http://www.ladyada.net)
 example added 9 Jul 2009
 by Tom Igoe
 modified 22 Nov 2010
 by Tom Igoe
 Library & Example Converted for OLED
 by Bill Earl 30 Jun 2012
 
 This example code is in the public domain.
 */

// include the library code:
#include <Adafruit_CharacterOLED.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

#define MQTT_BROKER_IP "192.168.1.92"
#define MQTT_PORT 1881

/*
 * initialize the library with the OLED hardware
 * version OLED_Vx and numbers of the interface pins.
 * OLED_V1 = older, OLED_V2 = newer. If 2 doesn't work try 1 ;)

 Wiring Guide:

   OLED pin  <-->   ESP32 pin
    4 (rs)          14
    5 (rw)          32
    6 (en)          15
   11 (d4)          33
   12 (d5)          27
   13 (d6)          12
   14 (d7)          13

 Adafruit_CharacterOLED lcd(OLED_V2, version, rs, rw, en, d4, d5, d6, d7);
*/
Adafruit_CharacterOLED lcd(OLED_V2, 14, 32, 15, 33, 27, 12, 13);

// Set up WiFi
const char ssid[] = SECRET_SSID;
const char password[] = SECRET_PASS;

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

void setup()
{
  Serial.begin(115200);
  delay(100);

  Serial.println();
  Serial.println("In setup");

  // Start by connecting to a WiFi network
  connectWifi();

  // Then connect to the MQTT broker
  connectMQTT();

}


void loop()
{
  if (!mqtt_client.connected()) {
    reconnect();
  }
  mqtt_client.loop();
}

void connectWifi()
{
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Joining Wifi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    lcd.print(".");
  }

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Connected");
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void connectMQTT()
{
  delay(1000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Connecting to MQTT");
  Serial.print("Connecting to MQTT server ");
  Serial.print(MQTT_BROKER_IP);
  Serial.print(":");
  Serial.println(MQTT_PORT);
  mqtt_client.setServer(MQTT_BROKER_IP, MQTT_PORT);
  mqtt_client.setCallback(callback);

  if (mqtt_client.connected()) {
    Serial.println("MQTT: Connected");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Connected to MQTT");
  }
  mqtt_client.subscribe("astronauts");
}


void reconnect()
{
  while (!mqtt_client.connected()) {
	Serial.print("Attempting MQTT connection");
    if (mqtt_client.connected()) {
      Serial.println("Connected to MQTT");
      mqtt_client.setServer(MQTT_BROKER_IP, MQTT_PORT);
      mqtt_client.setCallback(callback);
      mqtt_client.subscribe("astronauts");
    } else {
      Serial.println("Failed to connect to MQTT");
    }
    delay(5000);
  }
}

// MQTT only has a single callback, so you have to figure out
// which topic it's for, and then process
void callback(char* topic, byte* payload, unsigned int length)
{
  if (topic == "astronauts") {
    Serial.print("Receiving: ");
    Serial.print(topic);
    Serial.print(" = ");
    Serial.write(payload, length);
    Serial.println(" ");
  } 
}
