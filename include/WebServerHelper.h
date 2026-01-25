#ifndef WEBSERVERHELPER_H
#define WEBSERVERHELPER_H

#include <Arduino.h>
#include <DNSOverrideLists.h>
#include <ESPAsyncWebServer.h>

#include <Dashboard.cpp>

#define HOURS 24
// track more domains but only show top 10 in Dashboard
#define TOP_N 10
#define TOP_N_TRACKED 50
#define MAX_DOMAIN_LEN 48

struct DomainStat
{
  char domain[MAX_DOMAIN_LEN];
  uint32_t count;
  bool wasSentUpstream;
  IPAddress ip;
};

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
#define STATS_VERSION 1
#define STATS_SAVE_ID "esp_hole"
#define STATS_SAVE_KEY "stats"

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
  HourStats _hourly[HOURS];
  uint8_t _currentHour;
  DomainStat _topBlocked[TOP_N_TRACKED];
  DomainStat _topQueried[TOP_N_TRACKED];
};

extern PersistedStats stats;

#define totalQueries stats._totalQueries
#define totalBlocked stats._totalBlocked
#define totalResponseTime stats._responseTime
#define totalAddedTime stats._processTime
#define hourly stats._hourly
#define currentHour stats._currentHour
#define topBlocked stats._topBlocked
#define topQueried stats._topQueried

void handleRoot();
void handleStats();
void handleListAdds();

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