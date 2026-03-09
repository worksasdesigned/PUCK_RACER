#include "Game_Musical.h"
#include "PuckNetwork.h"

void Game_Musical::setup() {
    Serial.println("GAME: Setup Musical Chairs");
    state = MUS_SETUP;
    
    PuckInfo* pucks = PuckNetwork::getPucks();
    for(int i=0; i<MAX_PEERS; i++) {
        puckStates[i] = 2; // Default ausgeschieden
    }
    
    // Aktiviere so viele Pucks, wie es Spieler gibt minus 1. (max verbundene Pucks)
    int pucksToActivate = initialPlayers - 1;
    if (pucksToActivate < 1) pucksToActivate = 1;
    
    int activated = 0;
    for(int i=0; i<MAX_PEERS; i++) {
        if(pucks[i].active && activated < pucksToActivate) {
            puckStates[i] = 0;
            activated++;
            setPuck(i, EFF_BREATHE_MOD4, CRGB::Blue, 50, 100);
        } else if (pucks[i].active) {
            setPuck(i, EFF_OFF, CRGB::Black);
        }
    }
    recalculateActive();
}

void Game_Musical::processCommand(String cmd, int value) {
    if (cmd == "setup") setup();
    else if (cmd == "cfg_ply") initialPlayers = value;
    else if (cmd == "cfg_base") baseTimeSec = value;
    else if (cmd == "cfg_var") varType = value;
    else if (cmd == "cfg_pause") pauseTimeSec = value;
    
    else if (cmd == "start") {
        if (state == MUS_SETUP || state == MUS_FINISHED) {
            setup(); // Reset
            startRound();
        }
    }
    else if (cmd == "stop") {
        state = MUS_FINISHED;
        CommandPacket cp; cp.cmd = CMD_EFFECT; cp.effectID = EFF_OFF;
        PuckNetwork::broadcast(cp);
    }
    else if (cmd == "reset") setup();
    else if (cmd == "exit") {
        CommandPacket cp; cp.cmd = CMD_EFFECT; cp.effectID = EFF_STATUS; 
        cp.r = 0; cp.g = 255; cp.b = 0; cp.extra = 255;
        PuckNetwork::broadcast(cp);
    }
    else if (cmd == "force_open") {
        if (state == MUS_PLAYING) triggerOpen();
    }
    else if (cmd == "toggle_pause") {
        if (state == MUS_PLAYING) {
            state = MUS_PAUSED_MANUAL;
            for(int i=0; i<MAX_PEERS; i++) {
                if (puckStates[i] == 0) setPuck(i, EFF_STATIC, CRGB::Yellow, 0, 100);
            }
        } else if (state == MUS_PAUSED_MANUAL) {
            startRound(); // Weiter gehts
        }
    }
    else if (cmd == "toggle_puck") {
        // Manuelles rein/rausnehmen durch den Trainer
        if (value >= 0 && value < MAX_PEERS) {
            if (puckStates[value] == 3 || puckStates[value] == 2) {
                puckStates[value] = 0; // Zurück ins Spiel
                if (state == MUS_PLAYING) setPuck(value, EFF_RAINBOW, CRGB::White, 0, 150);
                else setPuck(value, EFF_BREATHE_MOD4, CRGB::Blue, 50, 100);
            } else {
                puckStates[value] = 3; // Manuell deaktiviert
                setPuck(value, EFF_OFF, CRGB::Black);
            }
            recalculateActive();
        }
    }
}

void Game_Musical::startRound() {
    state = MUS_PLAYING;
    pressedCount = 0;
    
    for(int i=0; i<MAX_PEERS; i++) {
        if (puckStates[i] == 1) puckStates[i] = 0; // Gedrückte wieder freigeben
        if (puckStates[i] == 0) {
            setPuck(i, EFF_RAINBOW, CRGB::White, 0, 150);
        }
    }
    
    int varMs = 5000;
    if (varType == 0) varMs = 2000;
    if (varType == 2) varMs = 10000;
    
    unsigned long duration = (baseTimeSec * 1000UL) + random(0, varMs);
    stateStartTime = millis();
    targetTime = stateStartTime + duration;
}

