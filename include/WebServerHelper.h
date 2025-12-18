#include <Arduino.h>

#define HOURS 24
// track more domains but only show top 10 in Dashboard
#define TOP_N 10
#define TOP_N_TRACKED 50
#define MAX_DOMAIN_LEN 48

struct DnsLogEvent
{
  uint32_t durationMs;
  bool blocked;
  char domain[MAX_DOMAIN_LEN];
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
  uint32_t hourBlockTime = 0;
};

struct PersistedStats
{
  uint8_t version = STATS_VERSION;
  uint32_t _totalQueries;
  uint32_t _totalBlocked;
  uint64_t _responseTime;
  uint64_t _blockTime;
  HourStats _hourly[HOURS];  
  uint8_t _currentHour;
};

struct DomainStat
{
  char domain[MAX_DOMAIN_LEN];
  uint32_t count;
};

void loadPersistedStats();
void savePersistedStats();

void appendTopArray(String &json, DomainStat arr[]);
String getJsonStats();
void handleTimeSensitiveRotations();
void decayTopDomains(DomainStat arr[]);
void recordQuery(bool blocked, const char *domain, uint32_t respTime);
void sanitizeDomain(const char *dom, char *domain);
void updateTop(DomainStat arr[], const char *dom, uint32_t newTotal);
void updateTopBlocked(const char *domain);
void updateTopQueried(const char *domain);