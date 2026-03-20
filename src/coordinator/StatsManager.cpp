#include "StatsManager.h"
#include "PuckNetwork.h"

Preferences statsPrefs;
uint32_t gameStarts[30] = {0}; // index 1..28 used

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
    Serial.println("STATS: Speicherung erfolgreich!");
}