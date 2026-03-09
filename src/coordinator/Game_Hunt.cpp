#include "Game_Hunt.h"
#include "PuckNetwork.h"

void Game_Hunt::setup() {
    Serial.println("GAME: Setup Hunt!");
    initGame();
}

void Game_Hunt::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "show_colors") showColors();
    else if (cmd == "cfg_players") numPlayers = constrain(value, 1, 10);
    else if (cmd == "cfg_pucks") numPucks = constrain(value, 3, MAX_PEERS);
    else if (cmd == "cfg_time") gameDuration = value * 1000UL;
    else if (cmd == "cfg_radius") radius = constrain(value, 2, 10);
    else if (cmd == "cfg_diff") difficulty = constrain(value, 1, 5);
    else if (cmd == "cfg_sound") soundOn = (value == 1);
    
    else if (cmd == "start") {
        resetRound();
        startGameSequence();
    }
    else if (cmd == "stop") {
        gameState = HUNT_FINISHED;
        stateStartTime = millis();
    }
    else if (cmd == "reset") initGame(); 
    else if (cmd == "exit") {
        CommandPacket cp; memset(&cp, 0, sizeof(cp));
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_STATUS; cp.r = 0; cp.g = 255; cp.b = 0; cp.extra = 255;
        PuckNetwork::broadcast(cp);
        gameState = HUNT_SETUP;
    }
}

void Game_Hunt::initGame() {
    for(int i=0; i<numPlayers; i++) {
        players[i].id = i;
        players[i].color = PLAYER_COLORS[i % 10];
        players[i].score = 0;
        players[i].currentPuckIdx = -1;
        players[i].lastPuckIdx = -1;
        players[i].penaltyEndTime = 0;
    }

    PuckInfo* netPucks = PuckNetwork::getPucks();
    actualPuckCount = 0;
    
    for(int i=0; i<MAX_PEERS; i++) {
        if(netPucks[i].active && actualPuckCount < numPucks) {
            activePucks[actualPuckCount].globalIdx = i;
            activePucks[actualPuckCount].state = P_NEUTRAL;
            activePucks[actualPuckCount].assignedPlayerId = -1;
            activePucks[actualPuckCount].stateEndTime = 0;
            activePucks[actualPuckCount].warningSent = false;
            actualPuckCount++;
        }
    }
    
    gameState = HUNT_SETUP;
}

void Game_Hunt::showColors() {
    PuckInfo* netPucks = PuckNetwork::getPucks();
    
    for(int i=0; i<MAX_PEERS; i++) {
        if (netPucks[i].active) {
            bool isUsed = false;
            for(int j=0; j<actualPuckCount; j++) {
                if (activePucks[j].globalIdx == i) {
                    CRGB c = players[j % numPlayers].color;
                    setPuck(i, EFF_BREATHE_MOD4, c, 50, 85);
                    isUsed = true;
                    break;
                }
            }
            if (!isUsed) setPuck(i, EFF_OFF, CRGB::Black, 0, 0);
            delay(15);
        }
    }
}

void Game_Hunt::resetRound() {
    for(int i=0; i<numPlayers; i++) {
        players[i].score = 0;
        players[i].currentPuckIdx = -1;
        players[i].lastPuckIdx = -1;
        players[i].penaltyEndTime = 0;
    }
    for(int i=0; i<actualPuckCount; i++) {
        activePucks[i].state = P_NEUTRAL;
        activePucks[i].assignedPlayerId = -1;
        activePucks[i].stateEndTime = 0;
        activePucks[i].warningSent = false;
    }
}

unsigned long Game_Hunt::calculateTimeout() {
    if (difficulty == 1) return 0; // Unendlich
    
    unsigned long baseTime = 0;
    if (difficulty == 2) baseTime = 5000;
    else if (difficulty == 3) baseTime = 4000;
    else if (difficulty == 4) baseTime = 2000;
    else if (difficulty == 5) baseTime = 1000;

    float multiplier = 1.0f + ((radius - 2.0f) / 8.0f); 
    return (unsigned long)(baseTime * multiplier);
}

void Game_Hunt::startGameSequence() {
    gameState = HUNT_COUNTDOWN;
    stateStartTime = millis();
    
    for(int i=0; i<actualPuckCount; i++) {
        setPuck(activePucks[i].globalIdx, EFF_BLINK, CRGB::Purple, 200, 200);
        delay(10);
    }
    if (soundOn) {
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
        PuckNetwork::broadcast(snd);
    }
}

