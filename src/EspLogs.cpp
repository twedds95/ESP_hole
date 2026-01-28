#include <EspLogs.h>

static File logFile;
static uint32_t lastFlush = 0;
ESPHOLE_LOGLEVEL LOG_LEVEL = ESPHOLE_LOGLEVEL::INFO; // default

void dualPrintLogf(ESPHOLE_LOGLEVEL logLevel, ESPHOLE_LOGTYPES tagEnum, const char *fmt, ...)
{
    if (logLevel > LOG_LEVEL)
        return;

    const char *tag = LOG_TAG(tagEnum);
    char msg[MAX_LOG_MSG_LEN];
    char buf[MAX_LOG_MSG_LEN + 64];

    va_list arg;
    va_start(arg, fmt);
    int msg_len = vsnprintf(msg, sizeof(msg), fmt, arg);
    va_end(arg);
    if (msg_len <= 0)
        return;

    int len = snprintf(buf, sizeof(buf), "[%s] %s\n", tag, msg);
    if (len <= 0)
        return;

    // Serial output
    Serial.write((uint8_t *)buf, len);

    if (!logFile)
    {
        Serial.println("Logfile not found.");
        return;
    }

    if (logFile.size() > 1024)
    { // 1 KB / log
        rollLog();
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
}

void rollLog()
{
    String oldest = String(logName) + String(MAX_LOGS - 1);
    if (SPIFFS.exists(oldest))
        SPIFFS.remove(oldest);

    for (int i = MAX_LOGS - 2; i >= 0; i--)
    {
        String oldName = String(logName) + String(i);
        String newName = String(logName) + String(i + 1);
        if (SPIFFS.exists(oldName))
            SPIFFS.rename(oldName, newName);
    }

    String newLog = String(logName);
    logFile.close();
    logFile = SPIFFS.open(newLog, FILE_APPEND);
}

void setupLogs(ESPHOLE_LOGLEVEL lvl)
{
    LOG_LEVEL = lvl;
    logFile = SPIFFS.open(logName, FILE_APPEND);
    if (!logFile)
    {
        Serial.println("Logfile not opened.");
    }
    else
    {
        dualPrintLogf(ESPHOLE_LOGLEVEL::INFO, ESPHOLE_LOGTYPES::CODE, "Loggin Initialized with Log Level: %s", LOG_LVL(lvl));
    }
}