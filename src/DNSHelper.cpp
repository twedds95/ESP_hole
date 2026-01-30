#include <DNSHelper.h>

#include <BloomCheck.h>
#include <DNSOverrideLists.h>
#include <EspLogs.h>
#include <SecondCoreLoop.h>
#include <WebServerHelper.h>
#include <WiFi.h>

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
    auto blockList = getBlockList();
    return blockList.find(domain) != blockList.end();
}

bool isWhiteListOverride(const char *domain)
{
    auto whiteList = getWhiteList();
    return whiteList.find(domain) != whiteList.end();
}

bool isRewrite(const char *domain, IPAddress &ip)
{
    auto rewriteRules = getRewriteRules();
    // try exact match first
    auto it = rewriteRules.find(domain);
    if (it != rewriteRules.end())
    {
        ip = it->second;
        return true;
    }

    // check for partial matches
    for (auto &r : rewriteRules)
    {
        if (strstr(domain, r.first.c_str()) != nullptr)
        {
            ip = r.second;
            return true;
        }
    }

    return false;
}

bool isCached(const char *domain, IPAddress &ip)
{
    auto topQueried = getTopQueriedMap();
    auto it = topQueried.find(domain);
    if (it != topQueried.end())
    {
        DomainStat& stat = it->second;
        if (!ip.fromString(stat.ip))
        {
            return false;
        }
        return true;
    }

    auto topBlocked = getTopBlockedMap();
    it = topBlocked.find(domain);
    if (it != topBlocked.end())
    {
        ip = IPAddress(0, 0, 0, 0);
        return true;
    }
    
    return false;
}

IPAddress sendUpstream(const char *dom, IPAddress &ip, uint32_t processMs, String &logMsg)
{
    uint32_t oMillis = millis();
    WiFi.hostByName(dom, ip);
    uint32_t resolvMs = millis() - oMillis;
    if (ip == IPAddress(0, 0, 0, 0))
    {
        logMsg += "Block by upstream took %lu ms";
        enqueueDnsLog(true, dom, true, resolvMs, processMs, logMsg);
    }
    else
    {
        logMsg += "Resolve took %lu ms";
        enqueueDnsLog(false, dom, true, resolvMs, processMs, logMsg, ip);
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

    String logMsg = "Domain: %s | IP: %s | ";
    IPAddress ip;
    uint32_t oMillis = millis();
    if (isRewrite(dom.c_str(), ip))
    {
        uint32_t rewriteMs = millis() - oMillis;
        logMsg += "Rewrite took %lu ms";
        enqueueDnsLog(false, dom.c_str(), false, rewriteMs, 0, logMsg, ip);
        return ip;
    }

    if (isCached(dom.c_str(), ip))
    {
        uint32_t cacheLookupMs = millis() - oMillis;
        bool isCachedBlocked = ip == IPAddress(0, 0, 0, 0);
        if (isCachedBlocked)
        {
            logMsg += "Blocked | ";
        }
        logMsg += "Find in cache took %lu ms";
        enqueueDnsLog(isCachedBlocked, dom.c_str(), false, cacheLookupMs, 0, logMsg, ip);
        return ip;
    }

    if (isWhiteListOverride(dom.c_str()))
    {
        return sendUpstream(dom.c_str(), ip, millis() - oMillis, logMsg);
    }

    bool isBlock = isBlockedOverride(dom.c_str()) || isEasyBlock(dom.c_str()) || bloomCheck(dom.c_str());
    uint32_t processMs = millis() - oMillis;
    if (isBlock)
    {
        logMsg += "Blocked | Find took %lu ms";
        enqueueDnsLog(true, dom.c_str(), false, processMs, 0, logMsg);
        return IPAddress(0, 0, 0, 0);
    }

    return sendUpstream(dom.c_str(), ip, millis() - oMillis, logMsg);
}