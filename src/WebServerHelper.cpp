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
#define currentHour stats._currentHour

HourStats hourly[HOURS];
DomainStat topBlocked[TOP_N_TRACKED];
DomainStat topQueried[TOP_N_TRACKED];

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
  dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                ESPHOLE_LOGTYPES::STATS,
                "Size of data to persist = %u",
                sizeof(stats) + sizeof(hourly) + sizeof(topBlocked) + sizeof(topQueried));
  prefs.begin(STATS_SAVE_ID, false);
  size_t written = 0;
  written += prefs.putBytes(STATS_SAVE_KEY, &stats, sizeof(stats));
  written += prefs.putBytes(HOUR_SAVE_KEY, hourly, sizeof(hourly));
  written += prefs.putBytes(BLOCKS_SAVE_KEY, topBlocked, sizeof(topBlocked));
  written += prefs.putBytes(QUERIES_SAVE_KEY, topQueried, sizeof(topQueried));
  prefs.end();
  dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                ESPHOLE_LOGTYPES::STATS,
                "Version %d - Persisted %u bytes (stats=%u hourly=%u blocks=%u queries=%u)",
                STATS_VERSION,
                written,
                sizeof(stats),
                sizeof(hourly),
                sizeof(topBlocked),
                sizeof(topQueried));
}

void loadPersistedStats()
{
  dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                ESPHOLE_LOGTYPES::STATS,
                "Loading Persisted Data");
  prefs.begin(STATS_SAVE_ID, true);
  if (!prefs.isKey(STATS_SAVE_KEY))
  {
    dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                  ESPHOLE_LOGTYPES::STATS,
                  "Persisted Data: No Key - Initializing New");
    memset(&stats, 0, sizeof(stats));
    stats.version = STATS_VERSION;
    prefs.end();
    savePersistedStats();
    return;
  }

  prefs.getBytes(STATS_SAVE_KEY, &stats, sizeof(stats));
  if (stats.version != STATS_VERSION)
  {
    dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                  ESPHOLE_LOGTYPES::STATS,
                  "Persisted Data: New Version %d, Old Version: %d",
                  STATS_VERSION,
                  stats.version);
    memset(&stats, 0, sizeof(stats));
    stats.version = STATS_VERSION;
    prefs.clear();
    prefs.end();
    savePersistedStats();
    return;
  }

  prefs.getBytes(HOUR_SAVE_KEY, hourly, sizeof(hourly));
  prefs.getBytes(BLOCKS_SAVE_KEY, topBlocked, sizeof(topBlocked));
  prefs.getBytes(QUERIES_SAVE_KEY, topQueried, sizeof(topQueried));

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

size_t getTotalLogSize()
{
  size_t total = 0;
  for (int i = 0; i < MAX_LOGS; i++)
  {
    String fname = String(logName) + (i > 0 ? String(i) : "");
    if (SPIFFS.exists(fname))
    {
      File f = SPIFFS.open(fname, FILE_READ);
      total += f.size();
      f.close();
    }
  }
  return total;
}

void handleLogs()
{
  server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    size_t offset = req->hasParam("offset")
                      ? req->getParam("offset")->value().toInt()
                      : 0;

    size_t limit = req->hasParam("limit")
                      ? req->getParam("limit")->value().toInt()
                      : 2048;

    struct State {
      int fileIndex = MAX_LOGS - 1;
      size_t skip;
      size_t remaining;
      bool skippedFirstLine = false;
      File f;
    };

    constexpr size_t LINE_PAD = 256;
    size_t alignedOffset = (offset > LINE_PAD) ? offset - LINE_PAD : 0;

    State *s = new State();
    s->skip = alignedOffset;
    s->remaining = limit;

    AsyncWebServerResponse *res =
      req->beginChunkedResponse("text/plain",
        [s, offset](uint8_t *buf, size_t maxLen, size_t) -> size_t
        {
          while (s->fileIndex >= 0 && s->remaining > 0)
          {
            if (!s->f)
            {
              String fname = String(logName) +
                             (s->fileIndex > 0 ? String(s->fileIndex) : "");
              s->fileIndex--;

              if (!SPIFFS.exists(fname)) continue;

              s->f = SPIFFS.open(fname, FILE_READ);
              if (!s->f) continue;

              if (s->skip > 0)
              {
                size_t skipNow = min(s->skip, s->f.size());
                s->f.seek(skipNow, SeekSet);
                s->skip -= skipNow;
              }
            }

            if (!s->skippedFirstLine && s->skip == 0)
            {
              if (offset > 0) {
                while (s->f.available()) {
                  if (s->f.read() == '\n') break;
                }
              }
              s->skippedFirstLine = true;
            }

            size_t canRead = min({maxLen, s->remaining,
                                  (size_t)s->f.available()});
            if (canRead > 0)
            {
              size_t r = s->f.read(buf, canRead);
              if (s->remaining <= maxLen)
              {
                for (int i = r - 1; i >= 0; --i) {
                  if (buf[i] == '\n') {
                    r = i + 1;
                    break;
                  }
                }
              }

              s->remaining -= r;
              return r;
            }
            if (s->f) s->f.close();
          }

          return 0;
        });

    req->onDisconnect([s]() {
      if (s->f) s->f.close();
      delete s;
    });

    res->addHeader("X-Log-Total-Size",
                   String(getTotalLogSize()));

    req->send(res);
  });
}

