#include "StatsManager.h"
#include "PuckNetwork.h"
#include <LittleFS.h>

Preferences statsPrefs;
uint32_t gameStarts[30] = {0}; // index 1..28 used

// --- Playtime Tracking (dual storage with integrity check) ---
static const uint32_t PT_MAGIC = 0xA7C3E1B9;  // integrity seed
static const char* PT_FILE = "/._sys.dat";      // LittleFS backup
static uint32_t totalPlaytimeMin = 0;           // accumulated minutes
static unsigned long sessionStartMs = 0;        // millis() when current game started
static bool sessionActive = false;

// Simple integrity: store value + XOR checksum
static uint32_t ptChecksum(uint32_t val) {
    return val ^ PT_MAGIC;
}

static void ptSaveToFile(uint32_t minutes) {
    File f = LittleFS.open(PT_FILE, "w");
    if (!f) return;
    uint32_t chk = ptChecksum(minutes);
    f.write((uint8_t*)&minutes, 4);
    f.write((uint8_t*)&chk, 4);
    f.close();
}

static uint32_t ptLoadFromFile() {
    File f = LittleFS.open(PT_FILE, "r");
    if (!f || f.size() < 8) { if (f) f.close(); return 0; }
    uint32_t val = 0, chk = 0;
    f.read((uint8_t*)&val, 4);
    f.read((uint8_t*)&chk, 4);
    f.close();
    if (chk == ptChecksum(val)) return val;
    Serial.println("STATS: LittleFS playtime integrity fail!");
    return 0;
}

static void ptSaveToNVS(uint32_t minutes) {
    Preferences p;
    p.begin("pr_hw", false);
    p.putUInt("_ct", minutes);
    p.putUInt("_cv", ptChecksum(minutes));
    p.end();
}

static uint32_t ptLoadFromNVS() {
    Preferences p;
    p.begin("pr_hw", true);
    uint32_t val = p.getUInt("_ct", 0);
    uint32_t chk = p.getUInt("_cv", 0);
    p.end();
    if (chk == ptChecksum(val)) return val;
    Serial.println("STATS: NVS playtime integrity fail!");
    return 0;
}

// Helfer um MAC Adresse in einen kurzen String (die letzten 6 Zeichen) als Speicher-Key zu wandeln
String macToStr(const uint8_t* mac) {
    char buf[13];
    sprintf(buf, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

void StatsManager::begin() {
    statsPrefs.begin("pr_stats", false);
    for(int i=1; i<=28; i++) {
        gameStarts[i] = statsPrefs.getUInt(String("g" + String(i)).c_str(), 0);
    }
    statsPrefs.end();

    // Load playtime: take the HIGHER of the two stores (tamper protection)
    uint32_t fromNVS = ptLoadFromNVS();
    uint32_t fromFile = ptLoadFromFile();
    totalPlaytimeMin = max(fromNVS, fromFile);
    Serial.printf("STATS: Playtime loaded: %u min (NVS=%u, File=%u)\n", totalPlaytimeMin, fromNVS, fromFile);

    Serial.println("STATS: System geladen.");
}

void StatsManager::addGameStart(int gameID) {
    if(gameID >= 1 && gameID <= 28) {
        gameStarts[gameID]++;
    }
}

uint32_t StatsManager::getGameStarts(int gameID) {
    if(gameID >= 1 && gameID <= 28) return gameStarts[gameID];
    return 0;
}

uint32_t StatsManager::getPuckClicks(const uint8_t* mac) {
    statsPrefs.begin("pr_stats", true); // read-only
    String key = macToStr(mac).substring(6) + "_c"; 
    uint32_t val = statsPrefs.getUInt(key.c_str(), 0);
    statsPrefs.end();
    return val;
}

uint32_t StatsManager::getPuckTime(const uint8_t* mac) {
    statsPrefs.begin("pr_stats", true);
    String key = macToStr(mac).substring(6) + "_t"; 
    uint32_t val = statsPrefs.getUInt(key.c_str(), 0);
    statsPrefs.end();
    return val;
}

// --- Playtime API ---

void StatsManager::startPlaytime() {
    if (!sessionActive) {
        sessionStartMs = millis();
        sessionActive = true;
        Serial.println("STATS: Playtime session started.");
    }
}

void StatsManager::stopPlaytime() {
    if (sessionActive) {
        unsigned long elapsed = millis() - sessionStartMs;
        uint32_t addedMin = elapsed / 60000UL;
        totalPlaytimeMin += addedMin;
        sessionActive = false;
        Serial.printf("STATS: Playtime session ended. +%u min → total %u min\n", addedMin, totalPlaytimeMin);
        // Save immediately to both stores
        ptSaveToNVS(totalPlaytimeMin);
        ptSaveToFile(totalPlaytimeMin);
    }
}

uint32_t StatsManager::getTotalPlaytimeMinutes() {
    uint32_t current = totalPlaytimeMin;
    if (sessionActive) {
        current += (millis() - sessionStartMs) / 60000UL;
    }
    return current;
}

void StatsManager::saveAll() {
    Serial.println("STATS: Speichere Daten in den Flash...");
    statsPrefs.begin("pr_stats", false);

    // Spiel-Aufrufe speichern
    for(int i=1; i<=28; i++) {
        statsPrefs.putUInt(String("g" + String(i)).c_str(), gameStarts[i]);
    }

    // Aktive Pucks speichern
    PuckInfo* pucks = PuckNetwork::getPucks();
    for(int i=0; i<MAX_PEERS; i++) {
        if(pucks[i].active) {
            String baseKey = macToStr(pucks[i].mac).substring(6);
            statsPrefs.putUInt((baseKey + "_c").c_str(), pucks[i].totalClicks);
            statsPrefs.putUInt((baseKey + "_t").c_str(), pucks[i].totalMinutes);
        }
    }

    statsPrefs.end();

    // Playtime: also persist running session so far (crash protection)
    uint32_t pt = getTotalPlaytimeMinutes();
    ptSaveToNVS(pt);
    ptSaveToFile(pt);

    Serial.println("STATS: Speicherung erfolgreich!");
}