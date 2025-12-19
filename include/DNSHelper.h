#include <Arduino.h>

void setupDNSHelper();

bool isEasyBlock(const char *domain);
bool isBlockedOverride(const char *domain);
bool isWhiteListOverride(const char *domain);
bool isRewrite(const char *domain, IPAddress &ip);
IPAddress sendUpstream(const char *dom, IPAddress &ip, uint32_t processMs);
IPAddress handleDNSRequest(String domain);
void setupRewrite();
void setupWhiteList();
void setupBlockList();
void setupLogQueue();
void statsTask(void *arg);
void enqueueDnsLog(bool blocked, const char *domain, uint32_t resolveMs, uint32_t addedMs);