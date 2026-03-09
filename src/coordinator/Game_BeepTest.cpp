#include "Game_BeepTest.h"
#include "PuckNetwork.h"

void Game_BeepTest::setup() {
    Serial.println("GAME: Setup Beep Test");
    initGame();
}

void Game_BeepTest::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "cfg_players") numPlayers = constrain(value, 1, 10);
    else if (cmd == "start") {
        resetPlayerStats();
        globalCountdownStart = 0;
        testStartTime = 0;
        finishTime = 0;
        setGameState(BEEP_WAIT_HANDS); 
    }
    else if (cmd == "reset") {
        initGame(); 
    }
    else if (cmd == "stop") {
        setGameState(BEEP_FINISHED);
    }
    else if (cmd == "exit") {
        CommandPacket cp; memset(&cp, 0, sizeof(cp));
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_STATUS; cp.r = 0; cp.g = 255; cp.b = 0; cp.extra = 255;
        PuckNetwork::broadcast(cp);
        state = BEEP_SETUP;
    }
    else if (cmd == "elim_player") {
        if (value >= 0 && value < numPlayers) eliminatePlayer(value, true);
    }
    else if (cmd == "clear_card") {
        if (value >= 0 && value < numPlayers && players[value].yellowCards > 0) {
            players[value].yellowCards--;
        }
    }
}

void Game_BeepTest::initGame() {
    globalCountdownStart = 0;
    testStartTime = 0;
    finishTime = 0;
    globalLap = 1;
    currentLevel = 1;
    currentSpeed = 8.5;
    
    PuckInfo* pucks = PuckNetwork::getPucks();
    int assigned = 0;

    for(int i=0; i<10; i++) {
        players[i].id = i;
        players[i].color = PLAYER_COLORS[i % 10];
        
        if (i < numPlayers) {
            while(assigned < MAX_PEERS && !pucks[assigned].active) assigned++;
            players[i].startPuckIdx = (assigned < MAX_PEERS) ? assigned++ : -1;
            
            while(assigned < MAX_PEERS && !pucks[assigned].active) assigned++;
            players[i].targetPuckIdx = (assigned < MAX_PEERS) ? assigned++ : -1;
        } else {
            players[i].startPuckIdx = -1;
            players[i].targetPuckIdx = -1;
        }
    }
    
    resetPlayerStats();
    setGameState(BEEP_SETUP);
}

void Game_BeepTest::resetPlayerStats() {
    for(int i=0; i<numPlayers; i++) {
        if (players[i].startPuckIdx != -1 && players[i].targetPuckIdx != -1) {
            players[i].isEliminated = false;
            players[i].playerLap = 1;
            players[i].yellowCards = 0;
            players[i].vo2max = 0.0;
            players[i].falseStart = false;
            players[i].isHolding = false;
            players[i].currentPuckTarget = 1; 
            players[i].visualState = -1;
            players[i].errorTime = 0;
            players[i].highestLevel = 1;
        } else {
            players[i].isEliminated = true;
        }
    }
}

void Game_BeepTest::setGameState(BeepGameState newState) {
    state = newState;
    stateStartTime = millis();
    
    if (newState == BEEP_SETUP || newState == BEEP_WAIT_HANDS) {
        for(int i=0; i<numPlayers; i++) {
            if (!players[i].isEliminated) {
                players[i].isHolding = false;
                setPuck(players[i].startPuckIdx, EFF_SINGLE_CHASE, players[i].color, 40, 150);
                setPuck(players[i].targetPuckIdx, EFF_BREATHE_MOD4, players[i].color, 50, 50);
            }
        }
    }
    else if (newState == BEEP_FINISHED) {
        finishTime = millis();
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 1000;
        PuckNetwork::broadcast(snd);
        
        for(int i=0; i<numPlayers; i++) {
            if (!players[i].isEliminated) {
                setPuck(players[i].startPuckIdx, EFF_RAINBOW, CRGB::Black, 0, 200);
                setPuck(players[i].targetPuckIdx, EFF_RAINBOW, CRGB::Black, 0, 200);
            }
        }
    }
}

