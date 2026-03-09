#include "Game_SimpleCountdown.h"
#include "PuckNetwork.h"

void Game_SimpleCountdown::setup() {
    Serial.println("GAME: Setup Simple Countdown");
    state = CD_SETUP;

    PuckInfo* pucks = PuckNetwork::getPucks();
    
    // Setze alle Pucks (bis MAX_PEERS) auf Standardwerte zurück
    for(int i=0; i<MAX_PEERS; i++) {
        currentCounts[i] = startCount;
        lastInput[i] = 0;
        isFlashing[i] = false;
        hasFinished[i] = false;
        finishTimes[i] = 0;
    }
    
    // Zeige auf den aktiven Pucks die zugehörige Spielerfarbe an
    for(int i=0; i<activePucks; i++) {
        if (pucks[i].active) setPuckToPlayerColor(i, EFF_BREATHE_MOD4, 50);
    }
}

void Game_SimpleCountdown::processCommand(String cmd, int value) {
    if (cmd == "setup") {
        resetGame();
    } 
    else if (cmd == "config_players") {
        // SICHERHEITSABFRAGE: Verhindere, dass activePucks auf 0 fällt 
        // oder größer als das PLAYER_COLORS Array (12) wird.
        if (value > 0 && value <= 12) {
            activePucks = value;
        }
    } 
    else if (cmd == "config_count") {
        startCount = value;
    } 
    else if (cmd == "config_time") {
        timeLimitSeconds = value;
    } 
    else if (cmd == "config_warn") {
        warnEnd = (value == 1);
    } 
    else if (cmd == "config_stopwin") {
        stopOnWin = (value == 1);
    } 
    else if (cmd == "start") {
        startCountdown();
    } 
    else if (cmd == "stop") {
        stopGame();
    } 
    else if (cmd == "reset") {
        resetGame();
    } 
    else if (cmd == "exit") {
        exitGame();
    }
}

void Game_SimpleCountdown::startCountdown() {
    winnerIndex = -1;
    winnerTime = 0;

    // Nur die konfigurierten Spieler (activePucks) zurücksetzen
    for(int i=0; i<activePucks; i++) {
        currentCounts[i] = startCount;
        hasFinished[i] = false;
        finishTimes[i] = 0;
    }

    if (state == CD_RUNNING) return;
    state = CD_PREPARE;
    stateStartTime = millis();

    PuckInfo* pucks = PuckNetwork::getPucks();
    
    // Bereite alle konfigurierten UND physisch aktiven Pucks vor
    for(int i=0; i<activePucks; i++) {
        if(!pucks[i].active) continue;
        CommandPacket cp;
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_BLINK;
        cp.r = PLAYER_COLORS[i].r; cp.g = PLAYER_COLORS[i].g; cp.b = PLAYER_COLORS[i].b;
        cp.duration = 200; cp.extra = 255;
        PuckNetwork::sendToPuck(pucks[i].mac, cp);
    }

    // Start-Sequenz abspielen
    CommandPacket snd; memset(&snd, 0, sizeof(snd));
    snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
    PuckNetwork::broadcast(snd);
}

void Game_SimpleCountdown::loop() {
    unsigned long now = millis();

    // 1. PREPARE / COUNTDOWN PHASE
    if (state == CD_PREPARE) {
        if (now - stateStartTime > 3100) {
            state = CD_RUNNING;
            gameStartTime = now;

            PuckInfo* pucks = PuckNetwork::getPucks();
            for(int i=0; i<activePucks; i++) {
                if(!pucks[i].active) continue;
                updateProgress(i);
            }
        }
    }
    // 2. RUNNING PHASE
    else if (state == CD_RUNNING) {
        if (timeLimitSeconds > 0) {
            unsigned long elapsedSec = (now - gameStartTime) / 1000;
            
            // Warnton bei noch 5 verbleibenden Sekunden
            if (warnEnd && (timeLimitSeconds - elapsedSec == 5) && (now % 1000 < 50)) {
                 CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 50;
                 PuckNetwork::broadcast(snd);
            }
            
            // Zeitlimit erreicht
            if (elapsedSec >= timeLimitSeconds) stopGame();
        }

        // Visuelles Feedback (Flash) nach Input zurücksetzen
        for(int i=0; i<activePucks; i++) {
            if (!hasFinished[i] && isFlashing[i] && (now - flashStartTime[i] > 150)) {
                isFlashing[i] = false;
                updateProgress(i);
            }
        }
    }
    // 3. FINISHED PHASE
    else if (state == CD_FINISHED) {
        if (!winnerAnimationDone && (now - finishTime > 10000)) {
            winnerAnimationDone = true;
            PuckInfo* pucks = PuckNetwork::getPucks();
            
            // Setze Lichter nach Siegeranimation wieder auf normales Atmen
            for(int i=0; i<activePucks; i++) {
                if(pucks[i].active) setPuckToPlayerColor(i, EFF_BREATHE_MOD4, 50);
            }
        }
    }
}

