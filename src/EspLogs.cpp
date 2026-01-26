#include <EspLogs.h>

#include <Arduino.h>
#include "SPIFFS.h"
#include <stdarg.h>

const char* logName = "/esplogs";
static File logFile;
static uint32_t lastFlush = 0;

int dualPrintf(const char *fmt, va_list args)
{
    char buf[256];

    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len <= 0)
        return len;

    // Serial output
    Serial.write((uint8_t *)buf, len);

    if (logFile.size() > 64 * 1024)
    { // 64 KB
        logFile.close();
        SPIFFS.remove(logName);
        logFile = SPIFFS.open(logName, FILE_APPEND);
    }

    // File output
    if (logFile)
    {
        logFile.write((uint8_t *)buf, len);
        if (millis() - lastFlush > 1000)
        {
            logFile.flush();
            lastFlush = millis();
        }
    }

    return len;
}

void setupLogs(esp_log_level_t level)
{
    logFile = SPIFFS.open(logName, FILE_APPEND);

    // Redirect all printf()
    esp_log_set_vprintf(dualPrintf);
    esp_log_level_set("*", level);
    esp_log_level_set(LOG_TAG(ESPHOLE_LOGTYPES::STATS), ESP_LOG_DEBUG);
}