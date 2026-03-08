#include <DNSTopDomainLists.h>

#include <algorithm>
#include <vector>
#include <SPIFFS.h>

#include <EspLogs.h>

namespace
{

#define BLOCK_PATH "/TOP_DOMAINS_BLOCK"
#define QUERY_PATH "/TOP_DOMAINS_QUERY"

    constexpr uint32_t TOP_STATS_MAGIC = 0x54535453; // 'TSTS'
    constexpr uint16_t TOP_STATS_VERSION = 2;

    struct TopStatsHeader
    {
        uint32_t magic;
        uint16_t version;
        uint16_t numTracked;
    };

    std::unordered_map<std::string, DomainStat> topBlockedMap;
    std::unordered_map<std::string, DomainStat> topQueriedMap;

    std::array<const DomainStat *, TOP_N> getTopN(const std::unordered_map<std::string, DomainStat> &map)
    {
        std::array<const DomainStat *, TOP_N> top{};
        top.fill(nullptr);

        for (const auto &pair : map)
        {
            const auto &stat = pair.second;
            if (stat.count == 0)
                continue;

            for (size_t i = 0; i < TOP_N; ++i)
            {
                if (!top[i] || stat.count > top[i]->count)
                {
                    for (size_t j = TOP_N - 1; j > i; --j)
                        top[j] = top[j - 1];

                    top[i] = &stat;
                    break;
                }
            }
        }
        return top;
    }

    const DomainStat *getSmallestByCount(const std::unordered_map<std::string, DomainStat> &map)
    {
        const DomainStat *smallest = nullptr;

        for (const auto &pair : map)
        {
            const auto &stat = pair.second;
            if (stat.count == 0)
                return &stat;

            if (!smallest || stat.count < smallest->count)
                smallest = &stat;
        }

        return smallest; // may be nullptr if map empty
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

    void decayTopDomain(std::unordered_map<std::string, DomainStat> &domMap, double_t percent, double_t totalQueries)
    {
        if (domMap.empty())
            return;

        std::vector<std::string> domainsToErase;
        for (const auto &pair : domMap)
        {
            const auto &tempStat = pair.second;
            if (tempStat.count <= 0)
            {
                dualPrintLogf(ESPHOLE_LOGLEVEL::ERROR,
                              ESPHOLE_LOGTYPES::STATS,
                              "A domain with a count of 0 is still cached -- this should not happen.");
            }

            double_t domPercentEstimate = (double_t)tempStat.count / totalQueries;
            uint32_t decayAmount = (uint32_t)(domPercentEstimate * percent * tempStat.count);
            if (decayAmount > tempStat.count)
            {
                domainsToErase.push_back(tempStat.domain);
            }
            else
            {
                domMap[tempStat.domain].count = tempStat.count - decayAmount;
            }
        }

        for (const auto &domain : domainsToErase)
        {
            domMap.erase(domain);
        }
    }

    void updateTop(std::unordered_map<std::string, DomainStat> &domMap, const char *dom, bool wasSentUpstream, IPAddress ip = IPAddress(0, 0, 0, 0))
    {
        char domain[MAX_DOMAIN_LEN];
        sanitizeDomain(dom, domain);

        // Check if exists
        auto it = domMap.find(domain);
        if (it != domMap.end())
        {
            it->second.count++;
            return;
        }

        DomainStat stat{};
        strncpy(stat.domain, domain, MAX_DOMAIN_LEN);
        stat.count = 1;
        stat.wasSentUpstream = wasSentUpstream;
        strncpy(stat.ip, ip.toString().c_str(), MAX_IP_LEN);

        // Check room for tracking
        if (domMap.size() >= TOP_N_TRACKED)
        {
            auto smallest = getSmallestByCount(domMap);
            domMap.erase(smallest->domain);
        }

        domMap[stat.domain] = stat;
    }

    bool removeFromTopList(std::unordered_map<std::string, DomainStat> &domMap, const char *dom)
    {
        if (domMap.empty())
            return false;

        char domain[MAX_DOMAIN_LEN];
        sanitizeDomain(dom, domain);
        // Check if exists
        auto it = domMap.find(domain);
        if (it != domMap.end())
        {
            domMap.erase(domain);
            return true;
        }

        return false;
    }

    void saveTopStats(const char *path, const std::unordered_map<std::string, DomainStat> map)
    {
        File f = SPIFFS.open(String(path) + ".tmp", FILE_WRITE, true);
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
            (uint16_t)map.size()};

        f.write((uint8_t *)&hdr, sizeof(hdr));

        for (const auto &pair : map)
        {
            const auto &stat = pair.second;
            f.write((uint8_t *)&stat, sizeof(DomainStat));
        }

        f.close();
        if (SPIFFS.exists(path))
        {
            SPIFFS.remove(path);
        }

        SPIFFS.rename(String(path) + ".tmp", path);
        dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                      ESPHOLE_LOGTYPES::STATS,
                      "Cached Top %d Domains to %s SPIFFS File",
                      map.size(), path);
    }

    bool loadCachedTopStats(const char *path, std::unordered_map<std::string, DomainStat> &domMap)
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

        domMap.clear();

        DomainStat tmp;
        size_t count = min((size_t)hdr.numTracked, (size_t)TOP_N_TRACKED);

        for (size_t i = 0; i < count; i++)
        {
            if (f.read((uint8_t *)&tmp, sizeof(tmp)) != sizeof(tmp))
                break;
            domMap[tmp.domain] = tmp;
        }

        dualPrintLogf(ESPHOLE_LOGLEVEL::INFO,
                      ESPHOLE_LOGTYPES::STATS,
                      "Loaded Top %d Cached Domains from %s",
                      domMap.size(), path);
        f.close();
        return true;
    }

}

