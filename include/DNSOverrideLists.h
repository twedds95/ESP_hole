#ifndef DNSOVERRIDELISTS_H
#define DNSOVERRIDELISTS_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <vector>

struct RewriteRule
{
    String domain;
    IPAddress ip;
};

const std::vector<RewriteRule>& getRewriteRules();
const std::vector<String>& getWhiteList();
const std::vector<String>& getBlockList();

bool addRewriteRule(const char* domain, const char* ipStr);
bool addWhiteListEntry(const String& entry);
bool addBlockListEntry(const String& entry);
bool addListEntry(const String &entry, const char* fName, std::vector<String> &list);
bool removeListEntry(const String &entry, const char* fName, std::vector<String> &list);


void setupDNSLists();

void setupRewrite();
void setupWhiteList();
void setupBlockList();

#endif // DNSOVERRIDELISTS_H