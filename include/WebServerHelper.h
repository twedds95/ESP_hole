#ifndef WEBSERVERHELPER_H
#define WEBSERVERHELPER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include <DNSTopDomainLists.h>
#include <EspLogs.h>


#define HOURS 24
// track more domains but only show top 10 in Dashboard
#define TOP_N 10

// #Persisted Stats
#include <Preferences.h>
#define STATS_VERSION 1 //max 255
#define STATS_SAVE_ID "esp_hole"
#define STATS_SAVE_KEY "stats"
#define HOUR_SAVE_KEY "hourly"

struct HourStats
{
  uint32_t queries = 0;
  uint32_t blocked = 0;
  uint32_t hourResponseTime = 0;
  uint32_t hourProcessTime = 0;
};

struct PersistedStats
{
  uint8_t version = STATS_VERSION;
  uint32_t _totalQueries;
  uint32_t _totalBlocked;
  uint64_t _responseTime;
  uint64_t _processTime;
  uint8_t _currentHour;
};

void handleRoot();
void handleStats();
void handleLogs();
void emptyRequestHandler(AsyncWebServerRequest * request);
void handleLists();
const char *getListPath(const String &name);
String getListName(AsyncWebServerRequest *req);
void handleGetList(AsyncWebServerRequest *req);
void reloadLists();
void handleClearList(AsyncWebServerRequest *req);
void handlePostList(AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total);
void handleListUpdates();

void savePersistedStats();
void loadPersistedStats();
void setupServerHelper();

void appendTopArray(String &json, const std::set<DomainStat, DomainStatCompare> set);
String getJsonStats();
void handleTimeSensitiveRotations();
void recordQuery(bool blocked, const char *domain, bool wasSentUpstream, uint32_t resolveTime, uint32_t procTime, IPAddress ip);

#endif // WEBSERVERHELPER_H