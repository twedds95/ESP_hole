#include <DNSTopDomainLists.h>

#include <SPIFFS.h>

#include <EspLogs.h>

namespace
{

#define BLOCK_PATH "/TOP_DOMAINS_BLOCK"
#define QUERY_PATH "/TOP_DOMAINS_QUERY"

    enum class CACHED_TYPE
    {
        BLOCKED,
        QUERIED,
    };

    std::set<DomainStat, DomainStatCompare> topBlockedSet;
    std::set<DomainStat, DomainStatCompare> topQueriedSet;

    std::unordered_map<std::string, DomainStat> topBlockedMap;
    std::unordered_map<std::string, DomainStat> topQueriedMap;

    void getSetAndMapByType(CACHED_TYPE type,
                            std::set<DomainStat, DomainStatCompare> *&domSet,
                            std::unordered_map<std::string, DomainStat> *&domMap)
    {
        switch (type)
        {
        case CACHED_TYPE::BLOCKED:
            domSet = &topBlockedSet;
            domMap = &topBlockedMap;
            break;

        case CACHED_TYPE::QUERIED:
            domSet = &topQueriedSet;
            domMap = &topQueriedMap;
            break;

        default:
            domSet = nullptr;
            domMap = nullptr;
        }
        if (!domSet || !domMap)
        {
            dualPrintLogf(ESPHOLE_LOGLEVEL::ERROR,
                          ESPHOLE_LOGTYPES::STATS,
                          "Cached type could not be determined -- Cached Lists may be corrupted.");
        }
    }

    void decayTopDomain(CACHED_TYPE type, double_t percent, double_t totalQueries)
    {
        std::set<DomainStat, DomainStatCompare> *domSet = nullptr;
        std::unordered_map<std::string, DomainStat> *domMap = nullptr;
        getSetAndMapByType(type, domSet, domMap);
        if (!domSet || !domMap)
            return;

        // set elements are const, so just recreate the set since we modify everything
        std::set<DomainStat, DomainStatCompare> tempSet;
        tempSet.swap(*domSet);
        for (const auto &tempStat : tempSet)
        {
            if (tempStat.count <= 0)
            {
                dualPrintLogf(ESPHOLE_LOGLEVEL::ERROR,
                              ESPHOLE_LOGTYPES::STATS,
                              "A domain with a count of 0 is still cached -- this should not happen.");
            }

            DomainStat stat{};
            strncpy(stat.domain, tempStat.domain, MAX_DOMAIN_LEN);
            stat.count = 1;
            stat.wasSentUpstream = tempStat.wasSentUpstream;
            strncpy(stat.ip, tempStat.ip, MAX_IP_LEN);
            double_t domPercentEstimate = (double_t)tempStat.count / totalQueries;
            uint32_t decayAmount = (uint32_t)(domPercentEstimate * percent * tempStat.count);
            stat.count = tempStat.count - decayAmount;
            if (stat.count > 0)
            {
                domSet->insert(stat);
            }
            else
            {
                domMap->erase(stat.domain);
            }
        }
    }

    void updateTop(CACHED_TYPE type, const char *dom, bool wasSentUpstream, IPAddress ip = IPAddress(0, 0, 0, 0))
    {
        std::set<DomainStat, DomainStatCompare> *domSet = nullptr;
        std::unordered_map<std::string, DomainStat> *domMap = nullptr;
        getSetAndMapByType(type, domSet, domMap);
        if (!domSet || !domMap)
            return;

        char domain[MAX_DOMAIN_LEN];
        sanitizeDomain(dom, domain);

        DomainStat stat{};
        strncpy(stat.domain, domain, MAX_DOMAIN_LEN);
        stat.count = 1;
        stat.wasSentUpstream = wasSentUpstream;
        strncpy(stat.ip, ip.toString().c_str(), MAX_IP_LEN);

        // Check if exists
        auto it = domMap->find(domain);
        if (it != domMap->end())
        {
            domSet->erase(it->second);
            stat.count = it->second.count + 1;
            // cached items are not sent upstream so keep initial flag from first call
            stat.wasSentUpstream = it->second.wasSentUpstream;
            domSet->insert(stat);
            it->second.count++;
            return;
        }

        // Check room for tracking
        if (domSet->size() >= TOP_N_TRACKED)
        {
            auto smallest = domSet->end();
            domMap->erase(smallest->domain);
            domSet->erase(smallest);
        }

        domSet->insert(stat);
        (*domMap)[stat.domain] = stat;
    }

    bool removeFromTopList(CACHED_TYPE type, const char *dom)
    {
        std::set<DomainStat, DomainStatCompare> *domSet = nullptr;
        std::unordered_map<std::string, DomainStat> *domMap = nullptr;
        getSetAndMapByType(type, domSet, domMap);
        if (!domSet || !domMap)
            return false;

        char domain[MAX_DOMAIN_LEN];
        sanitizeDomain(dom, domain);
        // Check if exists
        auto it = domMap->find(domain);
        if (it != domMap->end())
        {
            domSet->erase(it->second);
            domMap->erase(domain);
            return true;
        }

        return false;
    }

