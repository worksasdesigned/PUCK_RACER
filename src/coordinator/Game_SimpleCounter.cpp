#include "Game_SimpleCounter.h"
#include "PuckNetwork.h"

void Game_SimpleCounter::setup() {
    Serial.println("GAME: Setup Simple Counter");
    state = STATE_SETUP;
    
    PuckInfo* pucks = PuckNetwork::getPucks();
    
    // Alle Werte bis MAX_PEERS sicherheitshalber resetten
    for(int i=0; i<MAX_PEERS; i++) {
        scores[i] = 0;
        lastInput[i] = 0;
        isFlashing[i] = false;
    }
    
    // LEDs nur für die aktiven Pucks einschalten
    for(int i=0; i<activePucks; i++) {
        if (pucks[i].active) {
            setPuckToPlayerColor(i, EFF_BREATHE_MOD4, 50); // Langsames Atmen
        }
    }
}

void Game_SimpleCounter::processCommand(String cmd, int value) {
    if (cmd == "setup") {
        resetGame();
    }
    else if (cmd == "config_players") {
        // SICHERHEITSABFRAGE: Spielerzahl begrenzen
        if (value > 0 && value <= 12) {
            activePucks = value;
        }
    }
    else if (cmd == "config_time") timeLimitSeconds = value;
    else if (cmd == "config_warn") warnEnd = (value == 1);
    else if (cmd == "start") startCountdown();
    else if (cmd == "stop") stopGame();
    else if (cmd == "reset") resetGame();
    else if (cmd == "exit") exitGame();
}

void Game_SimpleCounter::startCountdown() {
    for(int i=0; i<MAX_PEERS; i++) scores[i] = 0;

    if (state == STATE_RUNNING) return;
    
    state = STATE_PREPARE;
    stateStartTime = millis();
    
    PuckInfo* pucks = PuckNetwork::getPucks();
    // Nur aktive Pucks blinken lassen
    for(int i=0; i<activePucks; i++) {
        if(!pucks[i].active) continue;
        CommandPacket cp;
        cp.cmd = CMD_EFFECT;
        cp.effectID = EFF_BLINK;
        cp.r = PLAYER_COLORS[i].r; cp.g = PLAYER_COLORS[i].g; cp.b = PLAYER_COLORS[i].b;
        cp.duration = 200; cp.extra = 255;
        PuckNetwork::sendToPuck(pucks[i].mac, cp);
        
        CommandPacket snd;
        snd.cmd = CMD_SOUND; snd.duration = 100;
        PuckNetwork::sendToPuck(pucks[i].mac, snd);
    }
}

void Game_SimpleCounter::loop() {
    unsigned long now = millis();

    // 1. COUNTDOWN
    if (state == STATE_PREPARE) {
        if (now - stateStartTime > 2500) { 
            state = STATE_RUNNING;
            gameStartTime = now;
            
            PuckInfo* pucks = PuckNetwork::getPucks();
            for(int i=0; i<activePucks; i++) {
                if(!pucks[i].active) continue;
                // Chase, Speed 40ms, Helligkeit 100
                setPuckToPlayerColor(i, EFF_SINGLE_CHASE, 40); 
                
                CommandPacket snd;
                snd.cmd = CMD_SOUND; snd.duration = 600; 
                PuckNetwork::sendToPuck(pucks[i].mac, snd);
            }
        }
    }
    // 2. SPIEL LÄUFT
    else if (state == STATE_RUNNING) {
        // A) Zeitlimit prüfen
        if (timeLimitSeconds > 0) {
            unsigned long elapsedSec = (now - gameStartTime) / 1000;
            if (warnEnd && (timeLimitSeconds - elapsedSec == 5) && (now % 1000 < 50)) {
                 CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 50;
                 PuckNetwork::broadcast(snd); 
            }
            if (elapsedSec >= timeLimitSeconds) stopGame();
        }

        // B) Flash-Rücksetzung prüfen
        for(int i=0; i<activePucks; i++) {
            if (isFlashing[i] && (now - flashStartTime[i] > 150)) {
                isFlashing[i] = false;
                setPuckToPlayerColor(i, EFF_SINGLE_CHASE, 40); // Zurück zum Spiel-Effekt
            }
        }
    }
    // 3. SPIEL ENDE (Gewinner Show)
    else if (state == STATE_FINISHED) {
        if (!winnerAnimationDone && (now - finishTime > 10000)) {
            winnerAnimationDone = true;
            PuckInfo* pucks = PuckNetwork::getPucks();
            for(int i=0; i<activePucks; i++) {
                if(pucks[i].active) setPuckToPlayerColor(i, EFF_BREATHE_MOD4, 50);
            }
        }
    }
}

