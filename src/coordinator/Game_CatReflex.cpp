#include "Game_CatReflex.h"
#include "PuckNetwork.h"

void Game_CatReflex::setup() {
    Serial.println("GAME: Setup Cat Reflex");
    state = CR_SETUP;
    finishTime = 0;
    
    // Reset aller Statistiken
    for(int i=0; i<MAX_PEERS; i++) {
        points[i] = 0;
        reactTimes[i] = 0;
        roundEarly[i] = false;
        roundPressed[i] = false;
        isRoundWinner[i] = false;
    }
}

void Game_CatReflex::processCommand(String cmd, int value) {
    if (cmd == "setup") {
        resetGame();
        // Anzeigepuck (Index 0) zeigt Rainbow
        setPuck(0, EFF_RAINBOW, CRGB::Black, 50, 200);
        delay(15);
        // Spielerpucks (Index 1 bis playerCount) atmen in Lila (Purple)
        for(int p=1; p<=playerCount; p++) {
            setPuck(p, EFF_BREATHE_MOD4, PLAYER_COLORS[p-1], 40, 150);
            delay(15); 
        }
    }
    else if (cmd == "config_players") playerCount = value;
    else if (cmd == "config_highlander") highlanderMode = (value == 1);
    else if (cmd == "config_time") gameDurationSec = value;
    else if (cmd == "config_diff") difficultyMs = value;
    else if (cmd == "config_chaos") colorChaos = (value == 1);
    else if (cmd == "start") startSequence(true);
    else if (cmd == "stop") stopGame();
    else if (cmd == "reset") resetGame();
    else if (cmd == "exit") exitGame();
}

void Game_CatReflex::startSequence(bool forceRestart) {
    if (!forceRestart && state != CR_SETUP && state != CR_FINISHED) return;
    
    Serial.println("GAME: Cat Reflex Start Countdown");
    state = CR_START_COUNTDOWN;
    stateStartTime = millis();
    gameStartTime = millis();
    finishTime = 0;
    
    // Punkte nullen
    for(int p=0; p<=playerCount; p++) points[p] = 0;
    
    // Anzeigepuck spielt Countdown Sound
    CommandPacket snd; memset(&snd, 0, sizeof(snd));
    snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_SKI;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[0].mac, snd);
    
    // Alle Spielerpucks in Startposition (Single Chase) in ihrer zugewiesenen Farbe
    for(int p=1; p<=playerCount; p++) {
        setPuck(p, EFF_SINGLE_CHASE, PLAYER_COLORS[p-1], 50, 100);
        delay(15);
    }
}

void Game_CatReflex::loop() {
    unsigned long now = millis();

    // Globale Zeitueberwachung (nur wenn das Spiel aktiv laeuft)
    if (state != CR_SETUP && state != CR_FINISHED) {
        if (now - gameStartTime >= (gameDurationSec * 1000UL)) {
            stopGame();
            return;
        }
    }

    switch (state) {
        case CR_START_COUNTDOWN:
            // Nach 3.1s (Laenge des SEQ_SKI Sounds) beginnt die eigentliche Runde
            if (now - stateStartTime >= 3100) {
                state = CR_WAIT_GREEN;
                stateStartTime = now;
                greenDelayMs = random(1000, 3001); // 1-3 Sekunden Wartezeit
                setPuck(0, EFF_OFF, CRGB::Black);  // Anzeigepuck aus
                
                // Runden-Tracking fuer Spieler resetten
                for(int p=1; p<=playerCount; p++) {
                    roundEarly[p] = false;
                    roundPressed[p] = false;
                    reactTimes[p] = 0;
                    isRoundWinner[p] = false;
                }
            }
            break;

        case CR_WAIT_GREEN:
            // Farbchaos Logik (alle 300ms Farbe wechseln)
            if (colorChaos && (now - lastChaosTime > 300)) {
                lastChaosTime = now;
                setPuck(0, EFF_STATIC, getRandomChaosColor(), 0, 255);
            }

            // Zeit ist reif fuer das gruene Signal
            if (now - stateStartTime >= greenDelayMs) {
                state = CR_GREEN_ACTIVE;
                stateStartTime = now;
                greenSignalTime = now;
                setPuck(0, EFF_STATIC, CRGB::Green, 0, 255); // GRUEN!
            }
            break;

        case CR_GREEN_ACTIVE: {
            // Scope in geschweifte Klammern gesetzt, um Kompilierungsfehler zu vermeiden
            // Runde beenden, wenn 2.5 Sekunden lang nichts passiert ist
            // Oder wenn alle noch aktiven (nicht zu frueh gedrueckten) Spieler gedrueckt haben
            bool allDone = true;
            for(int p=1; p<=playerCount; p++) {
                if (!roundEarly[p] && !roundPressed[p]) allDone = false;
            }

            if (allDone || (now - stateStartTime >= 2500)) {
                evaluateRound();
            }
            break;
        }

        case CR_ROUND_RESULT:
            // 3 Sekunden Pause zur Ergebnisanzeige im Frontend
            if (now - stateStartTime >= 3000) {
                // Neustart der Runde
                state = CR_WAIT_GREEN;
                stateStartTime = now;
                greenDelayMs = random(1000, 3001);
                setPuck(0, EFF_OFF, CRGB::Black); // Anzeigepuck wieder aus
                
                // Spielerpucks zurueck auf Standard Chase
                for(int p=1; p<=playerCount; p++) {
                    roundEarly[p] = false;
                    roundPressed[p] = false;
                    reactTimes[p] = 0;
                    isRoundWinner[p] = false;
                    setPuck(p, EFF_SINGLE_CHASE, PLAYER_COLORS[p-1], 50, 100);
                    delay(10);
                }
                
                // Kurzer Beep fuer naechste Runde auf Anzeigepuck
                CommandPacket snd; memset(&snd, 0, sizeof(snd));
                snd.cmd = CMD_SOUND; snd.duration = 200;
                PuckNetwork::sendToPuck(PuckNetwork::getPucks()[0].mac, snd);
            }
            break;

        default:
            break;
    }
}

