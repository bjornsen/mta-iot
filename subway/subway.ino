/** @file subway.ino
 *  @brief Displays real time MTA subway information
 *
 *  Displays realtime MTA subway information. Tested with
 *  the ESP32 and Winstar 16x2 OLED display.
 *
 *  @author Bill Bernsen (bill@nycresistor.com)
 *  @bug No known bugs.
 */

// include the library code:
#include <Adafruit_CharacterOLED.h>
#include <WiFi.h>
#include <Esp.h>
#include <NTPClient.h>
#include "secrets.h"

#include <math.h>
#include <algorithm>

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
// Look up your station from stops.txt downloaded here: https://transitfeeds.com/p/mta/79
const char *STATION_LIST[4] = {"D24N", "R31N","",""};
const char STOP_NAME[] = "Atlantic Av - Barclays Ctr";
int FEED_NUMBER = 16;
// Filter all trains that arrive before you could get to the station
const time_t TIME_TO_TRAIN = 5 * 60;
// How often to check the MTA for updates. Defaults to 30s because that's how often
// the feed is updated.
int UPDATE_INTERVAL = 30000;

// These values probably don't need to change
const int LOG_BUFFER_SIZE = 1024;
const int PB_CHUNK_SIZE = 4096;

// Set up WiFi
const char *ssid = SECRET_SSID;
const char *password = SECRET_PASS;

// The MTA's data feed hostname
const char *host = "datamine.mta.info";

// Used for tracking streaming data from MTA upstream
int data_chunk_left = -1;
int data_buf_left = -1;
int data_buf_offset = 0;
uint8_t data_buf[PB_CHUNK_SIZE];

// Other useful globals
char *log_buffer = NULL;
int num_failures = 0;
time_t last_success = 0;

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

// Print a list of train arrivals to the attached LCD screen
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
             "Next train unix: %lld\nnow + TTT: %d",
             (long long) arr_info.departure,
             now + TIME_TO_TRAIN);
    log_message(LOG_DEBUG, log_buffer);

    // Filter out trains that are impossible to make
    diff = difftime((time_t) arr_info.departure, now + TIME_TO_TRAIN);
    if (diff < 0) {
      continue;
    }

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

void logger_func(enum log_level level, char *message) {
        if (level <= LOG_LEVEL) {
                Serial.println(message);
        }
}

// Get the next chunk size from the chunked data transfer
int get_next_chunk_size(WiFiClient *client) {
  char chunk_str[16];
  int bytes_read = 0;

  time_t start = (time_t) time_client.getEpochTime();
  time_t now = start;
  while (!client->available())
  {
    delay(100);
    Serial.printf("Waiting to get_next_chunk_size...\n");

    // Time out in 30s
    if (difftime(now, start) > 30) {
      Serial.print("Timeout in get_next_chunk_size");
      return 0;
    }
  }

  bytes_read = client->readBytesUntil('\n', &chunk_str[0], SIZE_MAX);
  Serial.printf("Read %d bytes\n", bytes_read);

  // Guard against the caller passing us a client that hasn't cleared
  // the CRLF after the previous chunk data.
  if (strncmp((char *) &chunk_str[0], "\r", 1) == 0) {
    bytes_read = client->readBytesUntil('\n', &chunk_str[0], SIZE_MAX);
    Serial.printf("Read %d bytes\n", bytes_read);
  }

  chunk_str[bytes_read] = '\0';
  int chunk_length = strtol(chunk_str, NULL, 16);
  Serial.printf("Next chunk length is %d bytes\n", chunk_length);
  return chunk_length;
}

int wifi_read(uint8_t *buf, int bytes_to_read) {
  int bytes_read = 0;
  while (bytes_to_read > bytes_read)
  {
    time_t start = (time_t)time_client.getEpochTime();
    time_t now = start;
    while (!client.available())
    {
      delay(100);
      Serial.printf("Waiting to wifi_read...\n");

      // Time out in 30s
      if (difftime(now, start) > 30)
      {
        Serial.print("Timeout in wifi_read");
        return 0;
      }
    }
    bytes_read += client.read(buf, bytes_to_read - bytes_read);
    Serial.printf("Read %d bytes in callback\n", bytes_read);
  }
  return bytes_read;
}