void emptyRequestHandler(AsyncWebServerRequest *request) {};
void handleLists()
{
  server.on("/list/blocklist", HTTP_GET, handleGetList);
  server.on("/list/whitelist", HTTP_GET, handleGetList);
  server.on("/list/rewrite", HTTP_GET, handleGetList);

  server.on("/list/clear/blocklist", HTTP_GET, handleClearList);
  server.on("/list/clear/whitelist", HTTP_GET, handleClearList);
  server.on("/list/clear/rewrite", HTTP_GET, handleClearList);

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

  File f = SPIFFS.open(path, FILE_READ);
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

void handleClearList(AsyncWebServerRequest *req)
{
  String name = getListName(req);
  const char *path = getListPath(name);

  if (!path)
  {
    req->send(404, "text/plain", "Invalid list");
    return;
  }

  SPIFFS.remove(path);
  reloadLists();
  req->send(200, "text/plain", "List cleared");
  dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                ESPHOLE_LOGTYPES::STATS,
                "List %s has been cleared.",
                name);
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
        removeFromTopBlock(d.c_str());
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
        removeFromTopQuery(d.c_str());
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

const DomainStat *getTopBlocked()
{
  return topBlocked;
}

const DomainStat *getTopQueried()
{
  return topQueried;
}

void setupServerHelper()
{
  loadPersistedStats();

  handleRoot();
  handleStats();
  handleLogs();
  handleListUpdates();

  server.begin();
  dualPrintLogf(ESPHOLE_LOGLEVEL::INFO,
                ESPHOLE_LOGTYPES::WIFI,
                "ESP_hole Dashboard started on: %s",
                WiFi.localIP().toString().c_str());
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
  std::sort(arr, arr + TOP_N_TRACKED, [](const DomainStat &a, const DomainStat &b)
            { return a.count > b.count; });

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

  // This one is too big / too often to send to logs, just use Serial
  Serial.printf("[%s] Sending JSON to Dashboard:\n %s\n",
                LOG_TAG(ESPHOLE_LOGTYPES::STATS), json.c_str());

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
      strncpy(arr[i].ip, ip.toString().c_str(), MAX_IP_LEN);
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
  strncpy(arr[minIdx].ip, ip.toString().c_str(), MAX_IP_LEN);
}

void removeFromTopQuery(const char *dom)
{
  char domain[MAX_DOMAIN_LEN];
  strncpy(domain, dom, MAX_DOMAIN_LEN);
  removeFromTopList(topQueried, dom)
      ? dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                      ESPHOLE_LOGTYPES::STATS,
                      "Domain '%s' was removed from top queried cached list.",
                      domain)
      : dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                      ESPHOLE_LOGTYPES::STATS,
                      "Domain '%s' was not found in top queried cached list.",
                      domain);
}

void removeFromTopBlock(const char *dom)
{
  char domain[MAX_DOMAIN_LEN];
  strncpy(domain, dom, MAX_DOMAIN_LEN);
  removeFromTopList(topBlocked, dom)
      ? dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                      ESPHOLE_LOGTYPES::STATS,
                      "Domain '%s' was removed from top blocked cached list.",
                      domain)
      : dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                      ESPHOLE_LOGTYPES::STATS,
                      "Domain '%s' was not found in top blocked cached list.",
                      domain);
}

bool removeFromTopList(DomainStat arr[], const char *dom)
{
  char domain[MAX_DOMAIN_LEN];
  sanitizeDomain(dom, domain);
  // Check if exists
  for (int i = 0; i < TOP_N_TRACKED; i++)
  {
    if (strcmp(arr[i].domain, domain) == 0)
    {
      strncpy(arr[i].domain, "", MAX_DOMAIN_LEN);
      arr[i].count = 0;
      arr[i].wasSentUpstream = true;
      strncpy(arr[i].ip, (IPAddress(0, 0, 0, 0)).toString().c_str(), MAX_IP_LEN);
      return true;
    }
  }

  return false;
}