void Game_BeepTest::loop() {
    unsigned long now = millis();
    
    // --- 1. COUNTDOWN LOGIK ---
    if (state == BEEP_WAIT_HANDS || state == BEEP_COUNTDOWN) {
        bool anyoneWaiting = false;
        bool allHolding = true;

        for (int i=0; i<numPlayers; i++) {
            if (!players[i].isEliminated) {
                anyoneWaiting = true;
                if (!players[i].isHolding) allHolding = false;
            }
        }

        if (anyoneWaiting && allHolding && globalCountdownStart == 0) {
            globalCountdownStart = now;
            CommandPacket snd; memset(&snd, 0, sizeof(snd)); 
            snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
            PuckNetwork::broadcast(snd);
            setGameState(BEEP_COUNTDOWN);
        }
        else if (globalCountdownStart > 0 && !allHolding && anyoneWaiting) {
            globalCountdownStart = 0; 
            for (int i=0; i<numPlayers; i++) {
                if (!players[i].isEliminated && !players[i].isHolding) {
                    triggerFalseStart(i);
                }
            }
            if (state == BEEP_COUNTDOWN) setGameState(BEEP_WAIT_HANDS); 
        }

        if (globalCountdownStart > 0 && now - globalCountdownStart >= 3000) {
            testStartTime = now;
            unsigned long accumulatedTime = now;
            float calcSpeed = 8.5;
            int calcLevel = 1;
            int lapsInLevel = 0;
            
            // Vorausberechnung aller Rundenzeiten
            for (int i=1; i<=200; i++) {
                unsigned long dur = (unsigned long)(72000.0 / calcSpeed);
                accumulatedTime += dur;
                schedule[i] = accumulatedTime; 
                
                lapsInLevel++;
                if (calcLevel <= 21 && lapsInLevel >= lapsPerLevel[calcLevel-1]) {
                    calcLevel++;
                    lapsInLevel = 0;
                    calcSpeed = 8.0 + (calcLevel * 0.5);
                }
            }
            totalScheduleLaps = 200;
            globalLap = 1;
            currentLevel = 1;
            currentSpeed = 8.5;
            
            setGameState(BEEP_RUNNING);
        }
    }
    
    // --- 2. ERHOLUNG VOM FEHLSTART ---
    if (state == BEEP_FALSE_START) {
        if (now - stateStartTime > 2000) {
            for(int i=0; i<numPlayers; i++) players[i].falseStart = false;
            setGameState(BEEP_WAIT_HANDS);
        }
    }

    // --- 3. RUNNING (BEEP TEST CORE) ---
    if (state == BEEP_RUNNING) {
        // Globaler Runden-Zähler
        if (globalLap <= totalScheduleLaps && now >= schedule[globalLap]) {
            globalLap++;
            
            // UI Update für Geschwindigkeit & Level
            int sum = 0;
            int lvl = 1;
            for (int i=0; i<21; i++) {
                sum += lapsPerLevel[i];
                if (globalLap <= sum) {
                    lvl = i + 1;
                    break;
                }
            }
            currentLevel = lvl;
            currentSpeed = 8.0f + (lvl * 0.5f);
        }
        
        int activePlayers = 0;
        for (int i=0; i<numPlayers; i++) {
            if (!players[i].isEliminated) {
                activePlayers++;
                updatePlayerVisuals(i, now);
            }
        }
        if (activePlayers == 0) setGameState(BEEP_FINISHED);
    }
}

