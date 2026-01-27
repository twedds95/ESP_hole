#ifndef WEBSERVERHELPER_H
#define WEBSERVERHELPER_H

#include <Arduino.h>
#include <DNSOverrideLists.h>
#include <ESPAsyncWebServer.h>
#include <EspLogs.h>

#include <Dashboard.cpp>

#define HOURS 24
// track more domains but only show top 10 in Dashboard
#define TOP_N 10
#define TOP_N_TRACKED 40 // tracking more causes issues with persistence
#define MAX_DOMAIN_LEN 48
#define MAX_IP_LEN 16


struct DnsLogEvent
{
  uint32_t resolveMs;
  uint32_t processMs;
  bool blocked;
  bool wasSentUpstream;
  char domain[MAX_DOMAIN_LEN];
  IPAddress ip;
};

// #Persisted Stats
#include <Preferences.h>
#define STATS_VERSION 1 //max 255
#define STATS_SAVE_ID "esp_hole"
#define STATS_SAVE_KEY "stats"
#define HOUR_SAVE_KEY "hourly"
#define BLOCKS_SAVE_KEY "top_blk"
#define QUERIES_SAVE_KEY "top_qry"

struct DomainStat
{
  char domain[MAX_DOMAIN_LEN];
  uint32_t count;
  bool wasSentUpstream;
  char ip[MAX_IP_LEN];
};

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
void handlePostList(AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total);
void handleListUpdates();
const DomainStat* getTopBlocked();
const DomainStat* getTopQueried();

void savePersistedStats();
void loadPersistedStats();
void setupServerHelper();

void appendTopArray(String &json, DomainStat arr[]);
String getJsonStats();
void handleTimeSensitiveRotations();
void decayTopDomains(DomainStat arr[]);
void recordQuery(bool blocked, const char *domain, bool wasSentUpstream, uint32_t resolveTime, uint32_t procTime, IPAddress ip);
void sanitizeDomain(const char *dom, char *domain);
void updateTopBlocked(const char *domain, bool wasSentUpstream);
void updateTopQueried(const char *domain, bool wasSentUpstream, IPAddress ip);
void updateTop(DomainStat arr[], const char *dom, bool wasSentUpstream, IPAddress ip = IPAddress(0, 0, 0, 0));
void removeFromTopList(DomainStat arr[], const char *dom);

#endif // WEBSERVERHELPER_H