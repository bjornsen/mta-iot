// include the library code:
#include <Adafruit_CharacterOLED.h>
#include <ArduinoJson.h>
#include <WiFi.h>
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

int unmatchedCurlies = 0;
boolean startedJson = false;
unsigned long lastConnectionTime = 10 * 60 * 1000; 	// last connect time
const unsigned long POSTING_INTERVAL = 10 * 60 * 1000; // 10 minutes

StaticJsonBuffer<JSON_BUFF_DIMENSION> jsonBuffer;

// where our data is coming from
const char SERVER[] = "api.open-notify.org";

// variable to hold the JSON-structured data
String incoming_text;

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
  // if 10 minutes have passed since the last check,
  // connect again and request new data.
  if (millis() - lastConnectionTime > POSTING_INTERVAL) {
    // the interval is up, time for a new request, record current time first
    lastConnectionTime = millis();
    httpRequest();
  }

  // read in the result of the httpRequest
  if (client.available()) {
    char c = client.read();  // reads one character at a time, stores it in "c"
    Serial.print(c);

    if (c == '{') {
      startedJson = true;
      unmatchedCurlies++;
    }
    if (c == '}') {
      unmatchedCurlies--;
    }
    if (startedJson == true) {
      incoming_text += c; // add the character to the incoming_text
    }
    if (unmatchedCurlies == 0 && startedJson == true) {
      // we're done reading, time to parse
      parseJson(incoming_text); 
      incoming_text = "";  // clear the string for the next httpRequest
      startedJson = false; 
    }
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
	Serial.println("about to do a GET");
    client.println("GET /astros.json HTTP/1.1\r\n"
      "Host: api.open-notify.org\r\n"
      "Connection: close\r\n"
      "\r\n");
	Serial.println("GET is done");
  } else {
    lcd.setCursor(0,0);
    lcd.print("json connect failed");
    Serial.println("connection to JSON source failed");
  }
}


void parseJson(String json_string) {
  Serial.print("incoming_text string: ");
  Serial.println(json_string);

  // parse the json!
  JsonObject& root = jsonBuffer.parseObject(json_string);

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
  lcd.print(number_of_astros);
  lcd.print(" humans are in");
  lcd.setCursor(0,1);
  lcd.print("space right now.");
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
