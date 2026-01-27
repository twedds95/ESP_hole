#include <Arduino.h>

// built-ins
#include <FS.h>
#include <SPIFFS.h>
#include <WiFiManager.h>

// customs
#include <BloomCheck.h>
#include <DNSHelper.h>
#include <DNSOverrideLists.h>
#include <DNSServer.h>
#include <EspLogs.h>
#include <WebServerHelper.h>

// Edit these to match your preference
IPAddress primaryDNS(94, 140, 14, 14);  // adguard
IPAddress secondaryDNS(194, 242, 2, 4); // mullvad

// this setting has been very stable for the S2 Mini, YMMV
wifi_power_t WiFiTxPower = WIFI_POWER_15dBm;

const byte DNS_PORT = 53;
DNSServer dnsServer;

void setupWifi();

void setup()
{
    Serial.begin(9600);
    setupWifi();
    if (SPIFFS.begin(true))
    {
        Serial.println("SPIFFS mounted successfully");
    }

    setupLogs(ESPHOLE_LOGLEVEL::INFO);

    dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
    bool dns_running = dnsServer.start(DNS_PORT, "*", WiFi.localIP());
    if (!dns_running)
    {
        dualPrintLogf(ESPHOLE_LOGLEVEL::ERROR, ESPHOLE_LOGTYPES::DNS, "DNS Server not running");
        return;
    }

    setupBloom();
    setupDNSHelper();
    dualPrintLogf(ESPHOLE_LOGLEVEL::INFO, ESPHOLE_LOGTYPES::DNS, "Upstream DNSs: %s, %s", WiFi.dnsIP(0).toString().c_str(), WiFi.dnsIP(1).toString().c_str());

    setupDNSLists();
    setupServerHelper();
}

void setupWifi()
{
    delay(10);
    WiFiManager wm;
    bool res;
    res = wm.autoConnect("AutoConnectAP_ESPHOLE", "ESP32_Connect"); // password protected ap

    if (!res)
    {
        dualPrintLogf(ESPHOLE_LOGLEVEL::ERROR, ESPHOLE_LOGTYPES::WIFI, "Failed to connect to WiFi");
    }
    else
    {
        // if you get here you have connected to the WiFi
        dualPrintLogf(ESPHOLE_LOGLEVEL::INFO, ESPHOLE_LOGTYPES::WIFI, "Connected to WiFi! :)");
        String wifi_ssid = wm.getWiFiSSID();
        String wifi_password = wm.getWiFiPass();

        dualPrintLogf(ESPHOLE_LOGLEVEL::INFO, ESPHOLE_LOGTYPES::WIFI, "WiFi SSID: %s", wifi_ssid.c_str());
        WiFi.begin(wifi_ssid, wifi_password);
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
        }

        dualPrintLogf(ESPHOLE_LOGLEVEL::INFO, ESPHOLE_LOGTYPES::WIFI, "WiFi connected | IP address: %s", WiFi.localIP().toString().c_str());
        WiFi.setTxPower(WiFiTxPower);
        // update upstream DNS
        WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), primaryDNS, secondaryDNS);        
    }
}

void loop()
{
    handleTimeSensitiveRotations();
    int dnsOK = dnsServer.processNextRequest();
    if (dnsOK == 0)
    {
        IPAddress ip = handleDNSRequest(dnsServer.getQueryDomainName());
        dnsServer.replyWithIP(ip);
    }
}
