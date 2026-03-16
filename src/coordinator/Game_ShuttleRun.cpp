#include "Game_ShuttleRun.h"
#include "PuckNetwork.h"

void Game_ShuttleRun::setup() {
    Serial.println("GAME: Setup Shuttle Run");
    state = SR_SETUP;
    winnerTime = 0;
    finishTime = 0;
}

void Game_ShuttleRun::processCommand(String cmd, int value) {
    if (cmd == "setup") {
        resetGame();
        for(int p=0; p<playerCount; p++) {
            setPuck(getPuckIndex(p, 0), EFF_SINGLE_CHASE, PLAYER_COLORS[p], 50, 100);
            delay(15); 
            setPuck(getPuckIndex(p, 1), EFF_BREATHE_MOD2, PLAYER_COLORS[p], 50, 30); 
            delay(15); 
        }
    }
    else if (cmd == "config_players") playerCount = value;
    else if (cmd == "config_rounds") maxRounds = value;
    else if (cmd == "start") startSequence(true);
    else if (cmd == "stop") stopGame();
    else if (cmd == "reset") resetGame();
    else if (cmd == "exit") exitGame();
}

void Game_ShuttleRun::startSequence(bool forceRestart) {
    if (!forceRestart && state != SR_SETUP && state != SR_FALSE_START) return;
    
    Serial.println("GAME: Wait for Hands");
    state = SR_WAIT_HANDS;
    winnerTime = 0; 
    finishTime = 0;
    
    for(int p=0; p<playerCount; p++) {
        isHolding[p] = false;
        activeTargetIdx[p] = 1;
        currentRound[p] = 0;
        hasFinished[p] = false;
        finishTimes[p] = 0; 
        isFlashing[p] = false;
        lastHitTime[p] = 0;
        
        setPuck(getPuckIndex(p, 0), EFF_BLINK, PLAYER_COLORS[p], 200, 100);
        delay(15); 
        setPuck(getPuckIndex(p, 1), EFF_STATUS, PLAYER_COLORS[p], 0, 50);
        delay(15);
    }
    
    // Kurzes Signal an alle Startpucks
    for(int p=0; p<playerCount; p++) {
        CommandPacket snd; memset(&snd, 0, sizeof(snd));
        snd.cmd = CMD_SOUND; snd.duration = 200;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[getPuckIndex(p, 0)].mac, snd);
        delay(20); 
    }
}

void Game_ShuttleRun::loop() {
    unsigned long now = millis();

    // 1. WARTEN AUF HÄNDE
    if (state == SR_WAIT_HANDS) {
        bool allReady = true;
        for(int p=0; p<playerCount; p++) {
            if (!isHolding[p]) allReady = false;
        }
        
        if (allReady) {
            state = SR_COUNTDOWN;
            stateStartTime = now;
            Serial.println("GAME: Countdown started");
            
            // FIX: SEQ_SKI auf Start-Pucks (wo die Spieler stehen)
            for(int p=0; p<playerCount; p++) {
                CommandPacket snd; memset(&snd, 0, sizeof(snd));
                snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
                PuckNetwork::sendToPuck(PuckNetwork::getPucks()[getPuckIndex(p, 0)].mac, snd);
                delay(15);
            }
        }
    }
    
    // 2. COUNTDOWN (SEQ_SKI dauert 3.8s)
    else if (state == SR_COUNTDOWN) {
        if (now - stateStartTime >= 3100) {
            // GO!
            state = SR_RUNNING;
            gameStartTime = now;
            
            for(int p=0; p<playerCount; p++) {
                setPuck(getPuckIndex(p, 0), EFF_STATUS, PLAYER_COLORS[p], 0, 50);
                delay(15);
                setPuck(getPuckIndex(p, 1), EFF_DOUBLE_CHASE, PLAYER_COLORS[p], 40, 200);
                delay(15);
            }
            Serial.println("GAME: GO!");
        }
    }
    
    // 3. FEHLSTART
    else if (state == SR_FALSE_START) {
        if (now - stateStartTime > 2000) {
            startSequence(true); 
        }
    }

    // 4. RUNNING (Flash Reset)
    else if (state == SR_RUNNING) {
        for(int i=0; i<MAX_PEERS; i++) {
            if (isFlashing[i] && (now - flashStartTime[i] > 150)) {
                isFlashing[i] = false;
                int pIdx = i / 2;
                if(pIdx < playerCount) {
                    setPuck(i, EFF_STATUS, PLAYER_COLORS[pIdx], 0, 50);
                }
            }
        }
    }
}