void Game_CatReflex::handleEvent(int puckIndex, EventPacket event) {
    if (event.type != EVT_BTN_CLICK) return;
    
    // Puck 0 ist der Anzeigepuck, Eingaben hier ignorieren
    if (puckIndex == 0) return;
    
    // Sicherstellen, dass der Index im Rahmen bleibt
    if (puckIndex > playerCount) return;

    if (state == CR_WAIT_GREEN) {
        // ZU FRUEH GEDRUECKT (Fehlstart)
        if (!roundEarly[puckIndex]) {
            roundEarly[puckIndex] = true;
            points[puckIndex]--; // Strafpunkt
            
            // Visuelles und akustisches Feedback fuer den Fehler
            setPuck(puckIndex, EFF_FAIL, CRGB::Red, 0, 255);
            CommandPacket snd; memset(&snd, 0, sizeof(snd));
            snd.cmd = CMD_SOUND; snd.duration = 150;
            PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
            
            // Pruefen, ob ALLE zu frueh gedrueckt haben -> Abbruch der Runde
            bool allEarly = true;
            for(int p=1; p<=playerCount; p++) {
                if (!roundEarly[p]) allEarly = false;
            }
            if (allEarly) {
                evaluateRound(); // Beendet die Runde ohne Gewinner
            }
        }
    } 
    else if (state == CR_GREEN_ACTIVE) {
        // RICHTIG GEDRUECKT
        if (!roundEarly[puckIndex] && !roundPressed[puckIndex]) {
            roundPressed[puckIndex] = true;
            reactTimes[puckIndex] = millis() - greenSignalTime;
            
            // Visuelles Feedback (Puck leuchtet statisch in seiner Farbe)
            setPuck(puckIndex, EFF_STATIC, PLAYER_COLORS[puckIndex-1], 0, 255);
            
            // Sound-Feedback
            CommandPacket snd; memset(&snd, 0, sizeof(snd));
            snd.cmd = CMD_SOUND; snd.duration = 100;
            PuckNetwork::sendToPuck(PuckNetwork::getPucks()[puckIndex].mac, snd);
        }
    }
}

void Game_CatReflex::evaluateRound() {
    state = CR_ROUND_RESULT;
    stateStartTime = millis();
    setPuck(0, EFF_OFF, CRGB::Black); // Anzeigepuck aus
    
    // Finde die schnellste Zeit
    unsigned long bestTime = 999999;
    for(int p=1; p<=playerCount; p++) {
        if (roundPressed[p] && reactTimes[p] < bestTime) {
            bestTime = reactTimes[p];
        }
    }

    // Punkte verteilen
    if (bestTime < 999999) { // Mindestens einer hat regulaer gedrueckt
        for(int p=1; p<=playerCount; p++) {
            if (roundPressed[p]) {
                if (highlanderMode) {
                    // Highlander: Nur die Schnellsten (bestTime) erhalten einen Punkt
                    if (reactTimes[p] == bestTime) {
                        points[p]++;
                        isRoundWinner[p] = true;
                    }
                } else {
                    // Normaler Modus: Innerhalb des Toleranzfensters?
                    if (reactTimes[p] <= difficultyMs) {
                        points[p]++; // Punkt fuer rechtzeitiges Druecken
                        if (reactTimes[p] == bestTime) {
                            points[p]++; // Extrapunkt fuer den Schnellsten
                            isRoundWinner[p] = true;
                        }
                    }
                }
            }
        }
    }
}

