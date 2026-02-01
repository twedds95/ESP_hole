#ifndef DNSOVERRIDELISTS_H
#define DNSOVERRIDELISTS_H

#include <Arduino.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

const std::unordered_map<std::string, IPAddress> *getRewriteRules();
const std::unordered_set<std::string> *getWhiteList();
const std::unordered_set<std::string> *getBlockList();

bool addRewriteRule(const char *domain, const char *ipStr);
bool addWhiteListEntry(const String &entry);
bool addBlockListEntry(const String &entry);

void setupDNSLists();

#endif // DNSOVERRIDELISTS_H