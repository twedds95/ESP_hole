#ifndef ESPLOGS_H
#define ESPLOGS_H

#include <Arduino.h>
#include <SPIFFS.h>

static const char* const LOG_TAGS[] PROGMEM = {
    "DNS", "WIFI", "BLOOM", "SPIFFS", "STATS", "CODE"
};

enum class ESPHOLE_LOGTYPES : uint8_t
{
  DNS = 0,
  WIFI,
  BLOOM,
  SPIFFS,
  STATS,
  CODE,  
};

#define LOG_TAG(tag) LOG_TAGS[(uint8_t)(tag)]

void setupLogs(esp_log_level_t level);

#endif //ESPLOGS_H