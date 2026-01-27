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
        dualPrintLogf(ESPHOLE_LOGLEVEL::ERROR, ESPHOLE_LOGTYPES::SPIFFS, "Failed to open rewrite file");
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
    removeListEntry(entry, "/blocklist", blockList);
    return addListEntry(entry, "/whitelist", whiteList);
}

bool addBlockListEntry(const String &entry)
{
    removeListEntry(entry, "/whitelist", whiteList);
    return addListEntry(entry, "/blocklist", blockList);
}

bool addListEntry(const String &entry, const char* fName, std::vector<String> &list)
{
    File f = SPIFFS.open(fName, FILE_APPEND);
    if (!f)
    {
        dualPrintLogf(ESPHOLE_LOGLEVEL::ERROR, ESPHOLE_LOGTYPES::SPIFFS, "Failed to open %s file\n", fName);
        return false;
    }

    f.println(entry);
    f.close();

    list.push_back(entry);
    return true;
}

bool removeListEntry(const String &entry, const char* fName, std::vector<String> &list)
{
    bool found = false;
    for (auto it = list.begin(); it != list.end(); ++it)
    {
        if (*it == entry)
        {
            list.erase(it);
            found = true;
            break;
        }
    }

    if (!found)
    {
        dualPrintLogf(ESPHOLE_LOGLEVEL::ERROR, ESPHOLE_LOGTYPES::CODE, "Entry '%s' not found in list\n", entry.c_str());
        return false;
    }

    File f = SPIFFS.open(fName, FILE_WRITE);
    if (!f)
    {
        dualPrintLogf(ESPHOLE_LOGLEVEL::ERROR, ESPHOLE_LOGTYPES::SPIFFS, "Failed to rewrite %s\n", fName);
        return false;
    }

    for (const auto &e : list)
    {
        f.println(e);
    }

    f.close();
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

    File f = SPIFFS.open("/blocklist", "r");
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