#ifndef DNSTOPDOMAINLISTS_H
#define DNSTOPDOMAINLISTS_H

#include <Arduino.h>
#include <array>
#include <string>
#include <unordered_map>

// track more domains but only show top 10 in Dashboard
#define TOP_N 10
#define TOP_N_TRACKED 100
#define MAX_DOMAIN_LEN 96
#define MAX_IP_LEN 16

struct DomainStat
{
  char domain[MAX_DOMAIN_LEN];
  uint32_t count;
  bool wasSentUpstream;
  char ip[MAX_IP_LEN];
};

const std::array<const DomainStat*, TOP_N> getTopBlockedArr();
const std::array<const DomainStat*, TOP_N> getTopQueriedArr();

const std::unordered_map<std::string, DomainStat> *getTopBlockedMap();
const std::unordered_map<std::string, DomainStat> *getTopQueriedMap();

void saveTopStats();
void loadCachedTopStats();
void decayTopDomains(double_t percent, double_t total);
void updateTopBlocked(const char *domain, bool wasSentUpstream);
void updateTopQueried(const char *domain, bool wasSentUpstream, IPAddress ip);
void removeFromTopBlock(const char *dom);
void removeFromTopQuery(const char *dom);

#endif // DNSTOPDOMAINLISTS_H