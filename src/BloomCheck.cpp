#include <BloomCheck.h>

#include <SPIFFS.h>

#include <EspLogs.h>

namespace
{
#define BLOOM_BITS 12500000
#define BLOOM_HASHES 6

#define BITS_PER_FILE 125000
#define BYTES_PER_FILE ((BITS_PER_FILE + 7) / 8)

#define TOTAL_BLOOM_PAGES ((BLOOM_BITS + BITS_PER_FILE - 1) / BITS_PER_FILE)
#define BLOOM_MAX_OPEN 4 // max open bloom files

#define HASH_VALUE 0x9747b28c

  struct BloomPage
  {
    bool open;
    uint16_t pageIndex;
    uint32_t lastUse;
    File file;
  };

  BloomPage pages[BLOOM_MAX_OPEN];

  // for debugging
  uint32_t bloomMisses = 0;
  uint32_t bloomQueries = 0;
  unsigned long lastMsgTick = 0;

  int findOpenPage(uint16_t pageIndex)
  {
    for (int i = 0; i < BLOOM_MAX_OPEN; i++)
    {
      if (pages[i].open && pages[i].pageIndex == pageIndex)
        return i;
    }
    return -1;
  }

  int findLRUVictim()
  {
    int victim = 0;
    uint32_t oldest = pages[0].lastUse;

    for (int i = 1; i < BLOOM_MAX_OPEN; i++)
    {
      if (!pages[i].open)
        return i; // free slot
      if (pages[i].lastUse < oldest)
      {
        oldest = pages[i].lastUse;
        victim = i;
      }
    }
    return victim;
  }

  BloomPage *getBloomPage(uint16_t pageIndex, bool openOnly)
  {
    uint32_t now = millis();

    int idx = findOpenPage(pageIndex);
    if (idx >= 0)
    {
      pages[idx].lastUse = now;
      return &pages[idx];
    }

    if (openOnly)
      return nullptr;

    int victim = findLRUVictim();

    if (pages[victim].open)
    {
      pages[victim].file.close();
      pages[victim].open = false;
      pages[victim].pageIndex = 0xFFFF;
      pages[victim].lastUse = 0;
      pages[victim].file = File();
    }

    char path[32];
    snprintf(path, sizeof(path), "/bloom%u.bin", pageIndex);

    File f = SPIFFS.open(path, FILE_READ);
    if (!f)
    {
      dualPrintLogf(ESPHOLE_LOGLEVEL::ERROR, ESPHOLE_LOGTYPES::BLOOM,
                    "Bloom filter %s could not be opened, bloom bin may be missing or corrupted.",
                    path);
      return nullptr;
    }

    pages[victim].open = true;
    pages[victim].pageIndex = pageIndex;
    pages[victim].lastUse = now;
    pages[victim].file = f;

    return &pages[victim];
  }

  uint32_t bloomHash(const char *s, uint32_t seed)
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
}

bool bloomCheck(const char *domain)
{
  bloomQueries++;
  for (int pass = 0; pass < 2; pass++)
  {
    bool openOnly = (pass == 0);
    bool miss = false;
    for (uint32_t i = 0; i < BLOOM_HASHES; i++)
    {
      uint32_t h = bloomHash(domain, HASH_VALUE + i);
      uint32_t bit = h % BLOOM_BITS;
      uint32_t byteIndex = bit >> 3;
      uint32_t pageIndex = byteIndex / BYTES_PER_FILE;
      uint32_t t_ = micros();
      BloomPage *p = getBloomPage(pageIndex, openOnly);
      uint32_t t_page = micros();
      serialPrintLogf("[%s] pageIndex = %lu getBloomPage=%lu us\n",
                      LOG_TAG(ESPHOLE_LOGTYPES::BLOOM),
                      pageIndex, t_page - t_);
      if (!p)
      {
        miss = true;
        continue;
      }

      uint32_t pageOff = byteIndex % BYTES_PER_FILE;
      uint32_t t0 = micros();
      p->file.seek(pageOff);
      uint32_t t1 = micros();
      uint8_t b = p->file.read();
      uint32_t t2 = micros();
      serialPrintLogf("[%s] offset=%lu seek=%lu us read=%lu us\n",
                      LOG_TAG(ESPHOLE_LOGTYPES::BLOOM),
                      pageOff, t1 - t0, t2 - t1);
      uint8_t mask = 1 << (bit & 7);
      if ((b & mask) == 0)
        return false; // definitely not blocked
    }

    if (!miss)
      return true; // all bits found in this pass
  }

  bloomMisses++; // need to close and load more pages
  return true;
}

void setupBloom()
{
  for (int i = 0; i < BLOOM_MAX_OPEN; i++)
  {
    pages[i].open = false;
  }

  for (int i = 0; i < int(TOTAL_BLOOM_PAGES); i++)
  {
    String bloomFileName = String("/bloom") + String(i) + String(".bin");
    if (!SPIFFS.exists(bloomFileName))
    {
      dualPrintLogf(ESPHOLE_LOGLEVEL::ERROR, ESPHOLE_LOGTYPES::BLOOM,
                    "Bloom filter %s not found!",
                    bloomFileName);
    }
    else
    {
      dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG, ESPHOLE_LOGTYPES::BLOOM,
                    "Bloom filter %s loaded",
                    bloomFileName);
    }
  }

  for (int i = 0; i < BLOOM_HASHES; i++)
  {
    uint32_t h = bloomHash("google.com", HASH_VALUE + i) % BLOOM_BITS;
    dualPrintLogf(ESPHOLE_LOGLEVEL::DEBUG, ESPHOLE_LOGTYPES::BLOOM,
                  "%d bit=%u page=%u off=%u",
                  i, h, h / BITS_PER_FILE, h % BITS_PER_FILE);
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