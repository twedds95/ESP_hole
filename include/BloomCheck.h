#ifndef BLOOMCHECK_H
#define BLOOMCHECK_H

#include <Arduino.h>
#include <SPIFFS.h>

#include <EspLogs.h>

#define BLOOM_BITS 12500000
#define BLOOM_HASHES 6
#define BLOOM_BYTES  ((BLOOM_BITS + 7) / 8)

static uint32_t bloomHash(const char *s, uint32_t seed);
bool bloomCheck(const char *domain);
void setupBloom();

#endif //BLOOMCHECK_H