#include <BloomCheck.h>

File bloomFile;

static uint32_t bloomHash(const char *s, uint32_t seed)
{
  uint32_t h = seed;
  while (*s) {
    h ^= (uint8_t)*s++;
    h *= 0x5bd1e995;
    h ^= h >> 15;
  }
  return h;
}

bool bloomCheck(const char *domain)
{
  if (!bloomFile) return true;

  for (uint32_t i = 0; i < BLOOM_HASHES; i++)
  {
    uint32_t h = bloomHash(domain, 0x9747b28c + i);
    uint32_t bit = h % BLOOM_BITS;
    uint32_t byteIndex = bit >> 3;
    uint8_t mask = 1 << (bit & 7);

    bloomFile.seek(byteIndex);
    uint8_t b = bloomFile.read();

    if ((b & mask) == 0)
      return false;  // definitely NOT blocked
  }

  return true; // maybe blocked
}

void setupBloom()
{
    bloomFile = SPIFFS.open("/bloom.bin", "r");
    if (!bloomFile)
    {
        Serial.println("Bloom filter not found!");
    }
    else
    {
        Serial.println("Bloom filter loaded");
    }
}