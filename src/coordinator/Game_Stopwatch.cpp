#include "Game_Stopwatch.h"
#include "PuckNetwork.h"

static unsigned long flashResetTime[MAX_PEERS] = {0}; 

void Game_Stopwatch::setup() {
    Serial.println("GAME: Setup Stopwatch");
    gameState = SW_SETUP;
}

void Game_Stopwatch::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "cfg_num") { numPlayers = constrain(value, 1, MAX_PEERS); initGame(); }
    else if (cmd == "cfg_central") centralStart = (value == 1);
    else if (cmd == "cfg_hold") holdToStart = (value == 1);
    else if (cmd == "cfg_lap") lapMode = (value == 1);
    
    else if (cmd == "start_all") startSequence();
    else if (cmd == "stop_all") stopAll();
    else if (cmd == "reset") resetAll();
    
    else if (cmd == "stop_one") { 
        if(value >= 0 && value < numPlayers) {
            SW_Player* p = &players[value];
            if(p->state == PL_RUNNING) {
                p->state = PL_FINISHED;
                p->finishTime = millis() - p->startTime;
                setPuck(value, EFF_BREATHE_MOD4, PLAYER_COLORS[value % 10], 50);
            }
        }
    }
    else if (cmd == "exit") {
        gameState = SW_SETUP;
        CommandPacket cp; 
        memset(&cp, 0, sizeof(cp));
        cp.cmd=CMD_EFFECT; cp.effectID=EFF_STATUS; cp.r=0; cp.g=255; cp.b=0; cp.extra=255; 
        PuckNetwork::broadcast(cp);
    }
}

void Game_Stopwatch::initGame() {
    actionQueue.clear(); 
    allReadyTime = 0; 
    
    for(int i=0; i<MAX_PEERS; i++) {
        players[i].state = PL_IDLE;
        players[i].startTime = 0;
        players[i].finishTime = 0;
        players[i].laps.clear();
        players[i].isHolding = false;
        players[i].falseStart = false;
        flashResetTime[i] = 0;
        
        // Kein blockierendes delay() mehr, wir planen das Senden in die Zukunft!
        if (i < numPlayers) {
            queueLight(i, EFF_BREATHE_MOD4, PLAYER_COLORS[i % 10], 50, 80, i * 15);
        } else {
            queueLight(i, EFF_OFF, CRGB::Black, 0, 0, i * 15);
        }
    }
    
    if(centralStart) gameState = SW_SETUP;
    else gameState = SW_RUNNING; 
}

void Game_Stopwatch::startSequence() {
    if (!centralStart) return; 
    
    for(int i=0; i<numPlayers; i++) {
        players[i].falseStart = false;
        players[i].isHolding = false;
        players[i].state = PL_IDLE;
    }

    if (holdToStart) {
        gameState = SW_WAIT_HANDS;
        allReadyTime = 0;
        for(int i=0; i<numPlayers; i++) {
            // Non-blocking queuing mit 15ms Abstand
            queueLight(i, EFF_SINGLE_CHASE, PLAYER_COLORS[i % 10], 40, 100, i * 15);
        }
    } else {
        gameState = SW_COUNTDOWN;
        stateStartTime = millis();
        CommandPacket snd; memset(&snd, 0, sizeof(snd));
        snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
        PuckNetwork::broadcast(snd);
    }
}

