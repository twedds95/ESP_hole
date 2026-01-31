#ifndef WEBSERVERHELPER_H
#define WEBSERVERHELPER_H

#include <Arduino.h>
#include <array>
#include <ESPAsyncWebServer.h>

#include <DNSTopDomainLists.h>
#include <EspLogs.h>

void setupServerHelper();
void handleTimeSensitiveRotations();
void recordQuery(bool blocked, const char *domain, bool wasSentUpstream, uint32_t resolveTime, uint32_t procTime, IPAddress ip);

#endif // WEBSERVERHELPER_H