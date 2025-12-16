#include <WebServerHelper.h>

// #Persisted Stats
Preferences prefs;
PersistedStats stats;

// Data
DomainStat topBlocked[TOP_N];
DomainStat topQueried[TOP_N];
uint8_t currentHour = 0;
unsigned long lastHourTick = 0;


void savePersistedStats()
{
  prefs.begin(STATS_SAVE_ID, false);
  prefs.putBytes(STATS_SAVE_KEY, &stats, sizeof(stats));
  prefs.end();
}

void loadPersistedStats()
{
  prefs.begin(STATS_SAVE_ID, true);

  if (prefs.isKey(STATS_SAVE_KEY))
  {
    prefs.getBytes(STATS_SAVE_KEY, &stats, sizeof(stats));

    if (stats.version != STATS_VERSION)
    {
      memset(&stats, 0, sizeof(stats));
      stats.version = STATS_VERSION;
    }
  }
  else
  {
    memset(&stats, 0, sizeof(stats));
    stats.version = STATS_VERSION;
  }

  prefs.end();
}

// Data
void handleHourRotation()
{
  unsigned long now = millis();
  if (now - lastHourTick < 3600000UL)
  {
    return;
  }

  savePersistedStats();

  lastHourTick = now;
  currentHour = (currentHour + 1) % HOURS;

  totalQueries -= hourly[currentHour].queries;
  totalBlocked -= hourly[currentHour].blocked;

  hourly[currentHour].queries = 0;
  hourly[currentHour].blocked = 0;
}

void appendTopArray(String &json, DomainStat *arr)
{
  json += "[";
  bool first = true;
  for (int i = 0; i < TOP_N; i++)
  {
    if (arr[i].count == 0)
      continue;
    if (!first)
      json += ",";
    first = false;
    json += "{";
    json += "\"d\":\"" + String(arr[i].domain) + "\",";
    json += "\"c\":" + String(arr[i].count);
    json += "}";
  }
  json += "]";
}

String getJsonStats()
{
  String json = "{";

  json += "\"total\":" + String(totalQueries) + ",";
  json += "\"blocked\":" + String(totalBlocked) + ",";

  // Memory
  json += "\"heap\":{";
  json += "\"free\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"min\":" + String(ESP.getMinFreeHeap());
  json += "},";

  // Hourly
  json += "\"hours\":[";
  for (int i = 0; i < HOURS; i++)
  {
    int idx = (currentHour + i + 1) % HOURS;
    json += "{";
    json += "\"q\":" + String(hourly[idx].queries) + ",";
    json += "\"b\":" + String(hourly[idx].blocked);
    json += "}";
    if (i < HOURS - 1)
      json += ",";
  }
  json += "],";

  // Top domains
  json += "\"top\":{";
  json += "\"queried\":";
  appendTopArray(json, topQueried);
  json += ",\"blocked\":";
  appendTopArray(json, topBlocked);
  json += "}";

  json += "}";
  return json;
}

void recordQuery(bool blocked, const char *domain)
{
  totalQueries++;
  hourly[currentHour].queries++;

  if (blocked)
  {
    totalBlocked++;
    hourly[currentHour].blocked++;
    updateTop(topBlocked, domain);
  }
  else
  {
    updateTop(topQueried, domain);
  }
}

void updateTop(DomainStat *arr, const char *domain)
{
  // Check if exists
  for (int i = 0; i < TOP_N; i++)
  {
    if (arr[i].count && strcmp(arr[i].domain, domain) == 0)
    {
      arr[i].count++;
      return;
    }
  }

  // Find empty slot
  for (int i = 0; i < TOP_N; i++)
  {
    if (arr[i].count == 0)
    {
      strncpy(arr[i].domain, domain, MAX_DOMAIN_LEN);
      arr[i].count = 1;
      return;
    }
  }

  // Replace smallest
  int minIdx = 0;
  for (int i = 1; i < TOP_N; i++)
  {
    if (arr[i].count < arr[minIdx].count)
      minIdx = i;
  }

  strncpy(arr[minIdx].domain, domain, MAX_DOMAIN_LEN);
  arr[minIdx].count = 1;
}