void Game_Stopwatch::loop() {
    unsigned long now = millis();
    processActions(); 

    // --- FLASH RESET ---
    for(int i=0; i<MAX_PEERS; i++) {
        if (flashResetTime[i] > 0 && now >= flashResetTime[i]) {
            flashResetTime[i] = 0;
            if (players[i].state == PL_RUNNING) {
                setPuck(i, EFF_SINGLE_CHASE, PLAYER_COLORS[i % 10], 28, 150);
            } else if (players[i].state == PL_FINISHED || players[i].state == PL_IDLE) {
                setPuck(i, EFF_BREATHE_MOD4, PLAYER_COLORS[i % 10], 50, 80);
            }
        }
    }

    // --- HOLD TO START LOGIK ---
    if (gameState == SW_WAIT_HANDS) {
        bool allReady = true;
        for(int i=0; i<numPlayers; i++) if(!players[i].isHolding) allReady = false;
        
        if(allReady) {
            if(allReadyTime == 0) allReadyTime = now;
            if(now - allReadyTime > 1000) {
                gameState = SW_COUNTDOWN;
                stateStartTime = now;
                CommandPacket snd; memset(&snd, 0, sizeof(snd));
                snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
                PuckNetwork::broadcast(snd);
            }
        } else {
            allReadyTime = 0; 
        }
    }
    
    // --- COUNTDOWN ---
    else if (gameState == SW_COUNTDOWN) {
        if (now - stateStartTime > 3000) { 
            gameState = SW_RUNNING;
            unsigned long commonStart = now;
            
            for(int i=0; i<numPlayers; i++) {
                players[i].state = PL_RUNNING;
                players[i].startTime = commonStart;
                players[i].laps.clear();
                // Kein delay(10) mehr, sondern non-blocking queue
                queueLight(i, EFF_SINGLE_CHASE, PLAYER_COLORS[i % 10], 28, 150, i * 15);
            }
        }
    }
    
    // --- FALSE START RESET ---
    else if (gameState == SW_FALSE_START) {
        if (now - stateStartTime > 2000) {
            startSequence(); 
        }
    }
}

void Game_Stopwatch::handleEvent(int pIdx, EventPacket event) {
    if (pIdx >= numPlayers) return;
    SW_Player* p = &players[pIdx];
    unsigned long now = millis();

    if (gameState == SW_WAIT_HANDS || gameState == SW_COUNTDOWN) {
        if (event.type == EVT_BTN_CLICK) { 
            p->isHolding = true;
            setPuck(pIdx, EFF_DOUBLE_CHASE, PLAYER_COLORS[pIdx % 10], 15, 150);
        }
        else if (event.type == EVT_BTN_RELEASE) {
            p->isHolding = false;
            if (gameState == SW_WAIT_HANDS) {
                setPuck(pIdx, EFF_SINGLE_CHASE, PLAYER_COLORS[pIdx % 10], 40, 100);
            } else if (gameState == SW_COUNTDOWN) {
                triggerFalseStart(pIdx);
            }
        }
        return;
    }

    bool canInteract = (centralStart && gameState == SW_RUNNING) || (!centralStart);
    if (!canInteract) return;

    if (!centralStart && p->state == PL_IDLE && event.type == EVT_BTN_CLICK) {
        p->state = PL_RUNNING;
        p->startTime = now;
        p->laps.clear();
        setPuck(pIdx, EFF_SINGLE_CHASE, PLAYER_COLORS[pIdx % 10], 28, 150);
        queueAction(pIdx, 1, 600, 0, 0); 
        return;
    }

    if (p->state == PL_RUNNING && event.type == EVT_BTN_CLICK) {
        unsigned long currentTime = now - p->startTime;
        if (!lapMode) {
            p->state = PL_FINISHED;
            p->finishTime = currentTime;
            setPuck(pIdx, EFF_BREATHE_MOD4, PLAYER_COLORS[pIdx % 10], 50);
            queueAction(pIdx, 1, 400, 0, 0); 
        } else {
            bool isDouble = false;
            if (p->laps.size() > 0) {
                if (currentTime - p->laps.back() < 1000) isDouble = true;
            }
            if (isDouble) {
                p->finishTime = p->laps.back(); 
                p->laps.pop_back(); 
                p->state = PL_FINISHED;
                setPuck(pIdx, EFF_BREATHE_MOD4, PLAYER_COLORS[pIdx % 10], 50);
                queueAction(pIdx, 1, 800, 0, 0); 
            } else {
                p->laps.push_back(currentTime);
                setPuck(pIdx, EFF_FLASH, CRGB::White, 50, 200); 
                flashResetTime[pIdx] = now + 150; 
                queueAction(pIdx, 1, 100, 0, 0); 
            }
        }
    }
    else if (!centralStart && p->state == PL_FINISHED && event.type == EVT_BTN_CLICK) {
        p->state = PL_RUNNING;
        p->startTime = now;
        p->finishTime = 0;
        p->laps.clear();
        setPuck(pIdx, EFF_SINGLE_CHASE, PLAYER_COLORS[pIdx % 10], 28, 150);
        queueAction(pIdx, 1, 600, 0, 0);
    }
}

