#ifndef SECONDCORELOOP_H
#define SECONDCORELOOP_H

#include <Arduino.h>

void setupSecondaryLoop();
void enqueueDnsLog(bool blocked,
                   const char *domain,
                   bool wasSentUpstream,
                   uint32_t resolveMs,
                   uint32_t addedMs,
                   const char *logMsg,
                   IPAddress ip = IPAddress(0, 0, 0, 0));

#endif // SECONDCORELOOP_H