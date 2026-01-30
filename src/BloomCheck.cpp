#include <BloomCheck.h>

#include <SPIFFS.h>

#include <EspLogs.h>

namespace
{

#define BLOOM_PAGE_SIZE 4096
#define BLOOM_CACHE_PAGES 2 // 8 KB total RAM

  struct BloomPage
  {
    uint32_t pageIndex;
    bool valid;
    uint32_t lastUse;
    uint8_t data[BLOOM_PAGE_SIZE];
  };

  BloomPage bloomCache[BLOOM_CACHE_PAGES];
  File bloomFile;

  // for debugging
  uint32_t bloomMisses = 0;
  uint32_t bloomQueries = 0;
  unsigned long lastMsgTick = 0;

  uint8_t *getBloomPage(uint32_t pageIndex)
  {
    uint32_t now = millis();
    for (int i = 0; i < BLOOM_CACHE_PAGES; i++)
    {
      if (bloomCache[i].valid && bloomCache[i].pageIndex == pageIndex)
      {
        bloomCache[i].lastUse = now;
        return bloomCache[i].data;
      }
    }

    bloomMisses++;
    int victim = 0;
    for (int i = 1; i < BLOOM_CACHE_PAGES; i++)
    {
      if (!bloomCache[i].valid ||
          bloomCache[i].lastUse < bloomCache[victim].lastUse)
      {
        victim = i;
      }
    }

    uint32_t offset = pageIndex * BLOOM_PAGE_SIZE;
    bloomFile.seek(offset);

    size_t read = bloomFile.read(
        bloomCache[victim].data,
        BLOOM_PAGE_SIZE);

    if (read != BLOOM_PAGE_SIZE)
    {
      memset(bloomCache[victim].data + read, 0,
             BLOOM_PAGE_SIZE - read);
    }

    bloomCache[victim].pageIndex = pageIndex;
    bloomCache[victim].valid = true;
    bloomCache[victim].lastUse = now;

    return bloomCache[victim].data;
  }
}

static uint32_t bloomHash(const char *s, uint32_t seed)
{
  uint32_t h = seed;
  while (*s)
  {
    h ^= (uint8_t)*s++;
    h *= 0x5bd1e995;
    h ^= h >> 15;
  }

  return h;
}

bool bloomCheck(const char *domain)
{
  if (!bloomFile)
    return false;

  bloomQueries++;
  for (uint32_t i = 0; i < BLOOM_HASHES; i++)
  {
    uint32_t h = bloomHash(domain, 0x9747b28c + i);
    uint32_t bit = h % BLOOM_BITS;
    uint32_t byteIndex = bit >> 3;
    uint32_t pageIndex = byteIndex / BLOOM_PAGE_SIZE;
    uint32_t pageOff = byteIndex % BLOOM_PAGE_SIZE;

    uint8_t *page = getBloomPage(pageIndex);
    uint8_t mask = 1 << (bit & 7);

    if ((page[pageOff] & mask) == 0)
    {
      return false; // definitely not blocked
    }
  }

  return true; // maybe blocked
}

void setupBloom()
{
  bloomFile = SPIFFS.open("/bloom.bin", FILE_READ);
  if (!bloomFile)
  {
    dualPrintLogf(ESPHOLE_LOGLEVEL::ERROR, ESPHOLE_LOGTYPES::BLOOM, "Bloom filter not found!");
  }
  else
  {
    for (int i = 0; i < BLOOM_CACHE_PAGES; i++)
    {
      bloomCache[i].valid = false;
    }
    dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG, ESPHOLE_LOGTYPES::BLOOM, "Bloom filter loaded");
  }
}

void handleTimedBloomMsg()
{
  unsigned long now = millis();
  if (now - lastMsgTick < 900000UL) // log every 15mins
  {
    return;
  }

  lastMsgTick = now;
  dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG,
                ESPHOLE_LOGTYPES::BLOOM,
                "Bloom had %d misses on %d queries in the past 15mins where it needed to read spiffs.",
                bloomMisses, bloomQueries);
  bloomMisses = 0;
  bloomQueries = 0;
}