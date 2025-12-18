#include <WebServerHelper.h>

// Data
DomainStat topBlocked[TOP_N_TRACKED];
DomainStat topQueried[TOP_N_TRACKED];
uint8_t currentHour = 0;
unsigned long lastHourTick = 0;
unsigned long lastPersistedTick = 0;

// #Persisted Stats
Preferences prefs;
PersistedStats stats;

#define totalQueries stats._totalQueries
#define totalBlocked stats._totalBlocked
#define totalResponseTime stats._responseTime
#define totalBlockTime stats._blockTime
#define hourly stats._hourly
#define currentHour stats._currentHour

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

void handleTimeSensitiveRotations()
{
  unsigned long now = millis();
  if (now - lastPersistedTick > 600000UL) // persist data every 10mins
  {
    lastPersistedTick = now;
    savePersistedStats();
  }

  if (now - lastHourTick < 3600000UL)
  {
    return;
  }

  lastHourTick = now;
  currentHour = (currentHour + 1) % HOURS;

  decayTopDomains(topBlocked);
  decayTopDomains(topQueried);

  totalQueries -= hourly[currentHour].queries;
  totalBlocked -= hourly[currentHour].blocked;
  totalResponseTime -= hourly[currentHour].hourResponseTime;
  totalBlockTime -= hourly[currentHour].hourBlockTime;

  hourly[currentHour].queries = 0;
  hourly[currentHour].blocked = 0;
  hourly[currentHour].hourResponseTime = 0;
  hourly[currentHour].hourBlockTime = 0;
}

void decayTopDomains(DomainStat arr[])
{
  double_t hourPercentEstimate = (double_t)hourly[currentHour].queries / (double_t)totalQueries;
  for (int i = 0; i < TOP_N_TRACKED; i++)
  {
    if (arr[i].count <= 0)
    {
      continue;
    }

    double_t domPercentEstimate = (double_t)arr[i].count / (double_t)totalQueries;
    uint32_t decayAmount = (uint32_t)(domPercentEstimate * hourPercentEstimate * arr[i].count);
    arr[i].count -= decayAmount;
  }
}

void appendTopArray(String &json, DomainStat arr[])
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
  json += "\"responseTime\":" + String(totalResponseTime) + ",";
  json += "\"blockTime\":" + String(totalBlockTime) + ",";

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
    json += "\"b\":" + String(hourly[idx].blocked) + ",";
    json += "\"t\":" + String(hourly[idx].hourResponseTime);
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

  Serial.println("Sending JSON to Dashboard:");
  Serial.println(json);

  return json;
}

void recordQuery(bool blocked, const char *domain, uint32_t respTime)
{
  totalQueries++;
  hourly[currentHour].queries++;
  totalResponseTime += respTime;
  hourly[currentHour].hourResponseTime += respTime;

  if (blocked)
  {
    totalBlocked++;
    hourly[currentHour].blocked++;
    totalBlockTime += respTime;
    hourly[currentHour].hourBlockTime += respTime;
    updateTopBlocked(domain);
  }
  else
  {
    updateTopQueried(domain);
  }
}

void sanitizeDomain(const char *in, char *domain)
{
  size_t used = 0;
  while (*in && used + 1 < MAX_DOMAIN_LEN)
  {
    char c = *in++;
    if ((c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '.')
    {
      domain[used++] = c;
    }
  }

  domain[used] = '\0';
}

void updateTopBlocked(const char *domain)
{
  updateTop(topBlocked, domain, totalBlocked);
}

void updateTopQueried(const char *domain)
{
  updateTop(topQueried, domain, totalQueries - totalBlocked);
}

void updateTop(DomainStat arr[], const char *dom, uint32_t newTotal)
{
  char domain[MAX_DOMAIN_LEN];
  sanitizeDomain(dom, domain);
  // Check if exists
  for (int i = 0; i < TOP_N_TRACKED; i++)
  {
    if (arr[i].count && strcmp(arr[i].domain, domain) == 0)
    {
      arr[i].count++;
      return;
    }
  }

  // Find empty slot
  for (int i = 0; i < TOP_N_TRACKED; i++)
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
  for (int i = 1; i < TOP_N_TRACKED; i++)
  {
    if (arr[i].count < arr[minIdx].count)
      minIdx = i;
  }

  strncpy(arr[minIdx].domain, domain, MAX_DOMAIN_LEN);
  arr[minIdx].count = 1;
}