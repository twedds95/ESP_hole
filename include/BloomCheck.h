#include <Arduino.h>
#include <SPIFFS.h>

#define BLOOM_BITS   2500000
#define BLOOM_HASHES 5
#define BLOOM_BYTES  ((BLOOM_BITS + 7) / 8)

static uint32_t bloomHash(const char *s, uint32_t seed);
bool bloomCheck(const char *domain);
void setupBloom();