void Game_ShuttleRun::handleEvent(int puckIndex, EventPacket event) {
    int playerIdx = puckIndex / 2;
    int puckRole = puckIndex % 2; 
    
    if (playerIdx >= playerCount) return; 

    // --- SETUP PHASE ---
    if (state == SR_WAIT_HANDS || state == SR_COUNTDOWN) {
        if (puckRole != 0) return;
        
        if (event.type == EVT_BTN_CLICK) {
            isHolding[playerIdx] = true;
            setPuck(puckIndex, EFF_DOUBLE_CHASE, PLAYER_COLORS[playerIdx], 30, 100);
        }
        else if (event.type == EVT_BTN_RELEASE) {
            isHolding[playerIdx] = false;
            
            if (state == SR_WAIT_HANDS) {
                setPuck(puckIndex, EFF_BLINK, PLAYER_COLORS[playerIdx], 200, 100);
            }
            else if (state == SR_COUNTDOWN) {
                triggerFalseStart(playerIdx);
            }
        }
        return;
    }

    // --- RUNNING PHASE ---
    if (state == SR_RUNNING) {
        if (hasFinished[playerIdx]) return;
        if (event.type != EVT_BTN_CLICK) return; 

        // NEU: Debouncing
        if (millis() - lastHitTime[playerIdx] < 200) return;
        lastHitTime[playerIdx] = millis();

        if (puckRole == activeTargetIdx[playerIdx]) {
            // TREFFER! 
            
            currentRound[playerIdx]++;
            CommandPacket snd; memset(&snd, 0, sizeof(snd));
            snd.cmd = CMD_SOUND; snd.duration = 100;
            PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
            
            if (currentRound[playerIdx] >= maxRounds) {
                // --- ZIEL ERREICHT (FINISH) ---
                hasFinished[playerIdx] = true;
                finishTimes[playerIdx] = millis() - gameStartTime;
                if (winnerTime == 0) winnerTime = finishTimes[playerIdx];

                int effect = (finishTimes[playerIdx] == winnerTime) ? EFF_RAINBOW : EFF_SPARKLE;
                
                setPuck(getPuckIndex(playerIdx, 0), effect, PLAYER_COLORS[playerIdx], 0, 200);
                delay(50);
                setPuck(getPuckIndex(playerIdx, 0), effect, PLAYER_COLORS[playerIdx], 0, 200); 
                delay(50);

                setPuck(getPuckIndex(playerIdx, 1), effect, PLAYER_COLORS[playerIdx], 0, 200);
                delay(50);
                setPuck(getPuckIndex(playerIdx, 1), effect, PLAYER_COLORS[playerIdx], 0, 200); 
                
                bool allDone = true;
                for(int p=0; p<playerCount; p++) if(!hasFinished[p]) allDone = false;
                if(allDone) stopGame();

            } else {
                // --- NORMALE RUNDE ---
                isFlashing[puckIndex] = true;
                flashStartTime[puckIndex] = millis();
                setPuck(puckIndex, EFF_FLASH, PLAYER_COLORS[playerIdx], 0, 255);

                activeTargetIdx[playerIdx] = (activeTargetIdx[playerIdx] == 1) ? 0 : 1;
                int nextPuck = getPuckIndex(playerIdx, activeTargetIdx[playerIdx]);
                setPuck(nextPuck, EFF_DOUBLE_CHASE, PLAYER_COLORS[playerIdx], 40, 200);
            }
        } else {
            // FALSCHER PUCK
            setPuck(puckIndex, EFF_FAIL, CRGB::Red, 0, 255);
            isFlashing[puckIndex] = true; 
            flashStartTime[puckIndex] = millis() + 350; 
            
            CommandPacket snd; memset(&snd, 0, sizeof(snd));
            snd.cmd = CMD_SOUND; snd.duration = 50;
            uint8_t* mac = PuckNetwork::getPucks()[puckIndex].mac;
            PuckNetwork::sendToPuck(mac, snd); delay(80);
            PuckNetwork::sendToPuck(mac, snd); delay(80);
            PuckNetwork::sendToPuck(mac, snd);
        }
    }
}