void Game_BeepTest::updatePlayerVisuals(int i, unsigned long now) {
    BeepPlayer* p = &players[i];
    
    // Rotes Fehlerblinken zurücksetzen nach 1.5 Sekunden
    if (p->errorTime > 0 && now - p->errorTime > 1500) {
        p->errorTime = 0;
        p->visualState = -1; // Erzwingt Update der Farben
    }

    unsigned long targetTime = schedule[p->playerLap];
    
    // 1. Timeout Check (2 Sekunden nach Beep abgelaufen)
    if (now > targetTime + 2000) {
        assignYellowCard(i);
        if (!p->isEliminated) {
            p->playerLap++;
            p->currentPuckTarget = !p->currentPuckTarget; 
            p->visualState = -1;
            updatePlayerStats(i);
        }
        return; // Im nächsten Durchlauf wird die Optik neu berechnet
    } 
    
    // 2. Visuelles Feedback (Spieler läuft noch auf dieses Ziel zu)
    if (p->playerLap <= globalLap) {
        long timeToTarget = targetTime - now;
        int newVis = 0; 
        
        if (timeToTarget <= 1500 && timeToTarget >= -2000) newVis = 2; // Fenster offen
        else if (timeToTarget <= 2500 && timeToTarget > 1500) newVis = 1; // Gelbe Warnung
        else newVis = 0; // Chase Modus
        
        if (p->visualState != newVis) {
            p->visualState = newVis;
            int tMacIdx = (p->currentPuckTarget == 0) ? p->startPuckIdx : p->targetPuckIdx;
            int oMacIdx = (p->currentPuckTarget == 0) ? p->targetPuckIdx : p->startPuckIdx;
            
            if (newVis == 2) {
                setPuck(tMacIdx, EFF_DOUBLE_CHASE, p->color, 30, 255);
                delay(20); 
                CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 400;
                PuckNetwork::sendToPuck(PuckNetwork::getPucks()[tMacIdx].mac, snd);
            }
            else if (newVis == 1) {
                setPuck(tMacIdx, EFF_COUNTDOWN, CRGB::Yellow, 28, 100);
            }
            else if (newVis == 0) {
                setPuck(tMacIdx, EFF_SINGLE_CHASE, p->color, 40, 200);
            }
            
            // Alten Puck abschalten (es sei denn, er blinkt gerade rot wegen Yellow Card)
            if (p->errorTime == 0) {
                delay(10);
                setPuck(oMacIdx, EFF_STATUS, p->color, 0, 20); 
            }
        }
    } 
    // 3. Warten (Spieler war schneller als die Zeit und wartet auf das Fenster)
    else {
        if (p->visualState != 3) {
            p->visualState = 3;
            int tMacIdx = (p->currentPuckTarget == 0) ? p->startPuckIdx : p->targetPuckIdx;
            int oMacIdx = (p->currentPuckTarget == 0) ? p->targetPuckIdx : p->startPuckIdx;
            
            setPuck(tMacIdx, EFF_STATUS, p->color, 0, 50);
            if (p->errorTime == 0) {
                delay(10);
                setPuck(oMacIdx, EFF_STATUS, p->color, 0, 50);
            }
        }
    }
}

void Game_BeepTest::updatePlayerStats(int pIdx) {
    int completedLap = players[pIdx].playerLap - 1;
    if (completedLap < 1) return;
    
    int lvl = 1;
    int sum = 0;
    for (int i=0; i<21; i++) {
        sum += lapsPerLevel[i];
        if (completedLap <= sum) {
            lvl = i + 1;
            break;
        }
    }
    players[pIdx].highestLevel = lvl;
    float speed = 8.0f + (lvl * 0.5f);
    players[pIdx].vo2max = (speed * 6.55f) - 35.8f;
    if(players[pIdx].vo2max < 0) players[pIdx].vo2max = 0;
}

void Game_BeepTest::assignYellowCard(int pIdx) {
    players[pIdx].yellowCards++;
    players[pIdx].errorTime = millis();
    
    if (players[pIdx].yellowCards >= 2) {
        eliminatePlayer(pIdx, true);
    } else {
        int tMacIdx = (players[pIdx].currentPuckTarget == 0) ? players[pIdx].startPuckIdx : players[pIdx].targetPuckIdx;
        setPuck(tMacIdx, EFF_FAIL, CRGB::Red, 0, 255);
        delay(20); 
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_ERROR;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[tMacIdx].mac, snd);
    }
}

void Game_BeepTest::eliminatePlayer(int pIdx, bool sendSound) {
    players[pIdx].isEliminated = true;
    setPuck(players[pIdx].startPuckIdx, EFF_STATIC, CRGB::Red, 0, 20);
    setPuck(players[pIdx].targetPuckIdx, EFF_STATIC, CRGB::Red, 0, 20);
    
    if (sendSound) {
        delay(20); 
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_EXPLOSION;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[players[pIdx].targetPuckIdx].mac, snd);
    }
}