void Game_CatReflex::stopGame() {
    state = CR_FINISHED;
    finishTime = millis();
    
    // Finde den globalen Gewinner (hoechste Punktzahl)
    int maxPoints = -9999;
    for(int p=1; p<=playerCount; p++) {
        if(points[p] > maxPoints) maxPoints = points[p];
    }

    // Abschluss-Animation: Anzeigepuck BREATHE_MOD4, Gewinner RAINBOW, Rest BREATHE_MOD4
    setPuck(0, EFF_BREATHE_MOD4, CRGB::Purple, 50, 200);
    delay(15);
    for(int p=1; p<=playerCount; p++) {
        if(points[p] == maxPoints && maxPoints > 0) {
            setPuck(p, EFF_RAINBOW, CRGB::Black, 50, 200); // Gewinner
        } else {
            setPuck(p, EFF_BREATHE_MOD4, PLAYER_COLORS[p-1], 40, 150); // Verlierer
        }
        delay(15);
    }
    
    // Fanfare auf dem Anzeigepuck
    CommandPacket snd; memset(&snd, 0, sizeof(snd));
    snd.cmd = CMD_SEQUENCE; snd.extra = SEQ_FANFARE;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[0].mac, snd);
}

void Game_CatReflex::resetGame() {
    setup(); 
}

void Game_CatReflex::exitGame() {
    state = CR_SETUP;
    CommandPacket light; memset(&light, 0, sizeof(light));
    light.cmd = CMD_EFFECT; light.effectID = EFF_STATUS; 
    light.r = 0; light.g = 255; light.b = 0; 
    light.extra = 255; light.duration = 0; 
    PuckNetwork::broadcast(light);
}

void Game_CatReflex::setPuck(int puckIndex, int effect, CRGB color, int speed, int bright) {
    if(puckIndex < 0 || puckIndex >= MAX_PEERS) return;
    PuckInfo* p = PuckNetwork::getPucks();
    if(!p[puckIndex].active) return;
    
    CommandPacket cp; memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(p[puckIndex].mac, cp);
}

CRGB Game_CatReflex::getRandomChaosColor() {
    // Vermeide Gruen explizit, nutze deutliche Kontraste
    CRGB chaosColors[] = {CRGB::Red, CRGB::Blue, CRGB::Yellow, CRGB::Magenta, CRGB::Cyan, CRGB::Orange};
    int idx = random(0, 6);
    return chaosColors[idx];
}

String Game_CatReflex::getStatusJSON() {
    String json = "{";
    json += "\"state\":" + String(state) + ",";
    json += "\"highlander\":" + String(highlanderMode ? 1 : 0) + ",";
    json += "\"difficulty\":" + String(difficultyMs) + ",";
    
    long elapsed = 0;
    if (state != CR_SETUP && state != CR_FINISHED) {
        elapsed = millis() - gameStartTime;
    } else if (state == CR_FINISHED) {
        elapsed = finishTime - gameStartTime;
    }
    
    // Kappe die Zeit bei 0, falls Timer rueckwaerts laeuft (im Frontend)
    long remaining = (gameDurationSec * 1000L) - elapsed;
    if (remaining < 0) remaining = 0;
    json += "\"timeLeft\":" + String(remaining) + ",";
    
    json += "\"players\":[";
    for(int p=1; p<=playerCount; p++) { // Achtung: Array beginnt ab 1 im JSON (fuer Frontend-Index 0)
        if(p>1) json += ",";
        json += "{";
        json += "\"points\":" + String(points[p]) + ",";
        json += "\"reactTime\":" + String(reactTimes[p]) + ",";
        json += "\"early\":" + String(roundEarly[p] ? 1 : 0) + ",";
        json += "\"pressed\":" + String(roundPressed[p] ? 1 : 0) + ",";
        json += "\"winner\":" + String(isRoundWinner[p] ? 1 : 0);
        json += "}";
    }
    json += "]}";
    return json;
}