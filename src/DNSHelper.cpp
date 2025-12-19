#include <DNSHelper.h>

#include <BloomCheck.h>
#include <DNSOverrideLists.h>
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

IPAddress sendUpstream(const char *dom, IPAddress &ip, uint32_t processMs)
{
    uint32_t oMillis = millis();
    WiFi.hostByName(dom, ip);
    uint32_t resolvMs = millis() - oMillis;
    Serial.print(" | IP:");
    Serial.print(ip);
    if (ip == IPAddress(0, 0, 0, 0))
    {
        Serial.printf("\n Block by upstream took %lu ms", resolvMs);
        Serial.printf(" | Find took %lu ms\n", processMs);
        enqueueDnsLog(true, dom, true, resolvMs, processMs);
    }
    else
    {
        Serial.printf("\nResolv took %lu ms", resolvMs);
        Serial.printf(" | Find took %lu ms\n", processMs);
        enqueueDnsLog(false, dom, true, resolvMs, processMs);
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

    Serial.println();
    Serial.print("Domain: ");
    Serial.print(dom.c_str());
    IPAddress ip;
    uint32_t oMillis = millis();
    if (isRewrite(dom.c_str(), ip))
    {
        uint32_t rewriteMs = millis() - oMillis;
        Serial.print(" | IP:");
        Serial.print(ip);
        Serial.printf("\nRewrite took %lu ms\n", rewriteMs);
        enqueueDnsLog(false, dom.c_str(), false, rewriteMs, 0);
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
        Serial.printf(" Blocked | Find took %lu ms\n", processMs);
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
            recordQuery(ev.blocked, ev.domain, ev.wasSentUpstream, ev.resolveMs, ev.processMs);
        }
    }
}

void enqueueDnsLog(bool blocked, const char *domain, bool wasSentUpstream, uint32_t resolvMs, uint32_t processMs)
{
    if (!dnsLogQueue)
        return;

    DnsLogEvent ev{};
    ev.blocked = blocked;
    ev.resolveMs = resolvMs;
    ev.processMs = processMs;
    ev.wasSentUpstream = wasSentUpstream;
    strncpy(ev.domain, domain, MAX_DOMAIN_LEN - 1);

    // Do NOT block DNS if queue is full
    xQueueSend(dnsLogQueue, &ev, 0);
}