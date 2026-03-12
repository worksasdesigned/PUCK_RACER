#include "Game_BombSquad.h"
#include "PuckNetwork.h"

void Game_BombSquad::setup() {
    Serial.println("GAME: Setup Bomb Squad");
    state = BOMB_SETUP;
}

void Game_BombSquad::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "cfg_pucks") activePucks = value;
    else if (cmd == "cfg_time") maxSeconds = value;
    else if (cmd == "cfg_diff") difficulty = value;
    else if (cmd == "cfg_rounds") maxRounds = value;
    else if (cmd == "cfg_group") groupMode = (value == 1);
    
    else if (cmd == "start") startRound();
    else if (cmd == "reset") initGame(); 
    else if (cmd == "stop") {
        state = BOMB_FINISHED;
        finishTime = millis();
        for(int i=0; i<activePucks; i++) setPuck(i, EFF_OFF, CRGB::Black);
    }
    else if (cmd == "exit") {
        state = BOMB_SETUP;
        CommandPacket cp; cp.cmd=CMD_EFFECT; cp.effectID=EFF_STATUS; cp.r=0; cp.g=255; cp.b=0; cp.extra=255; cp.duration=0;
        PuckNetwork::broadcast(cp);
    }
}

void Game_BombSquad::initGame() {
    currentRound = 0;
    state = BOMB_SETUP;
    for(int i=0; i<MAX_PEERS; i++) {
        players[i].score = 0;
        players[i].lastResult = RES_NONE;
        players[i].lastDelta = 0;
        players[i].totalDelta = 0;
        players[i].roundsPlayed = 0;
        players[i].targetTime = 0;
        hasActed[i] = false;
        if (i < activePucks) {
            setPuck(i, EFF_SINGLE_CHASE, PLAYER_COLORS[i], 40, 100);
        }
    }
}

void Game_BombSquad::startRound() {
    currentRound++;
    state = BOMB_INFO;
    stateStartTime = millis();
    
    int upper = maxSeconds + 1;
    if (upper < 4) upper = 4;
    
    int groupTarget = random(3, upper);
    maxTargetTimeThisRound = 0;
    
    if (groupMode) globalTargetTime = groupTarget;
    else globalTargetTime = 0;
    
    for(int i=0; i<activePucks; i++) {
        hasActed[i] = false;
        players[i].lastResult = RES_NONE;
        
        players[i].targetTime = groupMode ? groupTarget : random(3, upper);
        
        if (players[i].targetTime > maxTargetTimeThisRound) {
            maxTargetTimeThisRound = players[i].targetTime;
        }
        
        setPuck(i, EFF_BLINK_COUNT, PLAYER_COLORS[i], 500, 200, players[i].targetTime);
    }
}

void Game_BombSquad::loop() {
    unsigned long now = millis();
    
    if (state == BOMB_INFO) {
        if (now - stateStartTime > (unsigned long)(maxTargetTimeThisRound * 1000 + 1200)) {
            state = BOMB_COUNTDOWN;
            stateStartTime = now;
            sendSequence(SEQ_SKI); 
        }
    }
    else if (state == BOMB_COUNTDOWN) {
        if (now - stateStartTime > 3100) {
            state = BOMB_RUNNING;
            gameStartTime = now; 
            for(int i=0; i<activePucks; i++) setPuck(i, EFF_OFF, CRGB::Black);
        }
    }
    else if (state == BOMB_RUNNING) {
        if (now - gameStartTime > (unsigned long)(maxTargetTimeThisRound * 1000 + 5000)) {
            evaluateRound();
        }
    }
}

