#include "GameManager.h"
#include "PuckNetwork.h"

#include "Game_SimpleCounter.h"
#include "Game_SimpleCountdown.h"
#include "Game_ShuttleRun.h"
#include "Game_SimonSays.h" 
#include "Game_DisplayMode.h" 
#include "Game_SimonRuns.h"
#include "Game_SortingHat.h"
#include "Game_BombSquad.h" 
#include "Game_RedGreen.h" 
#include "Game_Stopwatch.h"
#include "Game_Zombie.h"
#include "Game_BeepTest.h"
#include "Game_React2P.h"
#include "Game_TTest.h"
#include "Game_Target.h"
#include "Game_Timer.h"
#include "Game_Hunt.h"
#include "Game_Pacemaker.h"
#include "Game_Memory.h"
#include "Game_Domination.h"
#include "Game_Batak.h"
#include "Game_Tabata.h"
#include "Game_Ball.h"
#include "Game_Whac.h"
#include "Game_CatReflex.h"
#include "Game_Musical.h" 
#include "Game_TTTT.h" 
#include "StatsManager.h" 


Game* GameManager::currentGame = nullptr;

Game_ShuttleRun gameShuttleRun;      // ID 1
Game_SimpleCounter gameCounter;      // ID 2
Game_SimpleCountdown gameCountdown;  // ID 3
Game_SimonSays gameSimon;            // ID 4
Game_DisplayMode gameDisplay;        // ID 5 
Game_SimonRuns gameSimonRuns;        // ID 6
Game_SortingHat gameSorting;         // ID 7
Game_BombSquad gameBomb;             // ID 8 
Game_RedGreen gameRedGreen;           // ID 9 
Game_Stopwatch gameStopwatch;           // ID 10 
Game_Zombie gameZombie;          // ID 11
Game_BeepTest gameBeepTest;      // ID12
Game_React2P gameReact2P;        // ID13
Game_TTest gameTTest;            // ID14
Game_Target gameTarget;          // ID15
Game_Timer gameTimer;            // ID16
Game_Hunt gameHunt;              // ID 17
Game_Pacemaker gamePacemaker;        // ID 18
Game_Memory gameMemory;          // ID 19
Game_Domination gameDomination;    // ID 20
Game_Batak gameBatak;          // ID 21
Game_Tabata gameTabata;          // ID 22
Game_Ball gameBall;          // ID 23
Game_Whac gameWhac; // ID 24
Game_CatReflex gameCatReflex;   // ID 25
Game_Musical gameMusical;   // ID 26
Game_TTTT gameTTTT;   // ID 27

// Global Variables form RAM watchdog
int lowestHeapGameId = 0;
int lowestHeapValue = 999999;
int lastGameId = 0;


void GameManager::begin() {
    Serial.println("GM: Game Engine gestartet.");
    StatsManager::begin(); // NEU
}

void GameManager::update() {
    if (currentGame) currentGame->loop();
}

void GameManager::startGame(int gameID) {
    Serial.printf("GM: Starte Spiel ID %d\n", gameID);
    StatsManager::addGameStart(gameID);
    currentGame = nullptr;

    switch(gameID) {
        case 1: currentGame = &gameShuttleRun; break;
        case 2: currentGame = &gameCounter; break;
        case 3: currentGame = &gameCountdown; break;
        case 4: currentGame = &gameSimon; break;
        case 5: currentGame = &gameDisplay; break; 
        case 6: currentGame = &gameSimonRuns; break;
        case 7: currentGame = &gameSorting; break;
        case 8: currentGame = &gameBomb; break; 
        case 9: currentGame = &gameRedGreen; break; 
        case 10: currentGame = &gameStopwatch; break; 
        case 11: currentGame = &gameZombie; break;
        case 12: currentGame = &gameBeepTest; break;
        case 13: currentGame = &gameReact2P; break;
        case 14: currentGame = &gameTTest; break;
        case 15: currentGame = &gameTarget; break;
        case 16: currentGame = &gameTimer; break;
        case 17: currentGame = &gameHunt; break;
        case 18: currentGame = &gamePacemaker; break;
        case 19: currentGame = &gameMemory; break;
        case 20: currentGame = &gameDomination; break;
        case 21: currentGame = &gameBatak; break;
        case 22: currentGame = &gameTabata; break;
        case 23: currentGame = &gameBall; break;
        case 24: currentGame = &gameWhac; break;
        case 25: currentGame = &gameCatReflex; break;
        case 26: currentGame = &gameMusical; break;
        case 27: currentGame = &gameTTTT; break;
        
        default: Serial.println("Unbekannte ID"); return;
    }
    
    if (currentGame) {
        // RAM watchdog: check heap from PREVIOUS game
        int h = ESP.getMinFreeHeap();
        if (h < lowestHeapValue) {
            lowestHeapValue = h;
            lowestHeapGameId = lastGameId;
        }
        lastGameId = gameID; // das letzte spiel wegschreiben
        Serial.print("Spiel geladen: "); Serial.println(currentGame->getName());
        currentGame->setup();
        
    }
}

Game* GameManager::getCurrentGame() { return currentGame; }

void GameManager::handlePuckEvent(const uint8_t* mac, EventPacket event) {
    if (!currentGame) return;
    PuckInfo* pucks = PuckNetwork::getPucks();
    int puckIndex = -1;
    for(int i=0; i<MAX_PEERS; i++) {
        if (pucks[i].active && memcmp(pucks[i].mac, mac, 6) == 0) {
            puckIndex = i; break;
        }
    }
    if (puckIndex != -1) {
        currentGame->handleEvent(puckIndex, event);
    }
}