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
#include <Esp.h>
#include <NTPClient.h>

#include "subway.h"

#define PB_FIELD_16BIT

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

// The user should set these values
const char *STATION_LIST[4] = {"D24N", "R31N","",""};
const char STOP_NAME[] = "Atlantic Av - Barclays Ctr";
int FEED_NUMBER = 16;
const char API_KEY[] = "147b1831f52a260d239082fa92ec8e35";
// Filter all trains that arrive before you could get to the station
const time_t TIME_TO_TRAIN = 5 * 60;

// These values probably don't need to change
int PB_BUFFER_SIZE = 100000;
int LOG_BUFFER_SIZE = 1024;

// Set up WiFi
const char *ssid = "mah_network";
const char *password = "mah_password";

// The MTA's data feed hostname
const char *host = "datamine.mta.info";

// Other useful globals
uint8_t *pb_buffer = NULL;
char *log_buffer = NULL;
int total_data_bytes = 0;
WiFiClient client;

// Set up time
#define NTP_OFFSET  0 // -5  * 60 * 60 // In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "0.pool.ntp.org"

WiFiUDP ntpUDP;
NTPClient time_client(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

bool arrival_cmp(ArrivalInfo a, ArrivalInfo b) {
  return (a.departure < b.departure);
}

void lcd_print_arrival_list(const std::vector<ArrivalInfo> &arrival_list) {
  time_t now, diff;
  
  // This doesn't work by default on the ESP32
  now = (time_t) time_client.getEpochTime();
  snprintf(log_buffer, LOG_BUFFER_SIZE,"Current time: %s", ctime(&now));
  log_message(LOG_INFO, log_buffer);

  lcd.clear();
  char lcd_buffer[16];
  int count = 0;
  for (const auto &arr_info : arrival_list) {
    if (count > 1) {
      break;
    }
    // Don't display trains that arrive before now
    snprintf(log_buffer, LOG_BUFFER_SIZE,
             "Next train unix: %lld\nnow + TTT: %d\n",
             (long long) arr_info.departure,
             now + TIME_TO_TRAIN);
    log_message(LOG_INFO, log_buffer);
    diff = difftime((time_t) arr_info.departure, now + TIME_TO_TRAIN);
    if (diff < 0) {
      log_message(LOG_INFO, "Departure not less than");
      continue;
    }
    struct tm *depart_tm = localtime((time_t *) &arr_info.departure); 
    Serial.printf("Next departure: %s\r\n", ctime((time_t *) &arr_info.departure));
    diff = difftime((time_t) arr_info.departure, now);
    struct tm *diff_tm = localtime(&diff);
    char buf[4];
    strftime(&buf[0], 4, "%M", diff_tm);
    snprintf(&lcd_buffer[0], 16, "%s - %s min",
            (char *) &arr_info.train_type[0],
            &buf);
    lcd.setCursor(0, count);
    lcd.print(&lcd_buffer[0]);
    count++;
  }
}

void setup()
{
  Serial.begin(115200);
  delay(100);

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("Joining Wifi");

  WiFi.begin(ssid, password);
  time_client.begin();

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
	lcd.print(".");
  }

  lcd.print("Connected");
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  logger_callback logger_callback_ptr = logger_func;
  register_logger(logger_callback_ptr);

  pb_buffer = (uint8_t *) malloc(PB_BUFFER_SIZE);
  if (!pb_buffer) {
    log_message(LOG_ERROR, "pb_buffer malloc failed");
  }

  log_buffer = (char *) malloc(LOG_BUFFER_SIZE);
  if (!log_buffer) {
    log_message(LOG_ERROR, "log_buffer malloc failed");
  }
}

void logger_func(enum log_level level, char *message) {
        if (level <= LOG_LEVEL) {
                Serial.println(message);
        }
}

void get_protobuf_data(uint8_t *buffer) {
  // This will send the request to the server
  int url_len = 256;
  char url[url_len];
  snprintf(&url[0], url_len, "/mta_esi.php?key=%s&feed_id=%i", API_KEY, FEED_NUMBER);
  // We now create a URI for the request
  log_message(LOG_INFO, "Requesting URL: ");
  log_message(LOG_INFO, url);
  char request[1024];
  snprintf(request, 1024, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", url, host);
  client.print(request);
  delay(1000);

  // Read all of the headers
  char headers[1024];
  memset(&headers, 0, 1024);
  while (strncmp(&headers[0], "\r", 1) != 0) {
    memset(&headers[0], 0, 1024);
    int bytes_read = client.readBytesUntil('\n', &headers[0], 1024);
    Serial.println(String(headers));
    Serial.println(headers[0], DEC);
    Serial.println(headers[1], DEC);
    Serial.println("-----");
  }
  Serial.println("End of headers");

  int bytes_read = 0;
  char transfer_length[8];

  total_data_bytes = 0;
  long tl = -1;
  while(true) {
    // Read until CRLF
    bytes_read = client.readBytesUntil('\n', &transfer_length[0], SIZE_MAX);
    Serial.printf("Read %d bytes\n", bytes_read);
    transfer_length[bytes_read] = '\0';
    tl = strtol(transfer_length, NULL, 16);
    if (tl == 0) {
      break;
    }
    Serial.printf("Next transfer holds %d bytes\n", tl);
    Serial.printf("There are %d bytes ready to be read\n", client.available());
    while (tl > 0) {
      while(!client.available()) {
        delay(100);
        Serial.printf("Waiting...\n");
      }
      bytes_read = client.read(&(buffer[total_data_bytes]), tl);
      tl -= bytes_read;
      total_data_bytes += bytes_read;
      Serial.printf("%d bytes left to read\n", tl);
    }
    bytes_read = client.readBytesUntil('\n', &transfer_length[0], SIZE_MAX);
    Serial.printf("Cleared 2 bytes confirmed with %d\n", bytes_read);
    Serial.printf("Read %d so far\n", total_data_bytes);
  }
}

int value = 0;

void loop()
{
  delay(5000);
  ++value;

  Serial.print("connecting to ");
  Serial.println(host);

  time_client.update();

  // Use WiFiClient class to create TCP connections
  const int httpPort = 80;
  if (!client.connect(host, httpPort))
  {
    Serial.println("connection failed");
    return;
  }

  memset(pb_buffer, 0, 80000);
  get_protobuf_data(pb_buffer);

  pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *) pb_buffer, total_data_bytes);

  transit_realtime_FeedMessage feedmessage = {};
  memset(&feedmessage, 0,sizeof(transit_realtime_FeedMessage));
  std::vector<ArrivalInfo> arrival_list;

  feedmessage.entity.funcs.decode = &entity_callback;
  feedmessage.entity.arg = (void *)&arrival_list;

  bool ret = pb_decode(&istream, transit_realtime_FeedMessage_fields, &feedmessage);
  if (!ret)
  {
    log_message(LOG_ERROR, "Decode failed");
  }
  std::sort(arrival_list.begin(), arrival_list.end(), arrival_cmp);

  // Read all the lines of the reply from server and print them to Serial
  print_arrival_list(arrival_list);
  lcd_print_arrival_list(arrival_list);

  log_message(LOG_INFO, "\nClosing connection\n");

  snprintf(log_buffer, LOG_BUFFER_SIZE, "%d", ESP.getFreeHeap());
  log_message(LOG_INFO, log_buffer);
  delay(30000);
}
