#ifndef DNSHELPER_H
#define DNSHELPER_H

#include <Arduino.h>

void setupDNSHelper();

bool isEasyBlock(const char *domain);
bool isBlockedOverride(const char *domain);
bool isWhiteListOverride(const char *domain);
bool isRewrite(const char *domain, IPAddress &ip);
IPAddress sendUpstream(const char *dom, IPAddress &ip, uint32_t processMs, String logMsg);
IPAddress handleDNSRequest(String domain);

void setupLogQueue();
void statsTask(void *arg);
void enqueueDnsLog(bool blocked, const char *domain, bool wasSentUpstream, uint32_t resolveMs, uint32_t addedMs, String logMsg, IPAddress ip = IPAddress(0,0,0,0));

#endif //DNSHELPER_H