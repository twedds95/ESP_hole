#ifndef DNSTOPDOMAINLISTS_H
#define DNSTOPDOMAINLISTS_H

#include <Arduino.h>
#include <set>
#include <string>
#include <unordered_map>

#define TOP_N_TRACKED 1000
#define MAX_DOMAIN_LEN 48
#define MAX_IP_LEN 16

constexpr uint32_t TOP_STATS_MAGIC = 0x54535453; // 'TSTS'
constexpr uint16_t TOP_STATS_VERSION = 1;

struct TopStatsHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t numTracked;
};

struct DomainStat
{
  char domain[MAX_DOMAIN_LEN];
  uint32_t count;
  bool wasSentUpstream;
  char ip[MAX_IP_LEN];
};

struct DomainStatCompare {
    bool operator()(const DomainStat& a, const DomainStat& b) const 
    {
        if (a.count != b.count)
            return a.count > b.count;
        return strcmp(a.domain, b.domain) < 0;
    }
};

const std::set<DomainStat, DomainStatCompare> &getTopBlockedSet();
const std::set<DomainStat, DomainStatCompare> &getTopQueriedSet();

const std::unordered_map<std::string, DomainStat> &getTopBlockedMap();
const std::unordered_map<std::string, DomainStat> &getTopQueriedMap();

void saveTopStats();
void loadCachedTopStats();
void sanitizeDomain(const char *dom, char *domain);
void decayTopDomains(double_t percent, double_t total);
void updateTopBlocked(const char *domain, bool wasSentUpstream);
void updateTopQueried(const char *domain, bool wasSentUpstream, IPAddress ip);
void removeFromTopBlock(const char *dom);
void removeFromTopQuery(const char *dom);

#endif // DNSTOPDOMAINLISTS_H