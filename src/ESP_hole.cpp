#include <Arduino.h>

// built-ins
#include <FS.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <WiFiManager.h>

// customs
#include <Dashboard.cpp>
#include <DNSHelper.h>
#include <DNSServer.h>
#include <WebServerHelper.h>

// Edit these to match your preference
IPAddress primaryDNS(94, 140, 14, 14);  // adguard
IPAddress secondaryDNS(194, 242, 2, 4); // mullvad

const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer server(80);

void handleRoot();
void handleStats();
void setup_wifi();

void handleRoot()
{
    server.send_P(200, "text/html", DASHBOARD_HTML);
}

void handleStats()
{
    server.send(200, "application/json", getJsonStats());
}

void setup()
{
    Serial.begin(9600);
    setup_wifi();
    if (SPIFFS.begin(true))
    {
        Serial.println("SPIFFS mounted successfully");
    }

    dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
    bool dns_running = dnsServer.start(DNS_PORT, "*", WiFi.localIP());
    if (!dns_running)
    {
        Serial.println("Error: DNS Server not running");
        return;
    }

    Serial.println("DNS Server ready");
    Serial.println("Upstream DNSs:");
    Serial.println(WiFi.dnsIP(0));
    Serial.println(WiFi.dnsIP(1));

    loadPersistedStats();

    server.on("/", handleRoot);
    server.on("/stats", handleStats);
    server.begin();
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

void loop()
{
    server.handleClient();
    handleHourRotation();
    int dnsOK = dnsServer.processNextRequest();
    if (dnsOK == 0)
    {
        IPAddress ip = handleDNSRequest(dnsServer.getQueryDomainName());
        dnsServer.replyWithIP(ip);
    }
}
