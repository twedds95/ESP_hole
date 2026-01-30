#include <SecondCoreLoop.h>

#include <DNSTopDomainLists.h>
#include <ESPLogs.h>
#include <WebServerHelper.h>

QueueHandle_t dnsLogQueue;

struct DnsLogEvent
{
  uint32_t resolveMs;
  uint32_t processMs;
  bool blocked;
  bool wasSentUpstream;
  char domain[MAX_DOMAIN_LEN];
  IPAddress ip;
  char logMsg[MAX_LOG_MSG_LEN];
};


void setupSecondaryLoop()
{
    dnsLogQueue = xQueueCreate(64, sizeof(DnsLogEvent));

    xTaskCreatePinnedToCore(
        secondLoopTask,
        "stats",
        4096,
        nullptr,
        1,
        nullptr,
        0 // core 0 (DNS usually runs on core 1)
    );
}

void secondLoopTask(void *arg)
{
    DnsLogEvent ev;

    for (;;)
    {
        handleTimeSensitiveRotations();
        if (xQueueReceive(dnsLogQueue, &ev, portMAX_DELAY))
        {
            recordQuery(ev.blocked, ev.domain, ev.wasSentUpstream, ev.resolveMs, ev.processMs, ev.ip);

            if (ev.wasSentUpstream)
            {
                char buffer[MAX_LOG_MSG_LEN];
                strncpy(buffer, ev.logMsg, sizeof(buffer) - 1);
                strncat(buffer, " | Bloom check took %lu ms", sizeof(buffer) - strlen(buffer) - 1);
                dualPrintLogf(ESPHOLE_LOGLEVEL::INFO,
                              ESPHOLE_LOGTYPES::DNS,
                              buffer,
                              ev.domain,
                              ev.ip.toString().c_str(),
                              ev.resolveMs,
                              ev.processMs);
            }
            else
            {
                dualPrintLogf(ESPHOLE_LOGLEVEL::INFO,
                              ESPHOLE_LOGTYPES::DNS,
                              ev.logMsg,
                              ev.domain,
                              ev.ip.toString().c_str(),
                              ev.resolveMs);
            }
        }
    }
}

void enqueueDnsLog(bool blocked,
                   const char *domain,
                   bool wasSentUpstream,
                   uint32_t resolvMs,
                   uint32_t processMs,
                   String logMsg,
                   IPAddress ip)
{
    if (!dnsLogQueue)
        return;

    DnsLogEvent ev{};
    ev.blocked = blocked;
    ev.resolveMs = resolvMs;
    ev.processMs = processMs;
    ev.wasSentUpstream = wasSentUpstream;
    ev.ip = ip;
    strncpy(ev.domain, domain, MAX_DOMAIN_LEN - 1);
    strncpy(ev.logMsg, logMsg.c_str(), MAX_LOG_MSG_LEN - 1);

    // Do NOT block DNS if queue is full
    xQueueSend(dnsLogQueue, &ev, 0);
}