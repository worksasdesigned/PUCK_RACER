#include "Game_Pacemaker.h"
#include "PuckNetwork.h"

void Game_Pacemaker::setup() {
    Serial.println("GAME: Setup Pacemaker");
    initGame();
}

void Game_Pacemaker::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "show_track") showTrack();
    else if (cmd == "cfg_pace") paceSecondsPerKm = value;
    else if (cmd == "cfg_dist") distanceBetweenPucks = value;
    else if (cmd == "cfg_dur") durationMs = value * 1000UL;
    else if (cmd == "cfg_shut") shuttleMode = (value == 1);
    else if (cmd == "cfg_hold") holdToStart = (value == 1);
    else if (cmd == "cfg_inpt") inputMode = value;
    else if (cmd == "cfg_lapt") lapTimeMs = value * 1000UL;
    else if (cmd == "cfg_grp") numGroups = constrain(value, 1, 2);

    else if (cmd == "change_pace" && inputMode == 0) {
        paceSecondsPerKm += value;
        paceSecondsPerKm = constrain(paceSecondsPerKm, 150, 480); // 2:30 to 8:00 min/km
        msPerPuck = paceSecondsPerKm * distanceBetweenPucks;
    }
    else if (cmd == "change_lap" && inputMode == 1) {
        long newLap = (long)lapTimeMs + (long)value * 1000L;
        lapTimeMs = (unsigned long)constrain(newLap, 5000L, 240000L); // 5s to 4min
        int steps_per_lap = shuttleMode ? ((activePucksCount - 1) * 2) : activePucksCount;
        if (steps_per_lap <= 0) steps_per_lap = 1;
        msPerPuck = lapTimeMs / steps_per_lap;
    }
    
    else if (cmd == "start") startGameSequence();
    else if (cmd == "stop") {
        if (gameState == PM_RUNNING) stoppedElapsedMs = millis() - runStartTime;
        else stoppedElapsedMs = durationMs;
        gameState = PM_FINISHED;
        for(int i=0; i<activePucksCount; i++) setPuck(puckGlobalIds[i], EFF_STATUS, CRGB::Green, 0, 85);
    }
    else if (cmd == "reset") initGame(); 
    else if (cmd == "exit") {
        CommandPacket cp; memset(&cp, 0, sizeof(cp));
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_STATUS; cp.r = 0; cp.g = 255; cp.b = 0; cp.extra = 255;
        PuckNetwork::broadcast(cp);
        gameState = PM_SETUP;
    }
}

void Game_Pacemaker::initGame() {
    PuckInfo* netPucks = PuckNetwork::getPucks();
    activePucksCount = 0;
    
    for(int i=0; i<MAX_PEERS; i++) {
        if(netPucks[i].active) {
            puckGlobalIds[activePucksCount] = i;
            activePucksCount++;
        }
    }
    
    if (inputMode == 1 && activePucksCount > 0) { 
        int steps_per_lap = shuttleMode ? ((activePucksCount - 1) * 2) : activePucksCount;
        if(steps_per_lap == 0) steps_per_lap = 1;
        msPerPuck = lapTimeMs / steps_per_lap;
    } else { 
        msPerPuck = paceSecondsPerKm * distanceBetweenPucks; 
    }
    
    currentSegment = -1;
    halfPassedFlag = false;
    falseStartPlayer = -1;
    stoppedElapsedMs = 0;
    gameState = PM_SETUP;
}

CRGB Game_Pacemaker::getPuckColor(int idx) {
    // 4er Blöcke: Pucks 0-3 = Farbe 0, 4-7 = Farbe 1, 8-11 = Farbe 2, ...
    // Bei >8 Pucks erscheint automatisch eine dritte Farbe (Grün).
    return BLOCK_COLORS[(idx / 4) % 8];
}

CRGB Game_Pacemaker::getGroupColor(int group) {
    return (group == 0) ? CRGB::Green : CRGB::Magenta;
}

int Game_Pacemaker::getSequencePuck(int group, int step) {
    if (activePucksCount == 0) return 0;
    int startIdx = (group == 0) ? 0 : (activePucksCount / 2);
    
    if (!shuttleMode) {
        return (startIdx + step) % activePucksCount;
    } else {
        int L = (activePucksCount - 1) * 2;
        if (L == 0) return 0;
        int modStep = step % L;
        int pos = startIdx + modStep; 
        if (pos < activePucksCount) return pos;
        else return L - pos; 
    }
}