void Game_BeepTest::triggerFalseStart(int pIdx) {
    players[pIdx].isHolding = false;
    players[pIdx].falseStart = true;
    state = BEEP_FALSE_START;
    stateStartTime = millis();
    
    setPuck(players[pIdx].startPuckIdx, EFF_POLICE, CRGB::Red, 0, 255);
    setPuck(players[pIdx].targetPuckIdx, EFF_POLICE, CRGB::Red, 0, 255);
    
    delay(20); 
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); 
    snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_ERROR;
    PuckNetwork::broadcast(snd); 
}

void Game_BeepTest::handleEvent(int puckIndex, EventPacket event) {
    if (event.type != EVT_BTN_CLICK && event.type != EVT_BTN_RELEASE) return;
    
    int role = 0;
    int pIdx = getPlayerIndex(puckIndex, role);
    if (pIdx == -1) return;
    
    BeepPlayer* p = &players[pIdx];

    if (state == BEEP_WAIT_HANDS || state == BEEP_COUNTDOWN) {
        if (role == 0) { 
            if (event.type == EVT_BTN_CLICK) {
                p->isHolding = true;
                setPuck(puckIndex, EFF_DOUBLE_CHASE, p->color, 20, 150);
            } else if (event.type == EVT_BTN_RELEASE) {
                p->isHolding = false;
                if (state == BEEP_WAIT_HANDS) setPuck(puckIndex, EFF_SINGLE_CHASE, p->color, 40, 150);
                else if (state == BEEP_COUNTDOWN) triggerFalseStart(pIdx);
            }
        }
        return;
    }

    if (state == BEEP_RUNNING && event.type == EVT_BTN_CLICK) {
        if (p->isEliminated) return;
        
        if (role == p->currentPuckTarget) {
            
            // Verhindert mehrfaches Klicken, wenn man bereits auf das nächste Fenster wartet
            if (p->playerLap > globalLap) return; 

            unsigned long now = millis();
            unsigned long targetTime = schedule[p->playerLap];
            bool early = false;
            
            if (now < targetTime - 1500) early = true; 
            
            if (early) {
                assignYellowCard(pIdx);
            } else {
                CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 80;
                PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
                delay(15); 
            }
            
            if (!p->isEliminated) {
                p->playerLap++;
                p->currentPuckTarget = !p->currentPuckTarget; 
                p->visualState = -1; 
                updatePlayerStats(pIdx);
                updatePlayerVisuals(pIdx, now);
            }
        }
    }
}

int Game_BeepTest::getPlayerIndex(int puckIndex, int& puckRole) {
    for (int i=0; i<numPlayers; i++) {
        if (players[i].startPuckIdx == puckIndex) {
            puckRole = 0;
            return i;
        }
        if (players[i].targetPuckIdx == puckIndex) {
            puckRole = 1;
            return i;
        }
    }
    return -1;
}

void Game_BeepTest::setPuck(int index, int effect, CRGB color, int speed, int bright) {
    if(index < 0 || index >= MAX_PEERS) return;
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, cp);
}

String Game_BeepTest::getStatusJSON() {
    String json = "{";
    json += "\"st\":" + String(state) + ",";
    
    long t = 0;
    if (state == BEEP_RUNNING) t = millis() - testStartTime;
    else if (state == BEEP_FINISHED) t = finishTime - testStartTime;
    if (t < 0) t = 0;
    
    json += "\"tT\":" + String(t) + ",";
    json += "\"spd\":" + String(currentSpeed) + ",";
    json += "\"lvl\":" + String(currentLevel) + ",";
    
    json += "\"players\":[";
    for(int i=0; i<numPlayers; i++) {
        if(i>0) json += ",";
        json += "{";
        json += "\"id\":" + String(players[i].id) + ",";
        json += "\"elim\":" + String(players[i].isEliminated ? 1 : 0) + ",";
        json += "\"hld\":" + String(players[i].isHolding ? 1 : 0) + ",";
        json += "\"fs\":" + String(players[i].falseStart ? 1 : 0) + ",";
        json += "\"yc\":" + String(players[i].yellowCards) + ",";
        
        int dist = (players[i].playerLap - 1) * 20;
        if (dist < 0) dist = 0;
        
        json += "\"dist\":" + String(dist) + ",";
        json += "\"vo2\":" + String(players[i].vo2max, 1);
        json += "}";
    }
    json += "]}";
    return json;
}