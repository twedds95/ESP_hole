#include <DNSOverrideLists.h>

std::vector<RewriteRule> rewriteRules;
std::vector<String> whiteList;
std::vector<String> blockList;

// Getter functions
const std::vector<RewriteRule> &getRewriteRules()
{
    return rewriteRules;
}

const std::vector<String> &getWhiteList()
{
    return whiteList;
}

const std::vector<String> &getBlockList()
{
    return blockList;
}

// Add functions
bool addRewriteRule(const char *domain, const char *ipStr)
{

    File f = SPIFFS.open("/rewrite", FILE_APPEND);
    if (!f)
    {
        Serial.println("ERROR: Failed to open rewrite file");
        return false;
    }

    char buf[sizeof(domain) + sizeof(ipStr) + 2];
    snprintf(buf, sizeof(buf), "%s,%s", domain, ipStr);
    f.println(buf);
    f.close();

    IPAddress ip;
    if (!ip.fromString(ipStr))
    {
        return false;
    }

    rewriteRules.push_back({domain, ip});
    return true;
}

bool addWhiteListEntry(const String &entry)
{
    File f = SPIFFS.open("/whitelist", FILE_APPEND);
    if (!f)
    {
        Serial.println("ERROR: Failed to open whitelist file");
        return false;
    }

    f.println(entry);
    f.close();

    whiteList.push_back(entry);
    return true;
}

bool addBlockListEntry(const String &entry)
{
    File f = SPIFFS.open("/blockList", FILE_APPEND);
    if (!f)
    {
        Serial.println("ERROR: Failed to open block file");
        return false;
    }

    f.println(entry);
    f.close();

    blockList.push_back(entry);
    return true;
}

void setupRewrite()
{
    rewriteRules.clear();

    File f = SPIFFS.open("/rewrite", "r");
    if (!f)
        return;

    while (f.available())
    {
        String line = f.readStringUntil('\n');
        line.trim();
        if (!line.length())
            continue;

        int comma = line.indexOf(',');
        if (comma < 0)
            continue;

        String dom = line.substring(0, comma);
        String ipStr = line.substring(comma + 1);

        dom.toLowerCase();
        ipStr.trim();

        IPAddress ip;
        if (!ip.fromString(ipStr))
            continue;

        rewriteRules.push_back({dom, ip});
    }

    f.close();
}

void setupWhiteList()
{
    whiteList.clear();

    File f = SPIFFS.open("/whitelist", "r");
    if (!f)
        return;

    while (f.available())
    {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 0)
            whiteList.push_back(line);
    }
    f.close();
}

void setupBlockList()
{
    blockList.clear();

    File f = SPIFFS.open("/blockList", "r");
    if (!f)
        return;

    while (f.available())
    {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 0)
            blockList.push_back(line);
    }
    f.close();
}

void setupDNSLists()
{
    setupRewrite();
    setupWhiteList();
    setupBlockList();
}