void Game_Pacemaker::showTrack() {
    for(int i=0; i<activePucksCount; i++) {
        // 4 Stufen pro Farbblock: 25%, 50%, 75%, 100% Füllung.
        int fillLevel = ((i % 4) + 1) * 64;
        if (fillLevel > 255) fillLevel = 255;
        setPuck(puckGlobalIds[i], EFF_PROGRESS, getPuckColor(i), fillLevel, 200);
        delay(15);
    }
}

void Game_Pacemaker::startGameSequence() {
    if (activePucksCount == 0) return;
    
    currentPuckIdx[0] = 0;
    if (numGroups == 2) currentPuckIdx[1] = activePucksCount / 2; 
    
    falseStartPlayer = -1;
    
    for(int i=0; i<activePucksCount; i++) {
        setPuck(puckGlobalIds[i], EFF_OFF, CRGB::Black, 0, 0);
        delay(10);
    }
    
    if (holdToStart) {
        gameState = PM_WAIT_HANDS;
        for(int r=0; r<numGroups; r++) {
            isHoldingStart[r] = false;
            setPuck(puckGlobalIds[currentPuckIdx[r]], EFF_SINGLE_CHASE, getGroupColor(r), 40, 200);
            delay(10);
        }
    } else {
        gameState = PM_COUNTDOWN;
        stateStartTime = millis();
        for(int r=0; r<numGroups; r++) {
            sendSequence(puckGlobalIds[currentPuckIdx[r]], SEQ_SKI);
            setPuck(puckGlobalIds[currentPuckIdx[r]], EFF_STATIC, getGroupColor(r), 0, 255);
        }
    }
}

void Game_Pacemaker::loop() {
    unsigned long now = millis();

    if (gameState == PM_COUNTDOWN) {
        if (now - stateStartTime > 3000) {
            gameState = PM_RUNNING;
            runStartTime = now;
            currentSegment = -1;
            halfPassedFlag = false;
        }
    }
    else if (gameState == PM_FALSE_START) {
        if (now - stateStartTime > 2000) {
            gameState = PM_WAIT_HANDS;
            falseStartPlayer = -1;
            for(int i=0; i<activePucksCount; i++) {
                setPuck(puckGlobalIds[i], EFF_OFF, CRGB::Black, 0, 0);
                delay(10);
            }
            for(int r=0; r<numGroups; r++) {
                setPuck(puckGlobalIds[currentPuckIdx[r]], EFF_SINGLE_CHASE, getGroupColor(r), 40, 200);
            }
        }
    }
    else if (gameState == PM_RUNNING) {
        unsigned long elapsed = now - runStartTime;
        
        if (elapsed >= durationMs) {
            stoppedElapsedMs = durationMs;
            gameState = PM_FINISHED;
            for(int r=0; r<numGroups; r++) {
                sendSequence(puckGlobalIds[getSequencePuck(r, currentSegment)], SEQ_FANFARE);
            }
            for(int i=0; i<activePucksCount; i++) setPuck(puckGlobalIds[i], EFF_WIN, CRGB::Green, 0, 200);
            return;
        }

        int segment = elapsed / msPerPuck;
        unsigned long phase = elapsed % msPerPuck;
        bool halfPassed = phase >= (msPerPuck / 2);
        
        if (segment != currentSegment) {
            currentSegment = segment;
            halfPassedFlag = false;
            
            for(int r = 0; r < numGroups; r++) {
                int reachedIdx = getSequencePuck(r, segment);
                int targetIdx = getSequencePuck(r, segment + 1);
                CRGB col = getGroupColor(r);
                
                unsigned long window = msPerPuck; 
                if (window > 1500) window = 1500; 
                int speed = window / 35; 
                if (speed < 1) speed = 1;
                
                setPuck(puckGlobalIds[reachedIdx], EFF_COUNTDOWN, col, speed, 255);
                sendSound(puckGlobalIds[reachedIdx], 80);
                
                setPuck(puckGlobalIds[targetIdx], EFF_STATIC, col, 0, 255);
            }
        }
        
        if (halfPassed && !halfPassedFlag) {
            halfPassedFlag = true;
            for(int r = 0; r < numGroups; r++) {
                int reachedIdx = getSequencePuck(r, segment);
                int nextNextIdx = getSequencePuck(r, segment + 2);
                CRGB col = getGroupColor(r);
                
                setPuck(puckGlobalIds[reachedIdx], EFF_OFF, CRGB::Black, 0, 0);
                setPuck(puckGlobalIds[nextNextIdx], EFF_STATIC, col, 0, 50); 
            }
        }
    }
}

