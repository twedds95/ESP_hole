#ifndef DNSOVERRIDELISTS_H
#define DNSOVERRIDELISTS_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <EspLogs.h>
#include <WebServerHelper.h>

const std::unordered_map<std::string, IPAddress> &getRewriteRules();
const std::unordered_set<std::string> &getWhiteList();
const std::unordered_set<std::string> &getBlockList();

bool addRewriteRule(const char *domain, const char *ipStr);
bool addWhiteListEntry(const String &entry);
bool addBlockListEntry(const String &entry);
bool addListEntry(const String &entry, const char *fName, std::unordered_set<std::string> &list);
bool removeListEntry(const String &entry, const char *fName, std::unordered_set<std::string> &list);

void setupDNSLists();

void setupRewrite();
void setupWhiteList();
void setupBlockList();

#endif // DNSOVERRIDELISTS_H