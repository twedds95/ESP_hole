#include <DNSHelper.h>

#include <BloomCheck.h>
#include <SPIFFS.h>
#include <WebServerHelper.h>
#include <WiFi.h>

QueueHandle_t dnsLogQueue;

bool easy_block(const char *domain)
{
    static const char *patterns[] = {
        "ad.",
        ".ad.",
        ".ads.",
        "adserver.",
        "adservers.",
        "adtrack.",
        "adtracker.",
        "adservice.",
        "adservices.",
        "analytics.",
        "telemetry.",
        "tracker.",
        "tracking.",
        "beacon.",
        "logging."
    };

    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++)
    {
        if (strstr(domain, patterns[i]) != nullptr)
        {
            return true;
        }
    }

    return false;
}

bool find_in_rewrite(const char *domain, IPAddress &ip)
{
    const char *fname = "/rewrite";
    if (!SPIFFS.exists(fname))
    {
        Serial.printf("\nWarning: rewrite file not found\n");
        return false;
    }
    File f = SPIFFS.open(fname, "r");
    if (!f)
    {
        Serial.printf("\nError: file open failed\n");
        return false;
    }

    char line[64];
    while (f.available())
    {
        size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
        line[n] = 0;

        if (line == ",@@@")
            break;

        char *rewriteDomain = strtok(line, ",");
        char *ipAddress = strtok(NULL, ",");

        if (strstr(domain, rewriteDomain) != NULL)
        {
            int one, two, three, four;
            if (sscanf(ipAddress, "%d.%d.%d.%d", &one, &two, &three, &four) == 4)
            {
                ip = IPAddress(one, two, three, four);
                return true;
            }
        }
    }
    f.close();

    return false;
}

IPAddress handleDNSRequest(String dom)
{
    if (dom.startsWith("www."))
    {
        dom.remove(0, 4);
    }

    if ((dom.isEmpty()))
    {
        return IPAddress(0, 0, 0, 0);
    }

    Serial.println();
    Serial.print("Domain: ");
    Serial.print(dom.c_str());
    IPAddress ip;
    uint32_t oMillis = millis();
    if (find_in_rewrite(dom.c_str(), ip))
    {
        uint32_t rewriteMs = millis() - oMillis;
        Serial.print(" | IP:");
        Serial.print(ip);
        Serial.printf("\nRewrite took %lu ms\n", rewriteMs);
        enqueueDnsLog(false, dom.c_str(), rewriteMs, 0);
        return ip;
    }

    bool isBlock = easy_block(dom.c_str()) || bloomCheck(dom.c_str());
    uint32_t proccessMs = millis() - oMillis;
    if (isBlock)
    {
        Serial.printf(" Blocked | Find took %lu ms\n", proccessMs);
        enqueueDnsLog(true, dom.c_str(), proccessMs, 0);
        return IPAddress(0, 0, 0, 0);
    }

    oMillis = millis();
    WiFi.hostByName(dom.c_str(), ip);
    uint32_t resolvMs = millis() - oMillis;
    Serial.print(" | IP:");
    Serial.print(ip);
    if (ip == IPAddress(0, 0, 0, 0))
    {
        Serial.printf("\n Block by upstream took %lu ms", resolvMs);
        Serial.printf(" | Find took %lu ms\n", proccessMs);
        enqueueDnsLog(true, dom.c_str(), resolvMs, proccessMs);
    }
    else
    {
        Serial.printf("\nResolv took %lu ms", resolvMs);
        Serial.printf(" | Find took %lu ms\n", proccessMs);
        enqueueDnsLog(false, dom.c_str(), resolvMs, proccessMs);
    }
    
    return ip;
}

void setupLogQueue()
{
    dnsLogQueue = xQueueCreate(64, sizeof(DnsLogEvent));

    xTaskCreatePinnedToCore(
        statsTask,
        "stats",
        4096,
        nullptr,
        1,
        nullptr,
        0 // core 0 (DNS usually runs on core 1)
    );
}

void statsTask(void *arg)
{
    DnsLogEvent ev;

    for (;;)
    {
        if (xQueueReceive(dnsLogQueue, &ev, portMAX_DELAY))
        {
            recordQuery(ev.blocked, ev.domain, ev.resolveMs, ev.proccessMs);
        }
    }
}

void enqueueDnsLog(bool blocked, const char *domain, uint32_t resolvMs, uint32_t proccessMs)
{
    if (!dnsLogQueue)
        return;

    DnsLogEvent ev{};
    ev.blocked = blocked;
    ev.resolveMs = resolvMs;
    ev.proccessMs = proccessMs;
    strncpy(ev.domain, domain, MAX_DOMAIN_LEN - 1);

    // Do NOT block DNS if queue is full
    xQueueSend(dnsLogQueue, &ev, 0);
}