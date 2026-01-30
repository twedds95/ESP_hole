#ifndef DNSTOPDOMAINLISTS_H
#define DNSTOPDOMAINLISTS_H

#include <Arduino.h>
#include <array>
#include <string>
#include <unordered_map>

// track more domains but only show top 10 in Dashboard
#define TOP_N 10
#define TOP_N_TRACKED 1000
#define MAX_DOMAIN_LEN 96
#define MAX_IP_LEN 16

constexpr uint32_t TOP_STATS_MAGIC = 0x54535453; // 'TSTS'
constexpr uint16_t TOP_STATS_VERSION = 2;

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

const std::array<const DomainStat*, TOP_N> getTopBlockedArr();
const std::array<const DomainStat*, TOP_N> getTopQueriedArr();

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