void Game_Stopwatch::triggerFalseStart(int pIdx) {
    gameState = SW_FALSE_START;
    stateStartTime = millis();
    
    for(int i=0; i<numPlayers; i++) {
        players[i].isHolding = false;
        if (i == pIdx) {
            players[i].falseStart = true;
            queueLight(i, EFF_BLINK, CRGB::Red, 100, 255, i * 15); 
        } else {
            players[i].falseStart = false;
            queueLight(i, EFF_STATIC, CRGB::Red, 0, 50, i * 15);
        }
    }
    CommandPacket snd; memset(&snd, 0, sizeof(snd));
    snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_ERROR;
    PuckNetwork::broadcast(snd);
}

void Game_Stopwatch::stopAll() {
    gameState = SW_FINISHED;
    int delayOffset = 0;
    for(int i=0; i<numPlayers; i++) {
        if(players[i].state == PL_RUNNING) {
            players[i].state = PL_FINISHED;
            players[i].finishTime = millis() - players[i].startTime;
            queueLight(i, EFF_BREATHE_MOD4, PLAYER_COLORS[i % 10], 50, 255, delayOffset);
            delayOffset += 15;
        }
    }
}

void Game_Stopwatch::resetAll() {
    initGame();
}

// Queue für Sound Commands
void Game_Stopwatch::queueAction(int pIdx, int type, int v1, int v2, int delayMs) {
    SW_Action a; 
    a.triggerTime = millis() + delayMs; 
    a.puckIdx = pIdx;
    a.type = type; 
    a.val1 = v1; 
    a.val2 = v2; 
    actionQueue.push_back(a);
}

// NEU: Queue für Licht Commands (verhindert Delay/Blockieren)
void Game_Stopwatch::queueLight(int pIdx, int effect, CRGB color, int speed, int bright, int delayMs) {
    SW_Action a; 
    a.triggerTime = millis() + delayMs; 
    a.puckIdx = pIdx;
    a.type = 0; // Type 0 = Light
    a.effect = effect;
    a.color = color;
    a.speed = speed;
    a.bright = bright;
    actionQueue.push_back(a);
}

void Game_Stopwatch::processActions() {
    unsigned long now = millis();
    auto it = actionQueue.begin();
    while (it != actionQueue.end()) {
        if (now >= it->triggerTime) {
            if (it->type == 0) {
                // Sende Licht (mit queueLight geplant)
                setPuck(it->puckIdx, it->effect, it->color, it->speed, it->bright);
            } else {
                // Sende Sound (mit queueAction geplant)
                CommandPacket snd; 
                memset(&snd, 0, sizeof(snd)); 
                snd.cmd = CMD_SOUND; 
                snd.duration = it->val1; 
                PuckNetwork::sendToPuck(PuckNetwork::getPucks()[it->puckIdx].mac, snd); 
            }
            it = actionQueue.erase(it);
        } else {
            ++it;
        }
    }
}

void Game_Stopwatch::setPuck(int index, int effect, CRGB color, int speed, int bright) {
    if(index < 0 || index >= MAX_PEERS) return;
    PuckInfo* p = PuckNetwork::getPucks();
    if(!p[index].active) return;
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(p[index].mac, cp);
}

String Game_Stopwatch::getStatusJSON() {
    String json = "{";
    json += "\"state\":" + String(gameState) + ",";
    json += "\"central\":" + String(centralStart ? 1 : 0) + ",";
    json += "\"lap\":" + String(lapMode ? 1 : 0) + ",";
    json += "\"players\":[";
    for(int i=0; i<numPlayers; i++) {
        if(i>0) json += ",";
        json += "{";
        json += "\"id\":" + String(i) + ",";
        json += "\"st\":" + String(players[i].state) + ",";
        json += "\"hld\":" + String(players[i].isHolding ? 1 : 0) + ",";
        json += "\"fs\":" + String(players[i].falseStart ? 1 : 0) + ",";
        
        long t = 0;
        if(players[i].state == PL_RUNNING) t = millis() - players[i].startTime;
        else if(players[i].state == PL_FINISHED) t = players[i].finishTime;
        json += "\"t\":" + String(t);
        
        if (lapMode && !players[i].laps.empty()) {
            json += ",\"laps\":[";
            int count = players[i].laps.size();
            for(int k=0; k<count; k++) {
                if(k > 0) json += ",";
                json += String(players[i].laps[k]);
            }
            json += "]";
        }
        json += "}";
    }
    json += "]}";
    return json;
}