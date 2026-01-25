#include <WebServerHelper.h>

AsyncWebServer server(80);

unsigned long lastHourTick = 0;
unsigned long lastPersistedTick = 0;

// #Persisted Stats
Preferences prefs;
PersistedStats stats;

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

void handleRoot()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/html", DASHBOARD_HTML); });
}

void handleStats()
{
  server.on("/stats", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "application/json", getJsonStats()); });
}

void handleWhitelistAdds()
{
  server.on("/whitelist/add", HTTP_POST, [](AsyncWebServerRequest *req)
            {
      if (!req->hasParam("domain", true)) {
        req->send(400, "text/plain", "Missing domain");
        return;
      }

      String d = req->getParam("domain", true)->value();
      d.toLowerCase();
      d.trim();

      if (addWhiteListEntry(d))
      {        
        req->send(200, "text/plain", "OK");
        removeFromTopList(topBlocked, d.c_str());
      }
      else {
        req->send(500, "text/plain", "Error updating rewrite list");
      } });
}

void handleBlocklistAdds()
{
  server.on("/blocklist/add", HTTP_POST, [](AsyncWebServerRequest *req)
            {
      if (!req->hasParam("domain", true)) {
        req->send(400, "text/plain", "Missing domain");
        return;
      }
      
      String d = req->getParam("domain", true)->value();
      d.toLowerCase();
      d.trim();

      if (addBlockListEntry(d))
      {        
        req->send(200, "text/plain", "OK");
        removeFromTopList(topQueried, d.c_str());
      }
      else {
        req->send(500, "text/plain", "Error updating block list");
      } });
}

void handleRewriteAdds()
{
  server.on("/rewrite/add", HTTP_POST, [](AsyncWebServerRequest *req)
            {
      if (!req->hasParam("domain", true)) {
        req->send(400, "text/plain", "Missing domain");
        return;
      } 

      if (!req->hasParam("ip", true)) {
        req->send(400, "text/plain", "Missing rewrite IP");
        return;
      }
      
      String d = req->getParam("domain", true)->value();
      d.toLowerCase();
      d.trim();
      String ip = req->getParam("ip", true)->value();

      
      if (addRewriteRule(d.c_str(), ip.c_str()))
      {        
        req->send(200, "text/plain", "OK"); 
      }
      else {
        req->send(500, "text/plain", "Error updating rewrite list");
      } });
}

void handleListAdds()
{
  handleWhitelistAdds();
  handleBlocklistAdds();
  handleRewriteAdds();
}

void setupServerHelper()
{
  loadPersistedStats();

  handleRoot();
  handleStats();
  handleListAdds();

  server.begin();
  Serial.println("ESP_hole Dashboard started on:");
  Serial.println(WiFi.localIP());
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
  totalAddedTime -= hourly[currentHour].hourProcessTime;

  hourly[currentHour].queries = 0;
  hourly[currentHour].blocked = 0;
  hourly[currentHour].hourResponseTime = 0;
  hourly[currentHour].hourProcessTime = 0;
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
    json += "\"u\":" + String(arr[i].wasSentUpstream) + ",";
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
  json += "\"processTime\":" + String(totalAddedTime) + ",";

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

void recordQuery(bool blocked, const char *domain, bool wasSentUpstream, uint32_t resolveTime, uint32_t procTime, IPAddress ip)
{
  totalQueries++;
  hourly[currentHour].queries++;
  totalResponseTime += (resolveTime + procTime);
  hourly[currentHour].hourResponseTime += (resolveTime + procTime);
  totalAddedTime += procTime;
  hourly[currentHour].hourProcessTime += procTime;

  // procTime of 0 means query was not sent to upstream
  if (blocked)
  {
    totalBlocked++;
    hourly[currentHour].blocked++;
    updateTopBlocked(domain, wasSentUpstream);
  }
  else
  {
    updateTopQueried(domain, wasSentUpstream, ip);
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

void updateTopBlocked(const char *domain, bool wasSentUpstream)
{
  updateTop(topBlocked, domain, wasSentUpstream);
}

void updateTopQueried(const char *domain, bool wasSentUpstream, IPAddress ip)
{
  updateTop(topQueried, domain, wasSentUpstream, ip);
}

void updateTop(DomainStat arr[], const char *dom, bool wasSentUpstream, IPAddress ip)
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
      arr[i].wasSentUpstream = wasSentUpstream;
      arr[i].ip = ip;
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
  arr[minIdx].wasSentUpstream = wasSentUpstream;
  arr[minIdx].ip = ip;
}

void removeFromTopList(DomainStat arr[], const char *dom)
{
  char domain[MAX_DOMAIN_LEN];
  sanitizeDomain(dom, domain);
  // Check if exists
  for (int i = 0; i < TOP_N_TRACKED; i++)
  {
    if (arr[i].count && strcmp(arr[i].domain, domain) == 0)
    {
      strncpy(arr[i].domain, "", MAX_DOMAIN_LEN);
      arr[i].count = 0;
      arr[i].wasSentUpstream = true;
      arr[i].ip = IPAddress(0,0,0,0);
      return;
    }
  }
}