void Game_Hunt::loop() {
    unsigned long now = millis();

    if (gameState == HUNT_COUNTDOWN) {
        if (now - stateStartTime > 3000) {
            gameState = HUNT_RUNNING;
            stateStartTime = now;
            lastWarningBeep = 0;
            
            for(int i=0; i<actualPuckCount; i++) {
                activePucks[i].state = P_NEUTRAL;
                activePucks[i].assignedPlayerId = -1;
                activePucks[i].warningSent = false;
                setPuck(activePucks[i].globalIdx, EFF_BREATHE_MOD4, CRGB::White, 50, 85);
                delay(10);
            }
            assignPucksFromQueue();
        }
    }
    else if (gameState == HUNT_RUNNING) {
        unsigned long elapsed = now - stateStartTime;
        long timeLeft = gameDuration - elapsed;

        if (timeLeft <= 0) {
            gameState = HUNT_FINISHED;
            stateStartTime = now;
            
            for(int i=0; i<actualPuckCount; i++) {
                setPuck(activePucks[i].globalIdx, EFF_BLINK, CRGB::Green, 150, 255);
            }
            if (soundOn) {
                CommandPacket snd; memset(&snd,0,sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 750;
                PuckNetwork::broadcast(snd);
            }
            return;
        }

        if (soundOn) {
            if (timeLeft <= 10000 && timeLeft > 5000) {
                if (now - lastWarningBeep >= 1000) {
                    CommandPacket snd; memset(&snd,0,sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 50;
                    PuckNetwork::broadcast(snd);
                    lastWarningBeep = now;
                }
            } else if (timeLeft <= 5000) {
                if (now - lastWarningBeep >= 500) {
                    CommandPacket snd; memset(&snd,0,sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = 50;
                    PuckNetwork::broadcast(snd);
                    lastWarningBeep = now;
                }
            }
        }

        bool needAssignment = false;
        for (int i=0; i<actualPuckCount; i++) {
            HuntPuck* p = &activePucks[i];
            
            if (p->state == P_ACTIVE && difficulty > 1 && p->stateEndTime > 0) {
                // Timeout abgelaufen -> Strafe
                if (now > p->stateEndTime) {
                    players[p->assignedPlayerId].currentPuckIdx = -1;
                    players[p->assignedPlayerId].penaltyEndTime = now + 1000; 
                    
                    p->state = P_NEUTRAL;
                    p->assignedPlayerId = -1;
                    p->warningSent = false;
                    setPuck(p->globalIdx, EFF_BREATHE_MOD4, CRGB::White, 50, 85);
                    needAssignment = true;
                }
                // NEU: Warnung abfeuern (genau 1000ms vor Ablauf)
                else if (!p->warningSent && (p->stateEndTime - now) <= 1000) {
                    p->warningSent = true;
                    // EFF_COUNTDOWN mit Speed 28 (ca. 1 Sekunde für 35 LEDs)
                    setPuck(p->globalIdx, EFF_COUNTDOWN, players[p->assignedPlayerId].color, 28, 255);
                }
            }
            // Error Ablauf Check
            else if (p->state == P_ERROR && now > p->stateEndTime) {
                p->state = P_NEUTRAL;
                p->assignedPlayerId = -1;
                p->warningSent = false;
                setPuck(p->globalIdx, EFF_BREATHE_MOD4, CRGB::White, 50, 85);
                needAssignment = true;
            }
        }
        
        assignPucksFromQueue();
    }
    else if (gameState == HUNT_FINISHED) {
        if (now - stateStartTime > 4000) {
            evaluateWinners();
        }
    }
    else if (gameState == HUNT_WINNER) {
        if (now - stateStartTime > 10000) {
            gameState = HUNT_IDLE;
            for(int i=0; i<actualPuckCount; i++) {
                setPuck(activePucks[i].globalIdx, EFF_BREATHE_MOD4, CRGB::White, 50, 85);
            }
        }
    }
}

void Game_Hunt::assignPucksFromQueue() {
    unsigned long now = millis();
    
    for (int i=0; i<numPlayers; i++) {
        if (players[i].currentPuckIdx == -1 && now >= players[i].penaltyEndTime) {
            
            std::vector<int> validPucks;
            for (int j=0; j<actualPuckCount; j++) {
                if (activePucks[j].state == P_NEUTRAL) {
                    if (j != players[i].lastPuckIdx || actualPuckCount <= 1) {
                        validPucks.push_back(j);
                    }
                }
            }
            
            if (validPucks.empty()) {
                for (int j=0; j<actualPuckCount; j++) {
                    if (activePucks[j].state == P_NEUTRAL) validPucks.push_back(j);
                }
            }
            
            if (validPucks.size() > 0) {
                int randIdx = random(validPucks.size());
                int pIdx = validPucks[randIdx];
                
                activePucks[pIdx].state = P_ACTIVE;
                activePucks[pIdx].assignedPlayerId = i;
                activePucks[pIdx].warningSent = false;
                
                unsigned long tOut = calculateTimeout();
                if (tOut > 0) activePucks[pIdx].stateEndTime = now + tOut;
                else activePucks[pIdx].stateEndTime = 0;
                
                players[i].currentPuckIdx = pIdx;
                
                setPuck(activePucks[pIdx].globalIdx, EFF_STATIC, players[i].color, 0, 255);
            }
        }
    }
}

void Game_Hunt::handleEvent(int globalPuckIdx, EventPacket event) {
    if (gameState != HUNT_RUNNING || event.type != EVT_BTN_CLICK) return;

    int pIdx = -1;
    for(int i=0; i<actualPuckCount; i++) {
        if (activePucks[i].globalIdx == globalPuckIdx) { pIdx = i; break; }
    }
    if (pIdx == -1) return;

    HuntPuck* p = &activePucks[pIdx];

    if (p->state == P_ACTIVE) {
        int playerId = p->assignedPlayerId;
        players[playerId].score++;
        players[playerId].lastPuckIdx = pIdx; 
        
        p->state = P_NEUTRAL;
        p->assignedPlayerId = -1;
        p->stateEndTime = 0;
        p->warningSent = false;
        
        setPuck(globalPuckIdx, EFF_BREATHE_MOD4, CRGB::White, 50, 85);
        if (soundOn) sendSound(globalPuckIdx, 80);
        
        players[playerId].currentPuckIdx = -1;
        players[playerId].penaltyEndTime = 0; 
        
        assignPucksFromQueue();
    } 
    else if (p->state == P_NEUTRAL) {
        p->state = P_ERROR;
        p->stateEndTime = millis() + 500; 
        p->warningSent = false;
        
        setPuck(globalPuckIdx, EFF_FLASH, CRGB::Red, 80, 255); // Feedback Rot statt Weiß für Fehler
        if (soundOn) sendSequence(globalPuckIdx, SEQ_ERROR); 
    }
}

void Game_Hunt::evaluateWinners() {
    gameState = HUNT_WINNER;
    stateStartTime = millis();

    int maxScore = -1;
    for(int i=0; i<numPlayers; i++) {
        if(players[i].score > maxScore) maxScore = players[i].score;
    }

    std::vector<int> winners;
    for(int i=0; i<numPlayers; i++) {
        if(players[i].score == maxScore && maxScore > 0) winners.push_back(i);
    }

    if (winners.size() == 0) {
        for(int i=0; i<actualPuckCount; i++) setPuck(activePucks[i].globalIdx, EFF_RAINBOW, CRGB::Black, 0, 200);
        return;
    }

    int pucksPerWinner = actualPuckCount / winners.size();
    int currentPuck = 0;

    for (size_t w=0; w<winners.size(); w++) {
        for(int p=0; p<pucksPerWinner; p++) {
            if (currentPuck < actualPuckCount) {
                setPuck(activePucks[currentPuck].globalIdx, EFF_DOUBLE_CHASE, players[winners[w]].color, 30, 255);
                currentPuck++;
            }
        }
    }

    while (currentPuck < actualPuckCount) {
        setPuck(activePucks[currentPuck].globalIdx, EFF_RAINBOW, CRGB::Black, 0, 200);
        currentPuck++;
    }
}

void Game_Hunt::setPuck(int index, int effect, CRGB color, int speed, int bright) {
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, cp);
}
void Game_Hunt::sendSound(int index, int duration) {
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = duration;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, snd);
}
void Game_Hunt::sendSequence(int index, int seqID) {
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = seqID;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, snd);
}

String Game_Hunt::getStatusJSON() {
    String json = "{";
    json += "\"st\":" + String(gameState) + ",";
    
    long t = 0;
    if (gameState == HUNT_RUNNING) t = gameDuration - (millis() - stateStartTime);
    if (t < 0 || gameState >= HUNT_FINISHED) t = 0;
    if (gameState == HUNT_SETUP || gameState == HUNT_COUNTDOWN) t = gameDuration;
    
    json += "\"t\":" + String(t) + ",";
    json += "\"lim\":" + String(gameDuration) + ",";
    
    json += "\"pl\":[";
    for(int i=0; i<numPlayers; i++) {
        if(i>0) json += ",";
        json += "{\"id\":" + String(players[i].id) + ",\"sc\":" + String(players[i].score) + "}";
    }
    json += "]}";
    return json;
}