void Game_Pacemaker::handleEvent(int puckIndex, EventPacket event) {
    if (gameState == PM_WAIT_HANDS || gameState == PM_COUNTDOWN) {
        for(int r=0; r<numGroups; r++) {
            if (puckIndex == puckGlobalIds[currentPuckIdx[r]]) { 
                if (event.type == EVT_BTN_CLICK) {
                    isHoldingStart[r] = true;
                    setPuck(puckIndex, EFF_DOUBLE_CHASE, getGroupColor(r), 20, 255);
                    
                    bool allReady = true;
                    for(int j=0; j<numGroups; j++) { if(!isHoldingStart[j]) allReady = false; }
                    
                    if (gameState == PM_WAIT_HANDS && allReady) {
                        gameState = PM_COUNTDOWN;
                        stateStartTime = millis();
                        for(int j=0; j<numGroups; j++) sendSequence(puckGlobalIds[currentPuckIdx[j]], SEQ_SKI);
                    }
                } else if (event.type == EVT_BTN_RELEASE) {
                    isHoldingStart[r] = false;
                    if (gameState == PM_WAIT_HANDS) {
                        setPuck(puckIndex, EFF_SINGLE_CHASE, getGroupColor(r), 40, 200);
                    } else if (gameState == PM_COUNTDOWN) {
                        triggerFalseStart(r);
                    }
                }
            }
        }
    }
}

void Game_Pacemaker::triggerFalseStart(int runner) {
    gameState = PM_FALSE_START;
    stateStartTime = millis();
    falseStartPlayer = runner;
    
    for(int r=0; r<numGroups; r++) {
        if (r == runner) {
            setPuck(puckGlobalIds[currentPuckIdx[r]], EFF_POLICE, CRGB::Red, 0, 255);
            sendSequence(puckGlobalIds[currentPuckIdx[r]], SEQ_ERROR);
        } else {
            setPuck(puckGlobalIds[currentPuckIdx[r]], EFF_OFF, CRGB::Black, 0, 0);
            // NEU: Zwingt den unschuldigen Puck, sein langes Countdown-Audio SOFORT abzubrechen!
            sendSound(puckGlobalIds[currentPuckIdx[r]], 50); 
        }
    }
}

void Game_Pacemaker::setPuck(int globalIdx, int effect, CRGB color, int speed, int bright) {
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[globalIdx].mac, cp);
}

void Game_Pacemaker::sendSound(int globalIdx, int duration) {
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = duration;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[globalIdx].mac, snd);
}

void Game_Pacemaker::sendSequence(int globalIdx, int seqID) {
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = seqID;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[globalIdx].mac, snd);
}

String Game_Pacemaker::getStatusJSON() {
    String json = "{";
    json += "\"st\":" + String(gameState) + ",";
    
    long t = 0;
    if (gameState == PM_RUNNING) t = durationMs - (millis() - runStartTime);
    else if (gameState == PM_FINISHED) t = durationMs - (long)stoppedElapsedMs;
    else if (gameState == PM_SETUP || gameState == PM_WAIT_HANDS || gameState == PM_COUNTDOWN) t = durationMs;
    if (t < 0) t = 0;
    
    json += "\"t\":" + String(t) + ",";
    json += "\"lim\":" + String(durationMs) + ",";
    json += "\"pace\":" + String(paceSecondsPerKm) + ",";
    json += "\"dist\":" + String(distanceBetweenPucks) + ",";
    json += "\"shut\":" + String(shuttleMode ? 1 : 0) + ",";
    json += "\"hld\":" + String(holdToStart ? 1 : 0) + ",";
    json += "\"pucks\":" + String(activePucksCount) + ",";
    json += "\"inpt\":" + String(inputMode) + ",";
    json += "\"lapt\":" + String(lapTimeMs / 1000) + ",";
    json += "\"grp\":" + String(numGroups) + ",";
    
    // NEU: Status für die UI Darstellung (Hand-Icon und Fehlstart)
    json += "\"run\":[";
    for(int r=0; r<numGroups; r++) {
        if(r>0) json += ",";
        json += "{\"h\":" + String(isHoldingStart[r] ? 1 : 0) + "}";
    }
    json += "],";
    json += "\"fs\":" + String(falseStartPlayer);
    
    json += "}";
    return json;
}