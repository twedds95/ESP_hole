#include <Arduino.h>

bool easy_block(const char *domain);
bool find_in_rewrite(const char *domain, IPAddress &ip);
IPAddress handleDNSRequest(String domain);
void setupLogQueue();
void statsTask(void *arg);
void enqueueDnsLog(bool blocked, const char *domain, uint32_t ms);