void Game_Musical::triggerOpen() {
    state = MUS_OPEN;
    stateStartTime = millis();
    for(int i=0; i<MAX_PEERS; i++) {
        // Double Chase Lila signalisiert: Jetzt drücken!
        if (puckStates[i] == 0) setPuck(i, EFF_DOUBLE_CHASE, CRGB(128, 0, 255), 30, 255);
    }
}

void Game_Musical::loop() {
    unsigned long now = millis();

    if (state == MUS_PLAYING) {
        if (now >= targetTime) triggerOpen();
    }
    else if (state == MUS_PAUSED_INTERIM) {
        // Automatische Pause zwischen den Runden
        if (now - stateStartTime >= (pauseTimeSec * 1000UL)) {
            eliminateRandomPuck();
            startRound();
        }
    }
}

void Game_Musical::handleEvent(int puckIndex, EventPacket event) {
    if (event.type != EVT_BTN_CLICK) return;

    if (state == MUS_OPEN) {
        if (puckStates[puckIndex] == 0) {
            // Erfolgreich gedrückt!
            puckStates[puckIndex] = 1; 
            pressedCount++;
            setPuck(puckIndex, EFF_STATIC, CRGB::Green, 0, 255);
            
            CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 100;
            PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);

            if (pressedCount >= activePuckCount) {
                endRound();
            }
        } 
        else if (puckStates[puckIndex] == 1) {
            // Fail: Schon gedrückt!
            setPuck(puckIndex, EFF_FAIL, CRGB::Red, 0, 255);
            CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 300;
            PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
            
            // Nach kurzem Schock zurück zu grün
            delay(300);
            setPuck(puckIndex, EFF_STATIC, CRGB::Green, 0, 255);
        }
    }
}

void Game_Musical::endRound() {
    if (activePuckCount == 1) {
        // Finale!
        state = MUS_FINISHED;
        for(int i=0; i<MAX_PEERS; i++) {
            if (puckStates[i] == 1) setPuck(i, EFF_WIN, CRGB::Green, 0, 200);
        }
        CommandPacket snd; snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_FANFARE;
        PuckNetwork::broadcast(snd);
    } else {
        // Normale Runde zu Ende
        state = MUS_PAUSED_INTERIM;
        stateStartTime = millis();
    }
}

void Game_Musical::eliminateRandomPuck() {
    recalculateActive();
    if (activePuckCount <= 1) return; // Nichts zu eliminieren
    
    // Suche alle Pucks, die noch im Rennen sind (waren erfolgreich = 1)
    int candidates[MAX_PEERS];
    int candCount = 0;
    for(int i=0; i<MAX_PEERS; i++) {
        if (puckStates[i] == 1 || puckStates[i] == 0) {
            candidates[candCount] = i;
            candCount++;
        }
    }
    
    if (candCount > 0) {
        int victimIdx = candidates[random(0, candCount)];
        puckStates[victimIdx] = 2; // Raus!
        setPuck(victimIdx, EFF_OFF, CRGB::Black);
    }
    recalculateActive();
}

void Game_Musical::recalculateActive() {
    activePuckCount = 0;
    for(int i=0; i<MAX_PEERS; i++) {
        if (puckStates[i] == 0 || puckStates[i] == 1) activePuckCount++;
    }
}

void Game_Musical::setPuck(int index, int effect, CRGB color, int speed, int bright) {
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[index].mac, cp);
}

String Game_Musical::getStatusJSON() {
    String json = "{";
    json += "\"st\":" + String(state) + ",";
    json += "\"act\":" + String(activePuckCount) + ",";
    
    long t = 0;
    if (state == MUS_PLAYING) t = targetTime - millis();
    else if (state == MUS_PAUSED_INTERIM) t = (pauseTimeSec * 1000UL) - (millis() - stateStartTime);
    if (t < 0) t = 0;
    json += "\"t\":" + String(t) + ",";
    
    json += "\"pucks\":[";
    bool first = true;
    for(int i=0; i<MAX_PEERS; i++) {
        if(PuckNetwork::getPucks()[i].active) {
            if(!first) json += ",";
            json += "{\"id\":" + String(i) + ",\"s\":" + String(puckStates[i]) + "}";
            first = false;
        }
    }
    json += "]}";
    return json;
}