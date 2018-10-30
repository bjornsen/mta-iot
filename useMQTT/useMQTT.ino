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

#define JSON_BUFF_DIMENSION 2500

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

int endResponse = 0;
boolean startJson = false;
unsigned long lastConnectionTime = 10 * 60 * 1000; 	// last connect time
const unsigned long POSTING_INTERVAL = 10 * 60 * 1000; // 10 minutes

StaticJsonBuffer<JSON_BUFF_DIMENSION> jsonBuffer;

// where our data is coming from
const char SERVER[] = "api.open-notify.org";

// variable to hold the JSON-structured data
String incoming_text;

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

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Connected");
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void loop()
{
  // if 10 minutes have passed since the last check,
  // connect again and request new data.
  if (millis() - lastConnectionTime > POSTING_INTERVAL) {
    // the interval is up, time for a new request, record current time first
    lastConnectionTime = millis();
    lcd.setCursor(0,0);
    lcd.print("in loop -> httpRequest");
    httpRequest();
  }

  // read in the result of the httpRequest
  char c = 0;
  if (client.available()) {
    c = client.read();

    if (endResponse == 0 && startJson == true) {
      // we're done reading, time to parse
      parseJson(incoming_text.c_str()); 
      incoming_text = "";  // clear the string for the next httpRequest
      startJson = false; 
    }
    if (c == '{') {
      startJson = true;
      endResponse++;
    }
    if (c == '}') {
      endResponse--;
    }
    if (startJson == true) {
      incoming_text += c;
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
}

void httpRequest() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("in httpRequest()");
  // Close any connection to server before opening a new one
  client.stop(); 
  
  // if there is a successful connection
  if (client.connect(SERVER, 80)) {
    client.println("GET /astros.json HTTP/1.1\r\nHost: api.open-notify.org\r\n\r\n");
    client.println("Connection: close");
    client.println();
  } else {
    lcd.setCursor(0,0);
    lcd.print("json connect failed");
    Serial.println("connection to JSON source failed");
  }
}


void parseJson(String json_string) {
  Serial.println(json_string);
  // cast the string to a character array for parseObject
  char * json = new char [json_string.length() +1];
  strcpy(json, json_string.c_str());

  // actually parse it
  JsonObject& root = jsonBuffer.parseObject(json);

  // make sure it worked
  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }

  // extract the data we want
  int number_of_astros = root["number"];

  // print it to the display
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("There are ");
  lcd.print(number_of_astros);
  lcd.setCursor(0,1);
  lcd.print(" humans in space.");
}
