#include <WebServerHelper.h>

AsyncWebServer server(80);

unsigned long lastHourTick = 0;
unsigned long lastPersistedTick = 0;

Preferences prefs;
PersistedStats stats;

#define totalQueries stats._totalQueries
#define totalBlocked stats._totalBlocked
#define totalResponseTime stats._responseTime
#define totalAddedTime stats._processTime
#define hourly stats._hourly
#define currentHour stats._currentHour
#define topBlocked stats._topBlocked
#define topQueried stats._topQueried

struct ListDef
{
  const char *name;
  const char *path;
};

ListDef lists[] = {
    {"blocklist", "/blocklist"},
    {"whitelist", "/whitelist"},
    {"rewrite", "/rewrite"}};

const int LIST_COUNT = sizeof(lists) / sizeof(lists[0]);

void savePersistedStats()
{
  ESP_LOGD(LOG_TAG(ESPHOLE_LOGTYPES::STATS), "Persisting Data");
  prefs.begin(STATS_SAVE_ID, false);
  prefs.putBytes(STATS_SAVE_KEY, &stats, sizeof(stats));
  prefs.end();
}

void loadPersistedStats()
{
  
  ESP_LOGD(LOG_TAG(ESPHOLE_LOGTYPES::STATS), "Loading Persisted Data");
  prefs.begin(STATS_SAVE_ID, true);

  if (prefs.isKey(STATS_SAVE_KEY))
  {
    ESP_LOGD(LOG_TAG(ESPHOLE_LOGTYPES::STATS), "Persisted Data: Found Key");
    prefs.getBytes(STATS_SAVE_KEY, &stats, sizeof(stats));
    if (stats.version != STATS_VERSION)
    {
      ESP_LOGD(LOG_TAG(ESPHOLE_LOGTYPES::STATS), "Persisted Data: New Version");
      memset(&stats, 0, sizeof(stats));
      stats.version = STATS_VERSION;
    }
  }
  else
  {
    ESP_LOGD(LOG_TAG(ESPHOLE_LOGTYPES::STATS), "Persisted Data: Initializing New");
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

void emptyRequestHandler(AsyncWebServerRequest *request) {};
void handleLists()
{
  server.on("/list/blocklist", HTTP_GET, handleGetList);
  server.on("/list/whitelist", HTTP_GET, handleGetList);
  server.on("/list/rewrite", HTTP_GET, handleGetList);

  server.on("/list/blocklist", HTTP_POST, emptyRequestHandler, nullptr, handlePostList);
  server.on("/list/whitelist", HTTP_POST, emptyRequestHandler, nullptr, handlePostList);
  server.on("/list/rewrite", HTTP_POST, emptyRequestHandler, nullptr, handlePostList);
}

const char *getListPath(const String &name)
{
  for (int i = 0; i < LIST_COUNT; i++)
  {
    if (name == lists[i].name)
      return lists[i].path;
  }
  return nullptr;
}

String getListName(AsyncWebServerRequest *req)
{
  String url = req->url();
  return url.substring(url.lastIndexOf('/') + 1);
}

void handleGetList(AsyncWebServerRequest *req)
{
  String name = getListName(req);
  const char *path = getListPath(name);

  if (!path)
  {
    req->send(404, "text/plain", "Invalid list");
    return;
  }

  if (!SPIFFS.exists(path))
  {
    req->send(200, "text/plain", "");
    return;
  }

  File f = SPIFFS.open(path, "r");
  if (!f)
  {
    req->send(500, "text/plain", "File open failed");
    return;
  }

  AsyncWebServerResponse *res =
      req->beginResponse(f, "text/plain");

  res->addHeader("Cache-Control", "no-store");
  req->send(res);
}

void reloadLists()
{
  setupBlockList();
  setupWhiteList();
  setupRewrite();
}

void handlePostList(AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total)
{
  if (index != 0)
    return;

  String name = getListName(req);
  const char *path = getListPath(name);

  if (!path)
  {
    req->send(404, "text/plain", "Invalid list");
    return;
  }

  if (total == 0 || req->contentLength() == 0)
  {
    SPIFFS.remove(path);
    reloadLists();
    req->send(200, "text/plain", "List cleared");
    return;
  }

  const char *tmp = "/tmp_list.txt";

  File f = SPIFFS.open(tmp, "w");
  if (!f)
  {
    req->send(500, "text/plain", "Temp open failed");
    return;
  }

  f.write(data, total);
  f.close();

  SPIFFS.remove(path);
  SPIFFS.rename(tmp, path);

  reloadLists();
  req->send(200, "text/plain", "OK");
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

void handleListUpdates()
{
  handleWhitelistAdds();
  handleBlocklistAdds();
  handleRewriteAdds();
  handleLists();
}

const DomainStat* getTopBlocked()
{
    return topBlocked;
}

const DomainStat* getTopQueried()
{
    return topQueried;
}

void setupServerHelper()
{
  loadPersistedStats();

  handleRoot();
  handleStats();
  handleListUpdates();

  server.begin();
  ESP_LOGI(LOG_TAG(ESPHOLE_LOGTYPES::WIFI), "ESP_hole Dashboard started on: %s", WiFi.localIP());
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

  ESP_LOGD(LOG_TAG(ESPHOLE_LOGTYPES::STATS), "Sending JSON to Dashboard: %s", json);

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
      arr[i].ip = IPAddress(0, 0, 0, 0);
      return;
    }
  }
}