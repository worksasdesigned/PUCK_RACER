#ifndef STATSMANAGER_H
#define STATSMANAGER_H

#include <Arduino.h>
#include <Preferences.h>

class StatsManager {
public:
    static void begin();
    static void addGameStart(int gameID);
    static uint32_t getGameStarts(int gameID);

    static uint32_t getPuckClicks(const uint8_t* mac);
    static uint32_t getPuckTime(const uint8_t* mac);

    // Playtime tracking (shareware)
    static void startPlaytime();
    static void stopPlaytime();
    static uint32_t getTotalPlaytimeMinutes();

    static void saveAll();
};

#endif