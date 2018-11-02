/** @file subway.cpp
 *  @brief Helper functions for decoding MTA GTFS via nanopb.
 *
 *  Implementation of helper functions for decoding MTA GTFS
 *  via nanopb.
 *
 *  @author Bill Bernsen (bill@nycresistor.com)
 *  @bug No know bugs.
 */

#include "subway.h"

int entity_count = 0;
logger_callback logger = NULL;

// Set the log level
enum log_level LOG_LEVEL = LOG_INFO;

/* Register a function to handle logging for your device.
*  This comes in handy when you're switching back and forth
*  between Arduino and Linux.
*/
void register_logger(logger_callback l) {
    logger = l;
}

// Log a message using the registered logger at the set log level
void log_message(enum log_level level, char *message) {
    if (logger == NULL) {
        printf("Use register_logger to register a logging function\n");
        return;
    }

    logger(level, message);
}

// Decodes a trip route id
bool trip_route_id_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
  log_message(LOG_DEBUG, "Decoding route_id\n");
  pb_byte_t route_id[2] = {0};

  struct ArrivalInfo *arrival_info = (struct ArrivalInfo *)*arg;

  if (stream->bytes_left <= 0)
  {
    return true;
  }

  bool ret = pb_read(stream, &(route_id[0]), stream->bytes_left);

  if (!ret)
  {
    log_message(LOG_ERROR, "Bad trip route id\n");
    //Serial.println("Decode failed: " + String(PB_GET_ERROR(stream)));
    return false;
  }

  log_message(LOG_DEBUG, "Writing route_id\n");
  strlcpy((char *)arrival_info->train_type, (char *)&(route_id[0]), sizeof(route_id));
  log_message(LOG_DEBUG, "Done writing route_id\n");
  return true;
}

// Decodes a stop id
bool stop_id_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
  log_message(LOG_DEBUG, "Decoding stop_id\n");
  pb_byte_t stop_id[8] = {0};

  struct ArrivalInfo *arrival_info = (struct ArrivalInfo *)*arg;

  if (stream->bytes_left <= 0)
  {
    return true;
  }

  bool ret = pb_read(stream, &(stop_id[0]), stream->bytes_left);
  if (!ret)
  {
    log_message(LOG_ERROR, "Bad stop id\n");
    //Serial.println("Decode failed: " + String(PB_GET_ERROR(stream)));
    return false;
  }

  log_message(LOG_DEBUG, "Writing stop_id\n");
  strlcpy((char *)&(arrival_info->station), (char *)&(stop_id[0]), sizeof(stop_id));
  log_message(LOG_DEBUG, "Done writing stop_id\n");

  return true;
}

// Decodes a stop time updatek
bool stop_time_update_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
  log_message(LOG_DEBUG, "Decoding stop_time_update\n");

  transit_realtime_TripUpdate_StopTimeUpdate stop_time_update = {};
  memset(&stop_time_update, 0, sizeof(stop_time_update));

  struct ArrivalInfo *arrival_info = (struct ArrivalInfo *)*arg;

  stop_time_update.stop_id.funcs.decode = stop_id_callback;
  stop_time_update.stop_id.arg = arrival_info;

  // Add extensions
  pb_extension_t ext;
  memset(&ext, 0, sizeof(ext));
  NyctStopTimeUpdate update;
  memset(&update, 0, sizeof(update));
  stop_time_update.extensions = &ext;
  ext.type = &nyct_stop_time_update;
  ext.dest = &update;

  bool ret = pb_decode(stream, transit_realtime_TripUpdate_StopTimeUpdate_fields, &stop_time_update);

  if (!ret)
  {
    log_message(LOG_DEBUG, "Bad stop time update\n");
   // Serial.println("Decode failed: " + String(PB_GET_ERROR(stream)));
    return false;
  }

  for (int i = 0; i < (sizeof(STATION_LIST) / sizeof(STATION_LIST[0])); i++)
  {
    if (strcmp((char *)arrival_info->station, STATION_LIST[i]) == 0)
    {
      log_message(LOG_DEBUG, "Matched a station\n");
      if (stop_time_update.has_arrival)
      {
        log_message(LOG_DEBUG, "Writing arrival\n");
        arrival_info->arrival = stop_time_update.arrival.time;
        //printf("Arrives: %s", ctime((time_t*) &arrival_info->arrival));
      }
      if (stop_time_update.has_departure)
      {
        log_message(LOG_DEBUG, "Writing departure\n");
        arrival_info->departure = stop_time_update.departure.time;
        //printf("Departs: %s", ctime((time_t*) &arrival_info->departure));
      }
    }
  }

  return true;
}

// Print a vector of ArrivalInfo
void print_arrival_list(const std::vector<ArrivalInfo> &arrival_list)
{
  char message[256];
  for (const auto &arr_info : arrival_list)
  {
    snprintf(message, 256, "%s train departs %s at %s",
        (char *) &arr_info.train_type[0],
        STOP_NAME,
        ctime((time_t *) &arr_info.departure));
    log_message(LOG_INFO, message);
  }
}

// Top level callback to register with nanopb. Returns the passed vector of
// ArrivalInfo filled out.
bool entity_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{

  log_message(LOG_DEBUG, "Decoding entity\n");
  entity_count++;
  transit_realtime_FeedEntity feedentity = {};

  std::vector<ArrivalInfo> *arrival_list = reinterpret_cast<std::vector<ArrivalInfo> *>(*arg);
  struct ArrivalInfo arrival_info = {};

  feedentity.trip_update.stop_time_update.funcs.decode = stop_time_update_callback;
  feedentity.trip_update.stop_time_update.arg = (void *) &arrival_info;

  feedentity.trip_update.trip.route_id.funcs.decode = trip_route_id_callback;
  feedentity.trip_update.trip.route_id.arg = (void *) &arrival_info;

  bool ret = pb_decode(stream, transit_realtime_FeedEntity_fields, &feedentity);
  if (!ret)
  {
    log_message(LOG_DEBUG, "Bad entity\n");
    //Serial.println("Decode failed: " + String(PB_GET_ERROR(stream)));
    return false;
  }

  if (arrival_info.departure != 0)
  {
    log_message(LOG_DEBUG, "Found a match\n");
    arrival_list->push_back(arrival_info);
  }

  return true;
}