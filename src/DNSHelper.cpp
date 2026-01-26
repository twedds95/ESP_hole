#include <DNSHelper.h>

#include <BloomCheck.h>
#include <DNSOverrideLists.h>
#include <EspLogs.h>
#include <WebServerHelper.h>
#include <WiFi.h>

QueueHandle_t dnsLogQueue;

bool isEasyBlock(const char *domain)
{
    static const char *patterns[] = {
        "ad.",
        ".ad.",
        ".ads.",
        "adserver.",
        "adservers.",
        "adtrack.",
        "adtracker.",
        "adservice.",
        "adservices.",
        "analytics.",
        "telemetry.",
        "tracker.",
        "tracking.",
        "beacon.",
        "logging."};

    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++)
    {
        if (strstr(domain, patterns[i]) != nullptr)
        {
            return true;
        }
    }

    return false;
}

bool isBlockedOverride(const char *domain)
{
    for (auto &b : getBlockList())
    {
        if (b.equalsIgnoreCase(domain))
            return true;
    }
    return false;
}

bool isWhiteListOverride(const char *domain)
{
    for (auto &b : getWhiteList())
    {
        if (b.equalsIgnoreCase(domain))
            return true;
    }
    return false;
}

bool isRewrite(const char *domain, IPAddress &ip)
{
    for (auto r : getRewriteRules())
    {
        if (strstr(domain, r.domain.c_str()) != nullptr)
        {
            ip = r.ip;
            return true;
        }
    }

    return false;
}

bool isCached(const char *domain, IPAddress &ip)
{
    const DomainStat *topQueried = getTopQueried();
    for (int i = 0; i < TOP_N_TRACKED; i++)
    {
        if (topQueried[i].count && strcmp(topQueried[i].domain, domain) == 0)
        {
            ip = topQueried[i].ip;
            return true;
        }
    }  
    
    const DomainStat *topBlocked = getTopBlocked();
    for (int i = 0; i < TOP_N_TRACKED; i++)
    {
        if (topBlocked[i].count && strcmp(topBlocked[i].domain, domain) == 0)
        {
            ip = IPAddress(0,0,0,0);
            return true;
        }
    }

    return false;
}

IPAddress sendUpstream(const char *dom, IPAddress &ip, uint32_t processMs)
{
    uint32_t oMillis = millis();
    WiFi.hostByName(dom, ip);
    uint32_t resolvMs = millis() - oMillis;
    ESP_LOGD(LOG_TAG(ESPHOLE_LOGTYPES::DNS), "IP: %s", ip);
    if (ip == IPAddress(0, 0, 0, 0))
    {
        ESP_LOGD(LOG_TAG(ESPHOLE_LOGTYPES::DNS), "Block by upstream took %lu ms | Find took %lu ms", resolvMs, processMs);
        enqueueDnsLog(true, dom, true, resolvMs, processMs);
    }
    else
    {
        ESP_LOGD(LOG_TAG(ESPHOLE_LOGTYPES::DNS), "Resolv took %lu ms | Find took %lu ms", resolvMs, processMs);
        enqueueDnsLog(false, dom, true, resolvMs, processMs, ip);
    }

    return ip;
}

IPAddress handleDNSRequest(String dom)
{
    if (dom.startsWith("www."))
    {
        dom.remove(0, 4);
    }

    if ((dom.isEmpty()))
    {
        return IPAddress(0, 0, 0, 0);
    }

    ESP_LOGD(LOG_TAG(ESPHOLE_LOGTYPES::DNS), "Domain: %s", dom);
    IPAddress ip;
    uint32_t oMillis = millis();
    if (isRewrite(dom.c_str(), ip))
    {
        uint32_t rewriteMs = millis() - oMillis;
        ESP_LOGD(LOG_TAG(ESPHOLE_LOGTYPES::DNS), "IP: %s", ip);
        ESP_LOGD(LOG_TAG(ESPHOLE_LOGTYPES::DNS), "Rewrite took %lu ms", rewriteMs);
        enqueueDnsLog(false, dom.c_str(), false, rewriteMs, 0, ip);
        return ip;
    }

    if (isCached(dom.c_str(), ip))
    {
        uint32_t cacheLookupMs = millis() - oMillis;
        ESP_LOGD(LOG_TAG(ESPHOLE_LOGTYPES::DNS), "IP: %s", ip);
        ESP_LOGD(LOG_TAG(ESPHOLE_LOGTYPES::DNS), "Find in cache took %lu ms", cacheLookupMs);
        enqueueDnsLog(ip == IPAddress(0, 0, 0, 0), dom.c_str(), false, cacheLookupMs, 0, ip);
        return ip;
    }

    if (isWhiteListOverride(dom.c_str()))
    {
        return sendUpstream(dom.c_str(), ip, millis() - oMillis);
    }

    bool isBlock = isBlockedOverride(dom.c_str()) || isEasyBlock(dom.c_str()) || bloomCheck(dom.c_str());
    uint32_t processMs = millis() - oMillis;
    if (isBlock)
    {
        ESP_LOGD(LOG_TAG(ESPHOLE_LOGTYPES::DNS), "Blocked | Find took %lu ms", processMs);
        enqueueDnsLog(true, dom.c_str(), false, processMs, 0);
        return IPAddress(0, 0, 0, 0);
    }

    return sendUpstream(dom.c_str(), ip, millis() - oMillis);
}

void setupDNSHelper()
{
    setupLogQueue();
}

void setupLogQueue()
{
    dnsLogQueue = xQueueCreate(64, sizeof(DnsLogEvent));

    xTaskCreatePinnedToCore(
        statsTask,
        "stats",
        4096,
        nullptr,
        1,
        nullptr,
        0 // core 0 (DNS usually runs on core 1)
    );
}

void statsTask(void *arg)
{
    DnsLogEvent ev;

    for (;;)
    {
        if (xQueueReceive(dnsLogQueue, &ev, portMAX_DELAY))
        {
            recordQuery(ev.blocked, ev.domain, ev.wasSentUpstream, ev.resolveMs, ev.processMs, ev.ip);
        }
    }
}

void enqueueDnsLog(bool blocked, const char *domain, bool wasSentUpstream, uint32_t resolvMs, uint32_t processMs, IPAddress ip)
{
    if (!dnsLogQueue)
        return;

    DnsLogEvent ev{};
    ev.blocked = blocked;
    ev.resolveMs = resolvMs;
    ev.processMs = processMs;
    ev.wasSentUpstream = wasSentUpstream;
    ev.ip = ip;
    strncpy(ev.domain, domain, MAX_DOMAIN_LEN - 1);

    // Do NOT block DNS if queue is full
    xQueueSend(dnsLogQueue, &ev, 0);
}