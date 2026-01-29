#ifndef ESPLOGS_H
#define ESPLOGS_H

#include <Arduino.h>
#include <SPIFFS.h>

#define logName "/esplogs"
#define MAX_LOGS 3
#define MAX_LOG_MSG_LEN 256

static const char* const LOG_TAGS[] PROGMEM = {
    "DNS", "WIFI", "BLOOM", "SPIFFS", "STATS", "CODE"
};

static const char* const LOG_LVLS[] PROGMEM = {
    "ERROR", "INFO", "DEBUG",
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

enum class ESPHOLE_LOGLEVEL : uint8_t
{
  ERROR = 0,
  INFO,
  DEBUG,
};

#define LOG_TAG(tag) LOG_TAGS[(uint8_t)(tag)]
#define LOG_LVL(lvl) LOG_LVLS[(uint8_t)(lvl)]

#define SERIAL_DEBUG false
#define serialPrintLogf(...) do { if (SERIAL_DEBUG) Serial.printf(__VA_ARGS__); } while (0)

void dualPrintLogf(ESPHOLE_LOGLEVEL logLevel, ESPHOLE_LOGTYPES tagEnum, const char *fmt, ...);
File rollLog();
void setupLogs(ESPHOLE_LOGLEVEL lvl);

#endif //ESPLOGS_H