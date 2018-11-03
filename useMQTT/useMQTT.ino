// include the library code:
#include <Adafruit_CharacterOLED.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

#define MQTT_BROKER_IP "192.168.1.92"
#define MQTT_PORT 1881
#define BUTTON 21
#define LED 13  // onboard red LED

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

   a "button"       21

 Adafruit_CharacterOLED lcd(OLED_V2, version, rs, rw, en, d4, d5, d6, d7);
*/
Adafruit_CharacterOLED lcd(OLED_V2, 14, 32, 15, 33, 27, 12, 13);

// Set up WiFi
const char ssid[] = SECRET_SSID;
const char password[] = SECRET_PASS;

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

int buttonState = -1;
long last_button;

void setup()
{
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("In setup");

  pinMode(BUTTON, INPUT_PULLUP); //use the pullup resistor inside the board
  pinMode(LED, OUTPUT); //use the pullup resistor inside the board

  // Start by connecting to a WiFi network
  connectWifi();

  // Then connect to the MQTT broker
  connectMQTT();
}


void loop()
{
  if (!mqtt_client.connected()) {
    connectMQTT();
  }
  mqtt_client.loop();

  // Make the red onboard LED light when we press the "button"
  if (digitalRead(BUTTON)) {
    digitalWrite(LED, 0);
    if (buttonState != 0 && millis() - last_button > 50) {
      //publish new state to MQTT
      mqtt_client.publish("button", "0");
      buttonState = 0;
      last_button = millis();
    }
  } else {
    digitalWrite(LED, 1);
    if (buttonState != 1 && millis() - last_button > 50) {
      //publish new state to MQTT
      mqtt_client.publish("button", "1");
      buttonState = 1;
      last_button = millis();
    }
  }
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
  lcd.print("Connecting to");
  lcd.setCursor(0,1);
  lcd.print("MQTT");
  Serial.print("Connecting to MQTT server ");
  Serial.print(MQTT_BROKER_IP);
  Serial.print(":");
  Serial.println(MQTT_PORT);
  mqtt_client.setServer(MQTT_BROKER_IP, MQTT_PORT);
  // deviceID, usrname,pw
  mqtt_client.connect("10","","");
  mqtt_client.setCallback(callback);

  if (mqtt_client.connected()) {
    Serial.println("MQTT: Connected");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Connected to");
    lcd.setCursor(0,1);
    lcd.print("MQTT");
  } 
  mqtt_client.subscribe("Slidervalue");
  mqtt_client.subscribe("button");
}


// MQTT only has a single callback, so you have to figure out
// which topic it's for, and then process
void callback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Receiving: ");
  Serial.print(topic);
  Serial.print(" = ");
  Serial.write(payload, length);
  Serial.println(" ");

  if (strcmp(topic, "Slidervalue") == 0) {
    lcd.setCursor(0,0);
    lcd.print("Slider value:   ");
    lcd.setCursor(16-length,0);
    for (int i = 0; i < length; i++) {
      lcd.print((char)(payload[i]));
    }
  } 

  if (strcmp(topic, "button") == 0) {
    lcd.setCursor(0,1);
    lcd.print("Button value:   ");
    lcd.setCursor(16-length,1);
    for (int i = 0; i < length; i++) {
      lcd.print((char)(payload[i]));
    }
  } 
}
