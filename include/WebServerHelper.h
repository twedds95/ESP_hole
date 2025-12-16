#include <Arduino.h>

#define HOURS 24
#define TOP_N 20
#define MAX_DOMAIN_LEN 48

// #Persisted Stats
#include <Preferences.h>
#define STATS_VERSION 1
#define STATS_SAVE_ID "esp_hole"
#define STATS_SAVE_KEY "stats"

struct HourStats
{
  uint32_t queries = 0;
  uint32_t blocked = 0;
};

struct PersistedStats
{
  uint8_t version = STATS_VERSION;
  uint32_t _totalQueries;
  uint32_t _totalBlocked;
  HourStats _hourly[HOURS];
};

struct DomainStat
{
  char domain[MAX_DOMAIN_LEN];
  uint32_t count;
};

void loadPersistedStats();
void savePersistedStats();

#define totalQueries stats._totalQueries
#define totalBlocked stats._totalBlocked
#define hourly stats._hourly


void appendTopArray(String &json, DomainStat *arr);
String getJsonStats();
void handleHourRotation();
void recordQuery(bool blocked, const char *domain);
void updateTop(DomainStat *arr, const char *domain);