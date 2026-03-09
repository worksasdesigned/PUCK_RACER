#include "Game_Tabata.h"
#include "PuckNetwork.h"

void Game_Tabata::setup() {
    Serial.println("GAME: Setup Tabata");
    // initGame/Licht kommt erst via cmd=setup (names.html)
}

void Game_Tabata::processCommand(String cmd, int value) {
    if (cmd == "setup") initGame();
    else if (cmd == "cfg_pucks") activePucksCount = constrain(value, 1, MAX_PEERS);
    else if (cmd == "cfg_work") workTimeMs = value * 1000UL;
    else if (cmd == "cfg_rest") restTimeMs = value * 1000UL;
    else if (cmd == "cfg_sound") soundMode = constrain(value, 0, 2); 
    else if (cmd == "cfg_lmode") limitMode = value; 
    else if (cmd == "cfg_lval") limitValue = value; 
    
    else if (cmd == "start") startGameSequence();
    else if (cmd == "stop") {
        gameState = TABATA_FINISHED;
        setAllPucks(EFF_STATUS, CRGB::Green, 0, 100);
    }
    else if (cmd == "pause") {
        if (gameState == TABATA_WORK || gameState == TABATA_REST) {
            // Spiel wird pausiert
            prePauseState = gameState;
            pauseBeginTime = millis();
            gameState = TABATA_PAUSED;
            setAllPucks(EFF_BREATHE_MOD4, CRGB::Yellow, 50, 150); // Gelbes Atmen als Pause-Signal
            if (soundMode != 2) sendBeep(150); 
        } 
        else if (gameState == TABATA_PAUSED) {
            // Spiel wird fortgesetzt
            unsigned long pauseDuration = millis() - pauseBeginTime;
            phaseStartTime += pauseDuration;
            globalStartTime += pauseDuration;
            lastTxTime += pauseDuration; 
            
            gameState = prePauseState;
            if (soundMode != 2) sendBeep(150);
            
            // Visuellen Status wiederherstellen
            if (gameState == TABATA_REST) {
                setAllPucks(EFF_BREATHE_MOD4, CRGB::Red, 50, 100);
            } else {
                lastFill = -1; // Erzwingt im nächsten Loop sofortiges Zeichnen des Balkens
            }
        }
    }
    else if (cmd == "reset") initGame(); 
    else if (cmd == "exit") {
        setAllPucks(EFF_STATUS, CRGB::Green, 0, 255);
        gameState = TABATA_SETUP;
    }
}

void Game_Tabata::initGame() {
    gameState = TABATA_SETUP;
    rounds = 0;
    setAllPucks(EFF_BREATHE_MOD4, CRGB::Turquoise, 50, 100);
}

void Game_Tabata::startGameSequence() {
    if (limitMode == 0) {
        targetRounds = limitValue * activePucksCount; 
        targetTimeMs = 0;
    } else {
        targetTimeMs = limitValue * 1000UL;
        targetRounds = 0;
    }

    gameState = TABATA_COUNTDOWN;
    phaseStartTime = millis();
    rounds = 1;
    setAllPucks(EFF_BLINK, CRGB::Purple, 200, 200);
    
    if (soundMode != 2) {
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
        PuckNetwork::broadcast(snd);
    }
}

void Game_Tabata::triggerFinish() {
    gameState = TABATA_FINISHED;
    if (soundMode != 2) {
        CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_FANFARE;
        PuckNetwork::broadcast(snd);
    }
    setAllPucks(EFF_WIN, CRGB::Green, 0, 200);
}

void Game_Tabata::loop() {
    unsigned long now = millis();

    if (gameState == TABATA_COUNTDOWN) {
        if (now - phaseStartTime > 3100) {
            gameState = TABATA_WORK;
            globalStartTime = now;
            phaseStartTime = now;
            warningStep = 0;
            lastFill = -1;
        }
    }
    else if (gameState == TABATA_WORK || gameState == TABATA_REST) {
        
        if (limitMode == 1 && targetTimeMs > 0 && (now - globalStartTime) >= targetTimeMs) {
            triggerFinish();
            return;
        }

        unsigned long phaseDuration = (gameState == TABATA_WORK) ? workTimeMs : restTimeMs;
        long timeLeft = phaseDuration - (now - phaseStartTime);

        if (timeLeft <= 0) {
            warningStep = 0;
            lastFill = -1;
            phaseStartTime = now;
            
            if (gameState == TABATA_WORK) {
                if (limitMode == 0 && targetRounds > 0 && rounds >= targetRounds) {
                    triggerFinish();
                    return;
                }

                gameState = TABATA_REST;
                if (soundMode != 2) sendBeep(400); 
                setAllPucks(EFF_BREATHE_MOD4, CRGB::Red, 50, 100);
            } else {
                gameState = TABATA_WORK;
                rounds++;
                if (soundMode != 2) sendBeep(400); 
            }
        } 
        else {
            if (soundMode == 0) {
                if (timeLeft <= 3000 && warningStep == 0) { sendBeep(100); warningStep = 1; }
                else if (timeLeft <= 2000 && warningStep == 1) { sendBeep(100); warningStep = 2; }
                else if (timeLeft <= 1000 && warningStep == 2) { sendBeep(100); warningStep = 3; }
            }

            if (gameState == TABATA_WORK) {
                int fill = (timeLeft * 255) / workTimeMs;
                if (fill < 0) fill = 0;
                if (fill > 255) fill = 255;
                
                if (fill != lastFill && (now - lastTxTime > 200)) {
                    lastFill = fill;
                    lastTxTime = now;
                    setAllPucks(EFF_PROGRESS, CRGB::Green, fill, 150);
                }
            }
        }
    }
}

void Game_Tabata::handleEvent(int puckIndex, EventPacket event) {
}

void Game_Tabata::setAllPucks(int effect, CRGB color, int speed, int bright) {
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::broadcast(cp);
}

void Game_Tabata::sendBeep(int duration) {
    CommandPacket snd; memset(&snd, 0, sizeof(snd)); snd.cmd = CMD_SOUND; snd.duration = duration;
    PuckNetwork::broadcast(snd);
}

String Game_Tabata::getStatusJSON() {
    String json = "{";
    json += "\"st\":" + String(gameState) + ","; 
    
    // Wenn pausiert, nutzen wir die pauseBeginTime statt millis() um die UI-Uhr einzufrieren
    unsigned long referenceTime = (gameState == TABATA_PAUSED) ? pauseBeginTime : millis();
    
    long gT = 0;
    if (gameState >= TABATA_WORK && gameState != TABATA_FINISHED) gT = referenceTime - globalStartTime;
    json += "\"gT\":" + String(gT) + ",";
    
    long pT = 0;
    if (gameState == TABATA_WORK || (gameState == TABATA_PAUSED && prePauseState == TABATA_WORK)) {
        pT = workTimeMs - (referenceTime - phaseStartTime);
    }
    else if (gameState == TABATA_REST || (gameState == TABATA_PAUSED && prePauseState == TABATA_REST)) {
        pT = restTimeMs - (referenceTime - phaseStartTime);
    }
    if (pT < 0) pT = 0;
    json += "\"pT\":" + String(pT) + ",";
    
    json += "\"rnd\":" + String(rounds) + ",";
    json += "\"lM\":" + String(limitMode) + ",";
    json += "\"tR\":" + String(targetRounds) + ",";
    json += "\"tT\":" + String(targetTimeMs / 1000) + ",";
    json += "\"wT\":" + String(workTimeMs / 1000) + ",";
    json += "\"rT\":" + String(restTimeMs / 1000);
    
    json += "}";
    return json;
}