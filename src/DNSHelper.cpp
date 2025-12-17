#include <DNSHelper.h>

#include <SPIFFS.h>
#include <WebServerHelper.h>
#include <WiFi.h>

bool easy_block(const char *domain)
{
    String list[] = {
        "ad.",
        "ads.",
        "adserver",
        "adservers",
        "adtrack",
        "adtracker",
        "adservice",
        "adservices",
        "advert",
        "advertising",
        "analytics",
        "applytics",
        "beacons",
        "logging",
        "pub.",
        "tracker",
        "tracking",
        "telemetry",

    };
    for (auto &text : list)
    {
        if (strstr(domain, text.c_str()))
        {
            return true;
        }
    }

    return false;
}

bool find_in_block(const char *domain)
{
    int len = strlen(domain);
    char first = domain[0];

    char fname[32];
    // Try split file first
    snprintf(fname, sizeof(fname), "/hosts_%d_%c", len, first);
    if (!SPIFFS.exists(fname))
    {
        // Fallback to unsplit file
        snprintf(fname, sizeof(fname), "/hosts_%d", len);
    }

    if (!SPIFFS.exists(fname))
    {
        // domain length and letter does not match any of our lists
        return false;
    }

    File f = SPIFFS.open(fname, "r");
    if (!f)
    {
        Serial.print("\nError: file open failed\n");
        return false;
    }

    f.seek(0, SeekSet);
    bool found = f.findUntil(domain, "@@@");
    f.close();

    return found;
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
        recordQuery(false, dom.c_str(), rewriteMs);
        return ip;
    }

    bool block = easy_block(dom.c_str());
    block = block || find_in_block(dom.c_str());
    uint32_t proccessMs = millis() - oMillis;
    if (block)
    {
        Serial.printf(" Blocked | Find took %lu ms\n", proccessMs);
        recordQuery(true, dom.c_str(), proccessMs);
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
        recordQuery(true, dom.c_str(), resolvMs + proccessMs);
    }
    else
    {
        Serial.printf("\nResolv took %lu ms", resolvMs);
        Serial.printf(" | Find took %lu ms\n", proccessMs);
        recordQuery(false, dom.c_str(), resolvMs + proccessMs);
    }
    return ip;
}