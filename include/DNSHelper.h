#ifndef DNSHELPER_H
#define DNSHELPER_H

#include <Arduino.h>

bool isEasyBlock(const char *domain);
bool isBlockedOverride(const char *domain);
bool isWhiteListOverride(const char *domain);
bool isRewrite(const char *domain, IPAddress &ip);
IPAddress sendUpstream(const char *dom, IPAddress &ip, uint32_t processMs, String &logMsg);
IPAddress handleDNSRequest(String domain);

#endif //DNSHELPER_H