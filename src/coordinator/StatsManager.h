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
    
    static void saveAll(); 
};

#endif