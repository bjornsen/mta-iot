// include the library code:
#include <Adafruit_CharacterOLED.h>
#include <WiFi.h>
#include "secrets.h"

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

WiFiClient client;

void setup()
{
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("In setup");


  // Start by connecting to a WiFi network
  connectWifi();
}


void loop()
{
  delay(5000);

  lcd.clear();
  lcd.setCursor(0,0);

  if (WiFi.status() == WL_CONNECTED) {
    lcd.print("Still connected");
    lcd.setCursor(0,1);
    lcd.print(":)  :)  :)  :)");
  } else {
    lcd.print("Connection lost :(");
    lcd.setCursor(0,1);
    lcd.print(":(  :(  :(  :(");
  }

  Serial.println("another pass through the loop");
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