void Game_ShuttleRun::triggerFalseStart(int playerIndex) {
    state = SR_FALSE_START;
    stateStartTime = millis();
    falseStartPlayer = playerIndex;
    Serial.println("GAME: FALSE START!");
    
    // Stoppe alle Countdown-Sequenzen und setze den Status
    for(int p=0; p<playerCount; p++) {
        isHolding[p] = false;
        int startPuckIdx = getPuckIndex(p, 0);

        // FIX: Sende Befehl zum Stoppen der Sound-Sequenz an jeden Puck
        CommandPacket stopSeq; memset(&stopSeq, 0, sizeof(stopSeq));
        stopSeq.cmd = CMD_SEQUENCE;
        stopSeq.extra = SEQ_OFF;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[startPuckIdx].mac, stopSeq);
        delay(15);

        if (p == playerIndex) {
            // Verursacher
            setPuck(startPuckIdx, EFF_POLICE, CRGB::Red, 0, 255);
        } else {
            // Die anderen
            setPuck(startPuckIdx, EFF_STATUS, PLAYER_COLORS[p], 0, 50);
        }
        delay(15);
    }
    
    // Sound nur für Verursacher
    int startPuck = getPuckIndex(playerIndex, 0);
    CommandPacket snd; memset(&snd, 0, sizeof(snd));
    snd.cmd = CMD_SOUND; snd.duration = 50;
    uint8_t* mac = PuckNetwork::getPucks()[startPuck].mac;
    PuckNetwork::sendToPuck(mac, snd); delay(100);
    PuckNetwork::sendToPuck(mac, snd); delay(100);
    PuckNetwork::sendToPuck(mac, snd);
}

void Game_ShuttleRun::stopGame() {
    state = SR_FINISHED;
    if(finishTime == 0) finishTime = millis();
}

void Game_ShuttleRun::resetGame() {
    setup(); 
}

void Game_ShuttleRun::exitGame() {
    state = SR_SETUP;
    CommandPacket light; memset(&light, 0, sizeof(light));
    light.cmd = CMD_EFFECT; light.effectID = EFF_STATUS; 
    light.r = 0; light.g = 255; light.b = 0; 
    light.extra = 255; light.duration = 0; 
    PuckNetwork::broadcast(light);
}

void Game_ShuttleRun::setPuck(int puckIndex, int effect, CRGB color, int speed, int bright) {
    if(puckIndex < 0 || puckIndex >= MAX_PEERS) return;
    PuckInfo* p = PuckNetwork::getPucks();
    if(!p[puckIndex].active) return;
    
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(p[puckIndex].mac, cp);
}

int Game_ShuttleRun::getPuckIndex(int player, int target) {
    return (player * 2) + target;
}

String Game_ShuttleRun::getStatusJSON() {
    String json = "{";
    json += "\"state\":" + String(state) + ",";
    
    long elapsed = 0;
    if (state == SR_RUNNING) elapsed = millis() - gameStartTime;
    else if (state == SR_FINISHED) elapsed = finishTime - gameStartTime;
    json += "\"time\":" + String(elapsed) + ",";
    
    json += "\"players\":[";
    for(int p=0; p<playerCount; p++) {
        if(p>0) json += ",";
        json += "{";
        json += "\"round\":" + String(currentRound[p]) + ",";
        json += "\"max\":" + String(maxRounds) + ",";
        json += "\"holding\":" + String(isHolding[p] ? 1 : 0) + ",";
        json += "\"finished\":" + String(hasFinished[p] ? 1 : 0) + ",";
        json += "\"time\":" + String(finishTimes[p]);
        json += "}";
    }
    json += "],";
    json += "\"fs\":" + String(falseStartPlayer) + ",";
    json += "\"winnerTime\":" + String(winnerTime);
    json += "}";
    return json;
}