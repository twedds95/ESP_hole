#include <Arduino.h>

bool easy_block(const char *domain);
bool find_in_block(const char *domain);
bool find_in_rewrite(const char *domain, IPAddress &ip);
IPAddress handleDNSRequest(String domain);