    void saveTopStats(const char *path, const std::set<DomainStat, DomainStatCompare> set)
    {
        File f = SPIFFS.open(String(path) + ".tmp", FILE_WRITE);
        if (!f)
        {
            dualPrintLogf(ESPHOLE_LOGLEVEL::ERROR,
                          ESPHOLE_LOGTYPES::STATS,
                          "Cached Top Domains Not Saved - %s SPIFFS File Error",
                          path);
            return;
        }

        TopStatsHeader hdr{
            TOP_STATS_MAGIC,
            TOP_STATS_VERSION,
            (uint16_t)set.size()};

        f.write((uint8_t *)&hdr, sizeof(hdr));

        for (const auto &entry : set)
        {
            f.write((uint8_t *)&entry, sizeof(DomainStat));
        }

        f.close();
        SPIFFS.remove(path);
        SPIFFS.rename(String(path) + ".tmp", path);
        dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                      ESPHOLE_LOGTYPES::STATS,
                      "Cached Top %d Domains to %s SPIFFS File",
                      set.size(), path);
    }

    bool loadCachedTopStats(const char *path, CACHED_TYPE type)
    {
        File f = SPIFFS.open(path, FILE_READ);
        if (!f)
        {
            dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                          ESPHOLE_LOGTYPES::STATS,
                          "Cached Top Domains Not Loaded - %s File Not Found",
                          path);
            return false;
        }

        TopStatsHeader hdr;
        if (f.read((uint8_t *)&hdr, sizeof(hdr)) != sizeof(hdr))
        {
            f.close();
            dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                          ESPHOLE_LOGTYPES::STATS,
                          "Cached Top Domains Not Loaded - %s Header Not Valid",
                          path);
            return false;
        }

        if (hdr.magic != TOP_STATS_MAGIC ||
            hdr.version != TOP_STATS_VERSION)
        {
            f.close();
            dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                          ESPHOLE_LOGTYPES::STATS,
                          "Cached Top Domains Not Loaded - %s Version Mismatch",
                          path);
            return false;
        }

        std::set<DomainStat, DomainStatCompare> *domSet = nullptr;
        std::unordered_map<std::string, DomainStat> *domMap = nullptr;
        getSetAndMapByType(type, domSet, domMap);
        if (!domSet || !domMap)
            return false;

        domSet->clear();
        domMap->clear();

        DomainStat tmp;
        size_t count = min((size_t)hdr.numTracked, (size_t)TOP_N_TRACKED);

        for (size_t i = 0; i < count; i++)
        {
            if (f.read((uint8_t *)&tmp, sizeof(tmp)) != sizeof(tmp))
                break;
            domSet->insert(tmp);
            (*domMap)[tmp.domain] = tmp;
        }

        dualPrintLogf(ESPHOLE_LOGLEVEL::INFO,
                      ESPHOLE_LOGTYPES::STATS,
                      "Loaded Top %d Cached Domains from %s",
                      domSet->size(), path);
        f.close();
        return true;
    }
}

const std::set<DomainStat, DomainStatCompare> &getTopBlockedSet()
{
    return topBlockedSet;
}

const std::set<DomainStat, DomainStatCompare> &getTopQueriedSet()
{
    return topQueriedSet;
}

const std::unordered_map<std::string, DomainStat> &getTopBlockedMap()
{
    return topBlockedMap;
}

const std::unordered_map<std::string, DomainStat> &getTopQueriedMap()
{
    return topQueriedMap;
}

void saveTopStats()
{
    saveTopStats(BLOCK_PATH, topBlockedSet);
    saveTopStats(QUERY_PATH, topQueriedSet);
}

void loadCachedTopStats()
{
    if (!loadCachedTopStats(BLOCK_PATH, CACHED_TYPE::BLOCKED))
    {
        dualPrintLogf(ESPHOLE_LOGLEVEL::INFO,
                      ESPHOLE_LOGTYPES::STATS,
                      "Cached Top Domains Not Loaded - %s SPIFFS File Not Found",
                      BLOCK_PATH);
    }
    if (!loadCachedTopStats(QUERY_PATH, CACHED_TYPE::QUERIED))
    {
        dualPrintLogf(ESPHOLE_LOGLEVEL::INFO,
                      ESPHOLE_LOGTYPES::STATS,
                      "Cached Top Domains Not Loaded - %s SPIFFS File Not Found",
                      QUERY_PATH);
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

void decayTopDomains(double_t percent, double_t total)
{
    decayTopDomain(CACHED_TYPE::BLOCKED, percent, total);
    decayTopDomain(CACHED_TYPE::QUERIED, percent, total);
}

void updateTopBlocked(const char *domain, bool wasSentUpstream)
{
    updateTop(CACHED_TYPE::BLOCKED, domain, wasSentUpstream);
}

void updateTopQueried(const char *domain, bool wasSentUpstream, IPAddress ip)
{
    updateTop(CACHED_TYPE::QUERIED, domain, wasSentUpstream, ip);
}

void removeFromTopQuery(const char *dom)
{
    char domain[MAX_DOMAIN_LEN];
    strncpy(domain, dom, MAX_DOMAIN_LEN);
    removeFromTopList(CACHED_TYPE::QUERIED, dom)
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
    removeFromTopList(CACHED_TYPE::BLOCKED, dom)
        ? dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                        ESPHOLE_LOGTYPES::STATS,
                        "Domain '%s' was removed from top blocked cached list.",
                        domain)
        : dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                        ESPHOLE_LOGTYPES::STATS,
                        "Domain '%s' was not found in top blocked cached list.",
                        domain);
}
