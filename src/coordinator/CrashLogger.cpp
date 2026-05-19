#include "CrashLogger.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include "Common.h"
#include "PuckNetwork.h"
#include "GameManager.h"

// In GameManager.cpp: int lastGameId = 0; (Global, kein static)
extern int lastGameId;

static const char* LOG_PATH = "/crashlog.csv";
static const char* OLD_PATH = "/crashlog.old";
static const unsigned long SAMPLE_INTERVAL_MS = 5000UL;
static const size_t MAX_LOG_BYTES = 256UL * 1024UL;
// CSV-Header steht in writeBootMarker(). Bei Format-Änderung Versionsnummer bumpen,
// damit ältere Log-Dateien erkennbar bleiben.
static const char* LOG_VERSION = "v1";

unsigned long CrashLogger::lastSample = 0;
unsigned long CrashLogger::startMs = 0;
bool CrashLogger::initialized = false;

void CrashLogger::begin() {
    startMs = millis();
    if (!LittleFS.begin(false)) {
        Serial.println("CrashLogger: LittleFS nicht bereit, deaktiviert.");
        return;
    }
    initialized = true;
    rotateIfNeeded();
    writeBootMarker();
    lastSample = millis();
}

void CrashLogger::update() {
    if (!initialized) return;
    unsigned long now = millis();
    if (now - lastSample < SAMPLE_INTERVAL_MS) return;
    lastSample = now;
    writeSample();
}

void CrashLogger::flush() {
    if (!initialized) return;
    File f = LittleFS.open(LOG_PATH, "a");
    if (!f) return;
    f.printf("S,%lu,shutdown\n", (millis() - startMs) / 1000UL);
    f.close();
}

void CrashLogger::rotateIfNeeded() {
    File f = LittleFS.open(LOG_PATH, "r");
    if (!f) return;
    size_t sz = f.size();
    f.close();
    if (sz < MAX_LOG_BYTES) return;

    if (LittleFS.exists(OLD_PATH)) LittleFS.remove(OLD_PATH);
    LittleFS.rename(LOG_PATH, OLD_PATH);
    Serial.printf("CrashLogger: rotated (%u bytes -> %s)\n", (unsigned)sz, OLD_PATH);
}

void CrashLogger::writeBootMarker() {
    File f = LittleFS.open(LOG_PATH, "a");
    if (!f) return;

    bool isEmpty = (f.size() == 0);
    if (isEmpty) {
        f.printf("# PuckRacer CrashLog %s — col: type,uptime_s,...\n", LOG_VERSION);
        f.println("# S = sample: S,uptime_s,heap_free,heap_min,largest,stack_hw,tx_ok,tx_fail,sc,cb,rx,qo,pucks,gid,clients");
        f.println("# B = boot:   B,uptime_s,reset_code,reset_reason");
    }

    esp_reset_reason_t r = esp_reset_reason();
    f.printf("B,0,%d,%s\n", (int)r, resetReasonStr((int)r));
    f.close();
}

void CrashLogger::writeSample() {
    File f = LittleFS.open(LOG_PATH, "a");
    if (!f) return;

    unsigned long uptime_s = (millis() - startMs) / 1000UL;
    uint32_t heapFree    = ESP.getFreeHeap();
    uint32_t heapMin     = ESP.getMinFreeHeap();
    uint32_t largest     = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    UBaseType_t stackHW  = uxTaskGetStackHighWaterMark(NULL);

    NetworkStats s = PuckNetwork::getStats();

    int activePucks = 0;
    PuckInfo* pucks = PuckNetwork::getPucks();
    for (int i = 0; i < MAX_PEERS; i++) {
        if (pucks[i].active) activePucks++;
    }

    int gameId = (GameManager::getCurrentGame() != nullptr) ? lastGameId : 0;
    int clients = WiFi.softAPgetStationNum();

    f.printf("S,%lu,%u,%u,%u,%u,%lu,%lu,%lu,%lu,%lu,%lu,%d,%d,%d\n",
        uptime_s,
        heapFree, heapMin, largest,
        (unsigned)stackHW,
        s.successfulTx, s.failedTx, s.sendCalls, s.sendCallbacks,
        s.totalPacketsRx, s.queueOverflows,
        activePucks, gameId, clients);

    f.close();
}

const char* CrashLogger::resetReasonStr(int reason) {
    switch ((esp_reset_reason_t)reason) {
        case ESP_RST_POWERON:   return "POWERON";
        case ESP_RST_EXT:       return "EXT";
        case ESP_RST_SW:        return "SW";
        case ESP_RST_PANIC:     return "PANIC";
        case ESP_RST_INT_WDT:   return "INT_WDT";
        case ESP_RST_TASK_WDT:  return "TASK_WDT";
        case ESP_RST_WDT:       return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT:  return "BROWNOUT";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "UNKNOWN";
    }
}