void Game_SimpleCountdown::handleEvent(int puckIndex, EventPacket event) {
    if (event.type != EVT_BTN_CLICK) return;
    
    // Ignoriere Eingaben von Pucks, die nicht zu den konfigurierten Spielern gehören
    if (puckIndex >= activePucks) return;
    
    unsigned long now = millis();

    // Klick während der Vorbereitung bestrafen/anzeigen
    if (state == CD_PREPARE) {
        CommandPacket cp;
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_POLICE; cp.duration = 1000;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, cp);
        return;
    }

    // Regulärer Klick im Spiel
    if (state == CD_RUNNING) {
        if (hasFinished[puckIndex]) return;

        // Entprellen (Debounce): Nur alle 300ms einen Klick zählen
        if (now - lastInput[puckIndex] < 300) return; 
        lastInput[puckIndex] = now;

        currentCounts[puckIndex]--;
        if (currentCounts[puckIndex] < 0) currentCounts[puckIndex] = 0;

        // Visuelles Feedback für den Klick (Flash)
        isFlashing[puckIndex] = true;
        flashStartTime[puckIndex] = now;

        CommandPacket cp;
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_FLASH;
        cp.r = PLAYER_COLORS[puckIndex].r; cp.g = PLAYER_COLORS[puckIndex].g; cp.b = PLAYER_COLORS[puckIndex].b;
        cp.duration = 150; cp.extra = 255;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, cp);

        CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 80;
        PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);

        // Prüfen, ob der Spieler fertig ist
        if (currentCounts[puckIndex] <= 0) {
            hasFinished[puckIndex] = true;
            finishTimes[puckIndex] = millis() - gameStartTime;

            // Ist dieser Spieler der Erste?
            if (winnerIndex == -1) {
                winnerIndex = puckIndex;
                winnerTime = finishTimes[puckIndex];

                CommandPacket win;
                win.cmd = CMD_EFFECT; win.effectID = EFF_RAINBOW; win.duration = 0; win.extra = 200;
                PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, win);

                if (stopOnWin) {
                    stopGame();
                }
            } else {
                // Nicht der Erste -> Double Chase Effekt
                setPuckToPlayerColor(puckIndex, EFF_DOUBLE_CHASE, 60);
            }

            // Prüfen, ob alle konfigurierten Pucks fertig sind
            bool allDone = true;
            PuckInfo* pucks = PuckNetwork::getPucks();
            for(int i=0; i<activePucks; i++) {
                if(pucks[i].active && !hasFinished[i]) allDone = false;
            }
            if(allDone) stopGame();
        }
    }
}

void Game_SimpleCountdown::updateProgress(int index) {
    if (startCount <= 0) return;
    
    // Berechne Fortschritt in Prozent (auf 255 skaliert für LEDs)
    int percent = (currentCounts[index] * 255) / startCount;
    if (percent > 255) percent = 255;
    if (percent < 0) percent = 0;
    setPuckToPlayerColor(index, EFF_PROGRESS, percent);
}

void Game_SimpleCountdown::stopGame() {
    if (state == CD_FINISHED) {
        if (!winnerAnimationDone) finishTime = millis() - 11000;
        return;
    }
    state = CD_FINISHED;
    finishTime = millis();
    winnerAnimationDone = false;

    CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 1000;
    PuckNetwork::broadcast(snd);

    PuckInfo* pucks = PuckNetwork::getPucks();
    for(int i=0; i<activePucks; i++) {
        if(!pucks[i].active) continue;
        // Pucks, die nicht fertig wurden, ausschalten
        if (!hasFinished[i]) {
            CommandPacket cp;
            cp.cmd = CMD_EFFECT; cp.effectID = EFF_OFF;
            PuckNetwork::sendToPuck(pucks[i].mac, cp);
        }
    }
}

void Game_SimpleCountdown::resetGame() {
    state = CD_SETUP;
    winnerIndex = -1;
    winnerTime = 0;
    PuckInfo* pucks = PuckNetwork::getPucks();
    
    // Alle Array-Werte löschen
    for(int i=0; i<MAX_PEERS; i++) {
        currentCounts[i] = startCount;
        finishTimes[i] = 0;
        isFlashing[i] = false;
        hasFinished[i] = false;
    }
    
    // LEDs der konfigurierten Spieler neu starten
    for(int i=0; i<activePucks; i++) {
        if(pucks[i].active) setPuckToPlayerColor(i, EFF_BREATHE_MOD4, 50);
    }
}

void Game_SimpleCountdown::exitGame() {
    state = CD_SETUP;
    CommandPacket light;
    light.cmd = CMD_EFFECT; light.effectID = EFF_STATUS;
    light.r = 0; light.g = 255; light.b = 0;
    light.extra = 255; light.duration = 0;
    PuckNetwork::broadcast(light);
}

void Game_SimpleCountdown::setPuckToPlayerColor(int index, int effect, int val) {
    PuckInfo* pucks = PuckNetwork::getPucks();
    CommandPacket cp;
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = PLAYER_COLORS[index].r; cp.g = PLAYER_COLORS[index].g; cp.b = PLAYER_COLORS[index].b;
    cp.duration = val;
    cp.extra = 100;
    PuckNetwork::sendToPuck(pucks[index].mac, cp);
}

String Game_SimpleCountdown::getStatusJSON() {
    String json = "{";
    json += "\"state\":" + String(state) + ",";
    
    long elapsed = 0;
    if (state == CD_RUNNING) elapsed = millis() - gameStartTime;
    else if (state == CD_FINISHED) elapsed = finishTime - gameStartTime;

    json += "\"time\":" + String(elapsed) + ",";
    json += "\"winnerTime\":" + String(winnerTime) + ",";
    json += "\"scores\":[";

    // Baue das JSON nur für die aktiv konfigurierten Spieler auf
    for(int i=0; i<activePucks; i++) {
        if(i > 0) json += ",";
        json += "{\"id\":" + String(i) +
                ",\"sc\":" + String(currentCounts[i]) +
                ",\"ft\":" + String(finishTimes[i]) + "}";
    }
    json += "]}";
    return json;
}