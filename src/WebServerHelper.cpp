#include <WebServerHelper.h>

#include <algorithm>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

#include <Dashboard.cpp>
#include <DNSOverrideLists.h>
#include <EspLogs.h>


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
                sizeof(stats) + sizeof(hourly));
  prefs.begin(STATS_SAVE_ID, false);
  size_t written = 0;
  written += prefs.putBytes(STATS_SAVE_KEY, &stats, sizeof(stats));
  written += prefs.putBytes(HOUR_SAVE_KEY, hourly, sizeof(hourly));
  prefs.end();
  dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                ESPHOLE_LOGTYPES::STATS,
                "Version %d - Persisted %u bytes (stats=%u hourly=%u)",
                STATS_VERSION,
                written,
                sizeof(stats),
                sizeof(hourly));
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
    String fname = String(logName) + (i > 0 ? String(i) : String());
    if (SPIFFS.exists(fname))
    {
      File f = SPIFFS.open(fname, FILE_READ);
      total += f.size();
      serialPrintLogf("[%s] getTotalLogSize: File %s has size %d\n",
                      LOG_TAG(ESPHOLE_LOGTYPES::CODE),
                      fname, f.size());
      f.close();
    }
  }
  return total;
}

void handleLogs()
{
  server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *req)
            {
    size_t totalSize = getTotalLogSize();
    
    size_t limit = req->hasParam("limit") ? 
      req->getParam("limit")->value().toInt():
      2048;
              
    size_t offset;
    if (req->hasParam("offset")) {
      offset = req->getParam("offset")->value().toInt();
    } else {
      offset = (totalSize > limit) ? totalSize - limit : 0;
    }

    struct State {
      int fileIndex = MAX_LOGS - 1;
      size_t skip;
      size_t remaining;
      bool skippedFirstLine = false;
      bool needsLeadingNewline = false;
      File f;
      bool haveCarry = false;
      char carry[256];
      size_t carryLen = 0;
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
            serialPrintLogf("[%s] skip=%d remaining=%d fileIndex=%d\n",
              LOG_TAG(ESPHOLE_LOGTYPES::CODE),
              s->skip, s->remaining, s->fileIndex);
            if (!s->f)
            {
              String fname = String(logName) +
                             (s->fileIndex > 0 ? String(s->fileIndex) : String());
              s->fileIndex--;              
              serialPrintLogf("[%s] HandleLogs: Try to open %s\n",
                LOG_TAG(ESPHOLE_LOGTYPES::CODE), fname.c_str());
              if (!SPIFFS.exists(fname)) continue;
 
              if (s->haveCarry) {
                s->carry[s->carryLen++] = '\n';
              }
 
              s->f = SPIFFS.open(fname, FILE_READ);
              if (!s->f) continue;
              
              if (s->needsLeadingNewline && s->remaining > 0)
              {
                buf[0] = '\n';
                s->needsLeadingNewline = false;
                s->remaining--;
                return 1;
              }

              serialPrintLogf("[%s] HandleLogs: %s file being read.\n",
                LOG_TAG(ESPHOLE_LOGTYPES::CODE), fname.c_str());
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
              size_t out = 0;
              if (s->haveCarry) {                
              serialPrintLogf("[%s] HandleLogs: %s carryover sent to buffer.\n",
                LOG_TAG(ESPHOLE_LOGTYPES::CODE), s->f.name());
                size_t copy = min(maxLen, s->carryLen);
                memcpy(buf, s->carry, copy);
                out += copy;
                if (copy < s->carryLen) {
                  memmove(s->carry, s->carry + copy, s->carryLen - copy);
                  s->carryLen -= copy;
                  return out;
                }

                s->haveCarry = false;
              }

              serialPrintLogf("[%s] HandleLogs: %s file can be read, sent to buffer.\n",
                LOG_TAG(ESPHOLE_LOGTYPES::CODE), s->f.name());
              size_t r = s->f.read(buf + out, canRead - out);
              r += out;

              int lastNL = -1;
              for (int i = r - 1; i >= 0; --i) {
                if (buf[i] == '\n') {
                  lastNL = i;
                  break;
                }
              }

              if (lastNL < 0) {
                // whole chunk is partial line -> stash it
                memcpy(s->carry, buf, r);
                s->carryLen = r;
                s->haveCarry = true;
                return 0;
              }

              // stash trailing partial
              size_t tail = r - (lastNL + 1);
              if (tail > 0) {
                memcpy(s->carry, buf + lastNL + 1, tail);
                s->carryLen = tail;
                s->haveCarry = true;
              }

              size_t emit = lastNL + 1;
              s->remaining -= emit;
              return emit;

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
            if (s->f)
            {
              if (s->f.size() > 0)
              {
                s->f.seek(s->f.size() - 1, SeekSet);
                if (s->f.read() != '\n') {
                  s->needsLeadingNewline = true;
                }
              }
              s->f.close();
            }
          }

          return 0;
        });

    req->onDisconnect([s]() {
      if (s->f) s->f.close();
      delete s;
    });

    res->addHeader("X-Log-Total-Size", String(totalSize));
    req->send(res); });
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
  if (now - lastPersistedTick > 900000UL) // persist data every 15mins
  {
    lastPersistedTick = now;
    savePersistedStats();
    saveTopStats();
  }

  if (now - lastHourTick < 3600000UL)
  {
    return;
  }

  lastHourTick = now;
  currentHour = (currentHour + 1) % HOURS;

  if (totalQueries > 0)
  {
    double_t hourPercentEstimate = (double_t)hourly[currentHour].queries / (double_t)totalQueries;
    decayTopDomains(hourPercentEstimate, totalQueries);
  }

  totalQueries -= hourly[currentHour].queries;
  totalBlocked -= hourly[currentHour].blocked;
  totalResponseTime -= hourly[currentHour].hourResponseTime;
  totalAddedTime -= hourly[currentHour].hourProcessTime;

  hourly[currentHour].queries = 0;
  hourly[currentHour].blocked = 0;
  hourly[currentHour].hourResponseTime = 0;
  hourly[currentHour].hourProcessTime = 0;
}

void appendTopArray(String &json, const std::set<DomainStat, DomainStatCompare> set)
{
  json += "[";
  bool first = true;
  const auto begin = set.begin();
  const auto end = set.size() < TOP_N ? set.end() : std::next(begin, 10);
  for (auto it = begin; it != end; ++it) {
    if (it->count == 0)
      continue;
    if (!first)
      json += ",";
    first = false;
    json += "{";
    json += "\"d\":\"" + String(it->domain) + "\",";
    json += "\"u\":" + String(it->wasSentUpstream) + ",";
    json += "\"c\":" + String(it->count);
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
  appendTopArray(json, getTopQueriedSet());
  json += ",\"blocked\":";
  appendTopArray(json, getTopBlockedSet());
  json += "}";

  json += "}";

  // This one is too big / too often to send to logs, just use Serial
  serialPrintLogf("[%s] Sending JSON to Dashboard:\n %s\n",
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