void Game_SimpleCounter::handleEvent(int puckIndex, EventPacket event) {
    if (event.type != EVT_BTN_CLICK) return;
    
    // Eingaben von nicht teilnehmenden Spielern ignorieren
    if (puckIndex >= activePucks) return;
    
    unsigned long now = millis();

    if (state == STATE_PREPARE) {
        // Frühstart Strafe
        CommandPacket cp;
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_POLICE; cp.duration = 1000;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, cp);
        return; 
    }

    if (state == STATE_RUNNING) {
        if (now - lastInput[puckIndex] < 330) return;
        lastInput[puckIndex] = now;
        scores[puckIndex]++;

        // FLASH STARTEN
        isFlashing[puckIndex] = true;
        flashStartTime[puckIndex] = now;

        CommandPacket cp;
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_FLASH;
        cp.r = PLAYER_COLORS[puckIndex].r; cp.g = PLAYER_COLORS[puckIndex].g; cp.b = PLAYER_COLORS[puckIndex].b;
        cp.duration = 150; 
        cp.extra = 255; // Volle Helligkeit für Flash
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, cp);
        
        CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 80;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
    }
}

void Game_SimpleCounter::stopGame() {
    if (state == STATE_FINISHED) {
        // Wenn man nochmal STOP drückt, Animation sofort abbrechen
        if (!winnerAnimationDone) {
            finishTime = millis() - 11000; 
        }
        return;
    }
    state = STATE_FINISHED;
    finishTime = millis();
    winnerAnimationDone = false;

    CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 1000;
    PuckNetwork::broadcast(snd);

    int maxScore = -1;
    PuckInfo* pucks = PuckNetwork::getPucks();
    for(int i=0; i<activePucks; i++) {
        if(pucks[i].active && scores[i] > maxScore) maxScore = scores[i];
    }
    
    for(int i=0; i<activePucks; i++) {
        if(!pucks[i].active) continue;
        if (scores[i] == maxScore && maxScore > 0) {
            CommandPacket cp;
            cp.cmd = CMD_EFFECT; cp.effectID = EFF_RAINBOW; cp.duration = 0; cp.extra = 200;
            PuckNetwork::sendToPuck(pucks[i].mac, cp);
        } else {
            CommandPacket cp;
            cp.cmd = CMD_EFFECT; cp.effectID = EFF_OFF;
            PuckNetwork::sendToPuck(pucks[i].mac, cp);
        }
    }
}

void Game_SimpleCounter::resetGame() {
    state = STATE_SETUP;
    PuckInfo* pucks = PuckNetwork::getPucks();
    for(int i=0; i<MAX_PEERS; i++) {
        scores[i] = 0;
        isFlashing[i] = false;
    }
    for(int i=0; i<activePucks; i++) {
        if(pucks[i].active) setPuckToPlayerColor(i, EFF_BREATHE_MOD4, 50);
    }
}

void Game_SimpleCounter::exitGame() {
    state = STATE_SETUP;
    CommandPacket light;
    light.cmd = CMD_EFFECT; light.effectID = EFF_STATUS; 
    light.r = 0; light.g = 255; light.b = 0; 
    light.extra = 255; light.duration = 0; 
    PuckNetwork::broadcast(light);
    Serial.println("GAME: Exit -> Status Light");
}

void Game_SimpleCounter::setPuckToPlayerColor(int index, int effect, int speed) {
    PuckInfo* pucks = PuckNetwork::getPucks();
    CommandPacket cp;
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = PLAYER_COLORS[index].r; cp.g = PLAYER_COLORS[index].g; cp.b = PLAYER_COLORS[index].b;
    cp.duration = speed; 
    cp.extra = 100; 
    PuckNetwork::sendToPuck(pucks[index].mac, cp);
}

String Game_SimpleCounter::getStatusJSON() {
    String json = "{";
    json += "\"state\":" + String(state) + ",";
    long elapsed = 0;
    if (state == STATE_RUNNING) elapsed = millis() - gameStartTime;
    else if (state == STATE_FINISHED) elapsed = finishTime - gameStartTime;
    
    json += "\"time\":" + String(elapsed) + ",";
    json += "\"scores\":[";
    
    // JSON nur für aktive Spieler generieren!
    PuckInfo* pucks = PuckNetwork::getPucks();
    bool first = true;
    for(int i=0; i<activePucks; i++) {
        if(pucks[i].active) {
            if(!first) json += ",";
            json += "{\"id\":" + String(i) + ",\"sc\":" + String(scores[i]) + "}";
            first = false;
        }
    }
    json += "]}";
    return json;
}