#include <Arduino.h>
#include <FS.h>
#include "DNSServer.h"
#include <SPIFFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>

// Edit these to match your preference
IPAddress primaryDNS(94, 140, 14, 14); // adguard
IPAddress secondaryDNS(194, 242, 2, 4); // mullvad

const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer webServer(80);
File f;

void setup_wifi();
bool easy_block(String dom);
bool find_in_block(String dom);
bool find_in_rewrite(String domain, IPAddress &ip);

void setup()
{
    Serial.begin(9600);
    setup_wifi();
    if (SPIFFS.begin(true)) {
        Serial.println("SPIFFS mounted successfully");
    }

    dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
    bool dns_running = dnsServer.start(DNS_PORT, "*", WiFi.localIP());
    if (dns_running)
    {
        Serial.println("DNS Server ready");
        Serial.println("Upstream DNSs:");
        Serial.println(WiFi.dnsIP(0));
        Serial.println(WiFi.dnsIP(1));
    }
    else
    {
        Serial.println("Error: DNS Server not running");
    }
}

void setup_wifi()
{
    delay(10);
    Serial.println();
    Serial.print("Connecting to: ");
    WiFiManager wm;
    bool res;
    res = wm.autoConnect("AutoConnectAP_ESPHOLE", "ESP32_Connect"); // password protected ap

    if (!res)
    {
        Serial.println("Failed to connect");
    }
    else
    {
        // if you get here you have connected to the WiFi
        Serial.println("Connected! :)");
        String wifi_ssid = wm.getWiFiSSID();
        String wifi_password = wm.getWiFiPass();

        Serial.println(wifi_ssid);
        WiFi.begin(wifi_ssid, wifi_password);
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
            Serial.print(".");
        }
        Serial.println("");
        Serial.print("WiFi connected | IP address: ");
        Serial.println(WiFi.localIP());

        // update upstream DNS
        WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), primaryDNS, secondaryDNS);
    }
}

bool easy_block(String dom)
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
        if (dom.indexOf(text) >= 0)
        {
            return true;
        }
    }

    return false;
}

bool find_in_block(String domain)
{
    int len = domain.length();
    char first = domain[0];

    char fname[32];
    // Try split file first
    snprintf(fname, sizeof(fname), "/hosts_%d_%c", len, first);
    File f = SPIFFS.open(fname, "r");
    if (!f)
    {
        // Fallback to unsplit file
        snprintf(fname, sizeof(fname), "/hosts_%d", len);
        f = SPIFFS.open(fname, "r");
    }
    if (!f)
    {
        Serial.printf("\nError: file open failed\n");
        return false;
    }
    
    f.seek(0, SeekSet);
    char dom_str[domain.length()];
    sprintf(dom_str, "%s", domain.c_str());
    bool found = f.findUntil(dom_str, "@@@");
    f.close();

    return found;
}

bool find_in_rewrite(String domain, IPAddress &ip)
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

        if (strstr(domain.c_str(), rewriteDomain) != NULL)
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


void loop()
{
    int dnsOK = dnsServer.processNextRequest();
    if (dnsOK == 0)
    {
        String dom = dnsServer.getQueryDomainName();
        if (dom.startsWith("www."))
            dom.remove(0, 4);
        f.setTimeout(5000);
        if ((dom != ""))
        {
            Serial.println();
            Serial.print("Domain: ");
            Serial.print(dom);
            IPAddress ip;
            uint32_t oMillis = millis();
            if (find_in_rewrite(dom, ip))
            {                
                dnsServer.replyWithIP(ip);
                uint32_t rewriteMs = millis() - oMillis;
                Serial.print(" | IP:");
                Serial.print(ip);
                Serial.printf("\nRewrite took %lu ms\n", rewriteMs);
                return;
            }

            oMillis = millis();
            bool block = easy_block(dom);
            block = block || find_in_block(dom);
            uint32_t proccessMs = millis() - oMillis;
            if (block)
            {
                Serial.printf(" Blocked | Find took %lu ms\n", proccessMs);
                dnsServer.replyWithIP(IPAddress(0, 0, 0, 0));
                return;
            }

            oMillis = millis();
            WiFi.hostByName(dom.c_str(), ip);
            dnsServer.replyWithIP(ip);
            uint32_t resolvMs = millis() - oMillis;
            Serial.print(" | IP:");
            Serial.print(ip);
            Serial.printf("\nResolv took %lu ms", resolvMs);
            Serial.printf(" | Find took %lu ms\n", proccessMs);
            dom = "";
        }
    }
}