const std::array<const DomainStat *, TOP_N> getTopBlockedArr()
{
    return getTopN(topBlockedMap);
}

const std::array<const DomainStat *, TOP_N> getTopQueriedArr()
{
    return getTopN(topQueriedMap);
}

const std::unordered_map<std::string, DomainStat> *getTopBlockedMap()
{
    return &topBlockedMap;
}

const std::unordered_map<std::string, DomainStat> *getTopQueriedMap()
{
    return &topQueriedMap;
}

void saveTopStats()
{
    saveTopStats(BLOCK_PATH, topBlockedMap);
    saveTopStats(QUERY_PATH, topQueriedMap);
}

void loadCachedTopStats()
{
    if (!loadCachedTopStats(BLOCK_PATH, topBlockedMap))
    {
        dualPrintLogf(ESPHOLE_LOGLEVEL::INFO,
                      ESPHOLE_LOGTYPES::STATS,
                      "Cached Top Domains Not Loaded - %s",
                      BLOCK_PATH);
    }
    if (!loadCachedTopStats(QUERY_PATH, topQueriedMap))
    {
        dualPrintLogf(ESPHOLE_LOGLEVEL::INFO,
                      ESPHOLE_LOGTYPES::STATS,
                      "Cached Top Domains Not Loaded - %s",
                      QUERY_PATH);
    }
}

void decayTopDomains(double_t percent, double_t total)
{
    decayTopDomain(topBlockedMap, percent, total);
    decayTopDomain(topQueriedMap, percent, total);
}

void updateTopBlocked(const char *domain, bool wasSentUpstream)
{
    updateTop(topBlockedMap, domain, wasSentUpstream);
}

void updateTopQueried(const char *domain, bool wasSentUpstream, IPAddress ip)
{
    updateTop(topQueriedMap, domain, wasSentUpstream, ip);
}

void removeFromTopQuery(const char *dom)
{
    char domain[MAX_DOMAIN_LEN];
    strncpy(domain, dom, MAX_DOMAIN_LEN);
    removeFromTopList(topQueriedMap, dom)
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
    removeFromTopList(topBlockedMap, dom)
        ? dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                        ESPHOLE_LOGTYPES::STATS,
                        "Domain '%s' was removed from top blocked cached list.",
                        domain)
        : dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                        ESPHOLE_LOGTYPES::STATS,
                        "Domain '%s' was not found in top blocked cached list.",
                        domain);
}
