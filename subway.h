#ifndef SUBWAY_H
#define SUBWAY_H

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <vector>
#include <algorithm>

#include "pb_decode.h"
#include "gtfs-realtime.pb.h"
#include "nyct-subway.pb.h"

enum log_level {LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG};

extern const char *STATION_LIST[4];
extern const char STOP_NAME[];
extern int FEED_NUMBER;
extern const char API_KEY[];
extern enum log_level LOG_LEVEL;

extern int entity_count;
typedef void (*logger_callback)(enum log_level, char *message);
extern logger_callback logger;

struct ArrivalInfo
{
  pb_byte_t station[8];
  pb_byte_t train_type[2];
  uint64_t arrival;
  uint64_t departure;
};

void print_arrival_list(const std::vector<ArrivalInfo> &arrival_list);

void register_logger(logger_callback l);
void log_message(enum log_level level, char *message);
struct ArrivalInfo *get_arrivals(char *protobuf);
bool entity_callback(pb_istream_t *stream, const pb_field_t *field, void **arg);

#endif /* SUBWAY_H */