// Returns false and sets bytes_left for EOF
bool pb_istream_callback(pb_istream_t *stream, uint8_t *buf, size_t count) {
  WiFiClient *client = (WiFiClient*) stream->state;
  bool status = true;
  int bytes_to_read = 0;
  int bytes_read = 0;

  // Read in data when we have none
  if (data_chunk_left <= 0 && data_buf_left <= 0) {
    data_chunk_left = get_next_chunk_size(client);
    if (data_chunk_left == 0) {
      log_message(LOG_INFO, "Chunked transfer has zero bytes left");
      stream->bytes_left = 0;
      return false;
    }
    bytes_to_read = std::min(PB_CHUNK_SIZE, data_chunk_left);

    // Read data into buffer and reset the buffer offset
    bytes_read = wifi_read(&data_buf[0], bytes_to_read);
    data_buf_left = bytes_read;
    data_chunk_left -= bytes_read;
    data_buf_offset = 0;
  }

  snprintf(log_buffer, LOG_BUFFER_SIZE, "%d bytes left in buffer with %d chunk left for %d requested",
           data_buf_left,
           data_chunk_left,
           count);
  log_message(LOG_DEBUG, log_buffer);

  if (count <= data_buf_left) {
    snprintf(log_buffer, LOG_BUFFER_SIZE, "Copying %d bytes from offset %d", count, data_buf_offset);
    log_message(LOG_DEBUG, log_buffer);
    strncpy((char *) buf, (char *) &data_buf[data_buf_offset], count);
    data_buf_offset += count;
    data_buf_left -= count;
  } else {
    // Copy the rest of the buffer into the caller's buffer
    strncpy((char *) buf, (char *) &data_buf[data_buf_offset], data_buf_left);
    // Remember an offset for the caller's buffer to continue writing
    int offset = data_buf_left;
    // Decrement the amount of data to be read
    count -= data_buf_left;

    // Get the next chunk if there's nothing left in this one
    if (data_chunk_left == 0) {
      data_chunk_left = get_next_chunk_size(client);
      if (data_chunk_left == 0) {
        log_message(LOG_INFO, "Stream indicated EOF");
        stream->bytes_left = 0;
        return false;
      }
    }

    bytes_to_read = std::min(PB_CHUNK_SIZE, data_chunk_left);
    bytes_read = wifi_read(&data_buf[0], bytes_to_read);
    data_buf_left = bytes_read;
    data_chunk_left -= bytes_read;
    data_buf_offset = 0;

    // Copy the rest of the requested bytes into the appropriate offset
    strncpy((char *) buf+offset, (char *) &data_buf[data_buf_offset], count);
    data_buf_offset += count;
    data_buf_left -= count;
  }

  return status;
}

// Sends a GET request to the MTA then reads and prints the headers
void request_data() {
  // This will send the request to the server
  int url_len = 256;
  char url[url_len];
  snprintf(&url[0], url_len, "/mta_esi.php?key=%s&feed_id=%i", API_KEY, FEED_NUMBER);

  // Create a URI for the request
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
    Serial.print(String(headers));
    Serial.println("-----");
  }
  Serial.println("End of headers");
}

void setup()
{
  Serial.begin(115200);
  delay(100);

  // We start by connecting to a WiFi network
  connectWiFi();

  logger_callback logger_callback_ptr = logger_func;
  register_logger(logger_callback_ptr);

  log_buffer = (char *) malloc(LOG_BUFFER_SIZE);
  if (!log_buffer) {
    log_message(LOG_ERROR, "log_buffer malloc failed");
  }
}

void loop()
{
  delay(5000);

  time_client.update();

  Serial.print("connecting to ");
  Serial.println(host);

  // Use WiFiClient class to create TCP connections
  const int httpPort = 80;
  if (!client.connect(host, httpPort))
  {
    Serial.println("connection failed");
    return;
  }
  request_data();

  data_chunk_left = 0;
  data_buf_left = 0;
  data_buf_offset = 0;
  pb_istream_t istream = {&pb_istream_callback, &client, SIZE_MAX};

  transit_realtime_FeedMessage feedmessage = {};
  memset(&feedmessage, 0,sizeof(transit_realtime_FeedMessage));
  std::vector<ArrivalInfo> arrival_list;

  feedmessage.entity.funcs.decode = &entity_callback;
  feedmessage.entity.arg = (void *)&arrival_list;

  bool ret = pb_decode(&istream, transit_realtime_FeedMessage_fields, &feedmessage);
  if (!ret)
  {
    log_message(LOG_ERROR, "Decode failed");
    // Perform exponential backoff of retries starting with 1s until it reaches the
    // update interval
    int retry = std::min(std::pow(2, num_failures), (double) UPDATE_INTERVAL);
    num_failures++;

    time_t now = (time_t) time_client.getEpochTime();
    time_t diff = difftime((time_t) last_success, now);

    // Only display an error after 60 seconds with no updates
    if (diff > 60) {
      char buf[4];
      struct tm *diff_tm = localtime(&diff);
      strftime(&buf[0], 4, "%S", diff_tm);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.printf("No updates for %ss", buf);
      lcd.setCursor(0, 1);
      lcd.printf("Reboot?");
    }

    Serial.printf("Failed %d times. Retry in %dms", num_failures, retry);
    delay(retry);
  } else {
    std::sort(arrival_list.begin(), arrival_list.end(), arrival_cmp);

    // Read all the lines of the reply from server and print them to Serial
    print_arrival_list(arrival_list);
    lcd_print_arrival_list(arrival_list);

    snprintf(log_buffer, LOG_BUFFER_SIZE, "%d bytes left in the heap", ESP.getFreeHeap());
    log_message(LOG_DEBUG, log_buffer);

    num_failures = 0;
    last_success = (time_t) time_client.getEpochTime();

    delay(UPDATE_INTERVAL);
  }
}

void connectWiFi()
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
  time_client.begin();

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
