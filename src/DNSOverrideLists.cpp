#include <DNSOverrideLists.h>

#include <SPIFFS.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <EspLogs.h>
#include <WebServerHelper.h>

std::unordered_map<std::string, IPAddress> rewriteRules;
std::unordered_set<std::string> whiteList;
std::unordered_set<std::string> blockList;

// Getter functions
const std::unordered_map<std::string, IPAddress> &getRewriteRules()
{
    return rewriteRules;
}

const std::unordered_set<std::string> &getWhiteList()
{
    return whiteList;
}

const std::unordered_set<std::string> &getBlockList()
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

    rewriteRules[domain] = ip;
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

bool addListEntry(const String &entry, const char *fName, std::unordered_set<std::string> &list)
{
    File f = SPIFFS.open(fName, FILE_APPEND);
    if (!f)
    {
        dualPrintLogf(ESPHOLE_LOGLEVEL::ERROR, ESPHOLE_LOGTYPES::SPIFFS, "Failed to open %s file\n", fName);
        return false;
    }
    
    if (f.size() > 0)
    {
        f.print("\n");
    }


    f.println(entry);
    f.close();

    list.insert(entry.c_str());
    return true;
}

bool removeListEntry(const String &entry, const char *fName, std::unordered_set<std::string> &list)
{
    bool found = false;
    for (auto it = list.begin(); it != list.end(); ++it)
    {
        if (*it == entry.c_str())
        {
            list.erase(it);
            removeFromTopBlock(entry.c_str());
            removeFromTopQuery(entry.c_str());
            found = true;
            break;
        }
    }

    if (!found)
    {
        dualPrintLogf(ESPHOLE_LOGLEVEL::ERROR, ESPHOLE_LOGTYPES::CODE, "Entry '%s' not found in list", entry.c_str());
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
        f.println(e.c_str());
    }

    f.close();
    return true;
}

void setupRewrite()
{
    rewriteRules.clear();

    File f = SPIFFS.open("/rewrite", FILE_READ);
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

        rewriteRules[dom.c_str()] = ip;
    }

    f.close();
}

void setupWhiteList()
{
    std::unordered_set<std::string> tempList;
    tempList.reserve(whiteList.size());
    tempList.insert(whiteList.begin(), whiteList.end());
    whiteList.clear();
    File f = SPIFFS.open("/whitelist", FILE_READ);
    if (!f)
        return;

    while (f.available())
    {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 0)
        {
            whiteList.insert(line.c_str());
            removeFromTopBlock(line.c_str());
        }
    }
    f.close();

    for (const auto &elem : tempList)
    {
        if (!whiteList.count(elem))
        {
            removeFromTopQuery(elem.c_str());
        }
    }
}

void setupBlockList()
{
    std::unordered_set<std::string> tempList(blockList);
    blockList.clear();
    File f = SPIFFS.open("/blocklist", FILE_READ);
    if (!f)
        return;

    while (f.available())
    {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 0)
        {
            blockList.insert(line.c_str());
            removeFromTopQuery(line.c_str());
        }
    }
    f.close();

    for (const auto &elem : tempList)
    {
        if (!blockList.count(elem))
        {
            removeFromTopBlock(elem.c_str());
        }
    }
}

void setupDNSLists()
{
    setupRewrite();
    setupWhiteList();
    setupBlockList();
}