void Game_BombSquad::handleEvent(int puckIndex, EventPacket event) {
    if (state != BOMB_RUNNING) return;
    if (puckIndex >= activePucks) return;
    if (event.type != EVT_BTN_CLICK) return;
    if (hasActed[puckIndex]) return; 

    unsigned long pressTime = millis(); // VOR jeglicher Logik erfassen (Bug 4 Fix)
    hasActed[puckIndex] = true;
    
    long delta = (long)(pressTime - gameStartTime) - (players[puckIndex].targetTime * 1000);
    
    players[puckIndex].lastDelta = delta;
    players[puckIndex].totalDelta += abs(delta);
    players[puckIndex].roundsPlayed++;

    int tol = getTolerance();
    bool success = (abs(delta) <= tol);
    
    if (success) {
        players[puckIndex].lastResult = RES_DEFUSED;
        players[puckIndex].score++;
        
        setPuck(puckIndex, EFF_RAINBOW, CRGB::Black, 0, 150);
        
        CommandPacket snd;
        snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_DINGDONG;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);

    } else {
        players[puckIndex].lastResult = RES_EXPLODED;
        setPuck(puckIndex, EFF_FAIL, CRGB::Red, 0, 255);
        
        CommandPacket snd;
        snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_EXPLOSION;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
    }

    bool allDone = true;
    for(int i=0; i<activePucks; i++) if(!hasActed[i]) allDone = false;
    
    if (allDone) {
        evaluateRound();
    }
}

void Game_BombSquad::evaluateRound() {
    // GUARD: Verhindert doppelten Aufruf durch Loop-Timeout und allDone (Bug 2 Fix)
    if (state != BOMB_RUNNING) return; 
    
    state = BOMB_FINISHED; // Sofort blockieren
    finishTime = millis();

    for(int i=0; i<activePucks; i++) {
        if(!hasActed[i]) {
            players[i].lastResult = RES_EXPLODED;
            players[i].lastDelta = 99999; 
            players[i].totalDelta += 10000; 
            players[i].roundsPlayed++;
            setPuck(i, EFF_STATIC, CRGB::Red, 0, 50); 
        }
    }
    
    if (currentRound >= maxRounds) {
        CommandPacket snd;
        snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_FANFARE;
        PuckNetwork::broadcast(snd);
        
        for(int i=0; i<activePucks; i++) setPuck(i, EFF_RAINBOW, CRGB::Black, 0, 150);
    }
}

int Game_BombSquad::getTolerance() {
    switch(difficulty) {
        case 0: return 750;
        case 1: return 500;
        case 2: return 250;
        default: return 500;
    }
}

void Game_BombSquad::sendSequence(int seqID) {
    CommandPacket cp;
    cp.cmd = CMD_SEQUENCE;
    cp.extra = seqID;
    PuckNetwork::broadcast(cp);
}

void Game_BombSquad::setPuck(int index, int effect, CRGB color, int speed, int bright, int extra) {
    if(index < 0 || index >= MAX_PEERS) return;
    PuckInfo* p = PuckNetwork::getPucks();
    if(!p[index].active) return;
    
    CommandPacket cp;
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g;
    cp.b = color.b;
    cp.duration = speed; cp.extra = bright; 
    if (effect == EFF_BLINK_COUNT) cp.extra = extra; 
    
    PuckNetwork::sendToPuck(p[index].mac, cp);
}

String Game_BombSquad::getStatusJSON() {
    String json = "{";
    json += "\"state\":" + String(state) + ",";
    json += "\"round\":" + String(currentRound) + ",";
    json += "\"max\":" + String(maxRounds) + ",";
    json += "\"target\":" + String(globalTargetTime) + ","; 
    
    long elapsed = 0;
    if (state == BOMB_RUNNING) elapsed = millis() - gameStartTime;
    else if (state == BOMB_FINISHED) elapsed = finishTime - gameStartTime;
    if (state < BOMB_RUNNING) elapsed = 0;
    if (elapsed < 0) elapsed = 0;
    
    json += "\"time\":" + String(elapsed) + ",";
    
    json += "\"players\":[";
    for(int i=0; i<activePucks; i++) {
        if(i>0) json += ",";
        long avg = (players[i].roundsPlayed > 0) ? (players[i].totalDelta / players[i].roundsPlayed) : 0;
        json += "{";
        json += "\"score\":" + String(players[i].score) + ",";
        json += "\"res\":" + String(players[i].lastResult) + ",";
        json += "\"last\":" + String(players[i].lastDelta) + ",";
        json += "\"avg\":" + String(avg) + ",";
        json += "\"target\":" + String(players[i].targetTime); // NEU für UI Anzeige (Zielzeit pro Puck)
        json += "}";
    }
    json += "]}";
    return json;
}