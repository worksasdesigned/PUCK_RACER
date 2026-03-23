#include "Game_Pitstop.h"
#include "PuckNetwork.h"

void Game_Pitstop::setup() {
    Serial.println("GAME: Setup Pitstop");
    initGame();
}

void Game_Pitstop::processCommand(String cmd, int value) {
    if      (cmd == "cfg_players")       numPlayers        = constrain(value, 1, MAX_PS_PLAYERS);
    else if (cmd == "cfg_time")          pitstopMs         = (unsigned long)value * 500UL;
    else if (cmd == "cfg_pitlane")       pitlaneMode       = (value == 1);
    else if (cmd == "cfg_countdown")     showCountdown     = (value == 1);
    else if (cmd == "cfg_alwyellow")     alwaysYellow      = (value == 1);
    else if (cmd == "cfg_statuspuck_en") statusPuckEnabled = (value == 1);
    else if (cmd == "cfg_statuspuck")    statusPuckIdx     = value;
    else if (cmd == "cfg_exittime")      pitlaneExitMs     = (unsigned long)value * 500UL;
    else if (cmd == "cfg_exitbtn")       exitButtonMode    = (value == 1);
    else if (cmd == "cfg_entry") {
        int slotIdx = value / 100;
        int puckIdx = value % 100;
        if (slotIdx >= 0 && slotIdx < MAX_PS_PLAYERS) slots[slotIdx].entryPuckIdx = puckIdx;
    }
    else if (cmd == "cfg_exit") {
        int slotIdx = value / 100;
        int puckIdx = value % 100;
        if (slotIdx >= 0 && slotIdx < MAX_PS_PLAYERS) slots[slotIdx].exitPuckIdx = puckIdx;
    }
    else if (cmd == "setup") initGame();
    else if (cmd == "show_track") {
        for (int i = 0; i < numPlayers; i++) {
            CRGB col = PLAYER_COLORS[i % 10];
            if (slots[i].entryPuckIdx >= 0)
                setPuck(slots[i].entryPuckIdx, EFF_SINGLE_CHASE, col, 20, 220);
            if (pitlaneMode && slots[i].exitPuckIdx >= 0)
                setPuck(slots[i].exitPuckIdx, EFF_BREATHE_MOD4, col, 10, 80);
            delay(10);
        }
        if (statusPuckEnabled && statusPuckIdx >= 0)
            setPuck(statusPuckIdx, EFF_RAINBOW, CRGB::Black, 10, 220);
    }
    else if (cmd == "start") {
        if (gameState == PS_SETUP || gameState == PS_FINISHED) {
            gameState = PS_RUNNING;
            totalStops = 0;
            lastStatusState = 0;
            lastBlinkToggle = millis();
            blinkStateOn = true;
            
            for (int i = 0; i < numPlayers; i++) {
                slots[i].stopCount = 0;
                slots[i].entryRunning = false;
                slots[i].exitRunning = false;
                slots[i].exitPending = false;
                slots[i].exitPendingStart = 0;
                slots[i].entryDoneAt = 0;
                slots[i].exitDoneAt = 0;
                slots[i].exitWaitingForButton = false;
                if (slots[i].isActive) restoreSlotVisuals(i);
            }
            updateStatusPuck();
        }
    }
    else if (cmd == "pause") {
        if (gameState == PS_RUNNING) {
            gameState = PS_PAUSED;
            pauseStartMs = millis();
            for (int i = 0; i < numPlayers; i++) {
                if (!slots[i].isActive) continue;
                if (slots[i].entryPuckIdx >= 0) setPuck(slots[i].entryPuckIdx, EFF_STATIC, CRGB::Red, 0, 200);
                if (pitlaneMode && slots[i].exitPuckIdx >= 0) setPuck(slots[i].exitPuckIdx, EFF_STATIC, CRGB::Red, 0, 200);
            }
            updateStatusPuck();
        } else if (gameState == PS_PAUSED) {
            unsigned long pausedMs = millis() - pauseStartMs;
            for (int i = 0; i < numPlayers; i++) {
                if (slots[i].entryRunning) slots[i].entryStart += pausedMs;
                if (slots[i].exitRunning)  slots[i].exitStart  += pausedMs;
                if (slots[i].exitPending)  slots[i].exitPendingStart += pausedMs; // Hintergrundtimer ebenfalls pausieren!
                if (slots[i].entryDoneAt > 0) slots[i].entryDoneAt += pausedMs;
                if (slots[i].exitDoneAt > 0)  slots[i].exitDoneAt  += pausedMs;
            }
            gameState = PS_RUNNING;
            for (int i = 0; i < numPlayers; i++) {
                if (slots[i].isActive) restoreSlotVisuals(i);
            }
            updateStatusPuck();
        }
    }
    else if (cmd == "stop") {
        gameState = PS_FINISHED;
        for (int i = 0; i < numPlayers; i++) {
            slots[i].entryRunning = false;
            slots[i].exitRunning = false;
            slots[i].exitPending = false;
            slots[i].exitPendingStart = 0;
            slots[i].exitWaitingForButton = false;
            if (slots[i].entryPuckIdx >= 0) setPuck(slots[i].entryPuckIdx, EFF_STATIC, CRGB::Red, 0, 100);
            if (pitlaneMode && slots[i].exitPuckIdx >= 0) setPuck(slots[i].exitPuckIdx, EFF_STATIC, CRGB::Red, 0, 100);
        }
        if (statusPuckEnabled && statusPuckIdx >= 0) setPuck(statusPuckIdx, EFF_STATIC, CRGB::Red, 0, 220);
        lastStatusState = 0;
    }
    else if (cmd == "reset") {
        for (int i = 0; i < MAX_PS_PLAYERS; i++) {
            slots[i].entryPuckIdx = -1;
            slots[i].exitPuckIdx  = -1;
        }
        initGame();
    }
    else if (cmd == "exit") {
        CommandPacket cp;
        memset(&cp, 0, sizeof(cp));
        cp.cmd = CMD_EFFECT; cp.effectID = EFF_STATUS;
        cp.r = 0; cp.g = 255; cp.b = 0; cp.extra = 255;
        PuckNetwork::broadcast(cp);
        gameState = PS_SETUP;
    }
    else if (cmd == "penalty") {
        int slotIdx = value / 100;
        int seconds = value % 100;
        if (slotIdx >= 0 && slotIdx < numPlayers) {
            unsigned long addMs = (unsigned long)(seconds * 1000);
            slots[slotIdx].penaltyMs += (int)addMs;
            
            if (slots[slotIdx].entryRunning) {
                slots[slotIdx].entryDuration += addMs;
                if (showCountdown && slots[slotIdx].entryPuckIdx >= 0) {
                    unsigned long elapsed = millis() - slots[slotIdx].entryStart;
                    unsigned long rem = (slots[slotIdx].entryDuration > elapsed) ? slots[slotIdx].entryDuration - elapsed : 0;
                    int spd = constrain((int)(rem / 35), 1, 1500);
                    setPuck(slots[slotIdx].entryPuckIdx, EFF_COUNTDOWN, CRGB::Red, spd, 255);
                }
            }
        }
    }
    else if (cmd == "clr_pen") {
        int slotIdx = value;
        if (slotIdx >= 0 && slotIdx < numPlayers && !slots[slotIdx].entryRunning) {
            slots[slotIdx].penaltyMs = 0; 
        }
    }
    else if (cmd == "toggle_active") {
        int slotIdx = value;
        if (slotIdx >= 0 && slotIdx < numPlayers) {
            slots[slotIdx].isActive = !slots[slotIdx].isActive;
            if (gameState == PS_RUNNING) {
                if (slots[slotIdx].isActive) {
                    restoreSlotVisuals(slotIdx);
                } else {
                    slots[slotIdx].entryRunning = false;
                    slots[slotIdx].exitRunning = false;
                    slots[slotIdx].exitPending = false;
                    slots[slotIdx].exitPendingStart = 0;
                    slots[slotIdx].exitWaitingForButton = false;
                    if (slots[slotIdx].entryPuckIdx >= 0) setPuck(slots[slotIdx].entryPuckIdx, EFF_STATIC, CRGB::Red, 0, 85);
                    if (pitlaneMode && slots[slotIdx].exitPuckIdx >= 0) setPuck(slots[slotIdx].exitPuckIdx, EFF_STATIC, CRGB::Red, 0, 85);
                }
                updateStatusPuck();
            }
        }
    }
}

void Game_Pitstop::initGame() {
    PuckInfo* netPucks = PuckNetwork::getPucks();
    int activePucks[MAX_PEERS];
    int activePuckCount = 0;
    
    for (int i = 0; i < MAX_PEERS; i++) {
        if (netPucks[i].active) activePucks[activePuckCount++] = i;
    }

    totalStops = 0;
    for (int i = 0; i < MAX_PS_PLAYERS; i++) {
        slots[i].entryRunning = false;
        slots[i].exitRunning  = false;
        slots[i].exitPending  = false;
        slots[i].exitPendingStart = 0;
        slots[i].stopCount    = 0;
        slots[i].isActive     = true;
        slots[i].penaltyMs    = 0;
        slots[i].entryStart   = 0;
        slots[i].entryDuration = 0;
        slots[i].entryDoneAt  = 0;
        slots[i].exitStart    = 0;
        slots[i].exitDoneAt   = 0;
        slots[i].exitWaitingForButton = false;

        if (slots[i].entryPuckIdx < 0 && i < numPlayers) {
            if (pitlaneMode) {
                slots[i].entryPuckIdx = (i * 2 < activePuckCount) ? activePucks[i * 2] : -1;
                slots[i].exitPuckIdx  = (i * 2 + 1 < activePuckCount) ? activePucks[i * 2 + 1] : -1;
            } else {
                slots[i].entryPuckIdx = (i < activePuckCount) ? activePucks[i] : -1;
                slots[i].exitPuckIdx  = -1;
            }
        }
    }
    gameState = PS_SETUP;
}

void Game_Pitstop::triggerEntry(int slotIdx) {
    PitstopSlot& slot = slots[slotIdx];
    unsigned long dur = pitstopMs + (unsigned long)slot.penaltyMs;
    
    slot.entryRunning  = true;
    slot.entryStart    = millis();
    slot.entryDuration = dur;
    slot.entryDoneAt   = 0;
    
    if (slot.entryPuckIdx >= 0) {
        if (showCountdown) {
            int spd = constrain((int)(dur / 35), 1, 1500);
            setPuck(slot.entryPuckIdx, EFF_COUNTDOWN, CRGB::Red, spd, 255);
        } else {
            setPuck(slot.entryPuckIdx, EFF_DOUBLE_CHASE, CRGB::Red, 20, 255);
        }
        sendSound(slot.entryPuckIdx, 50);
    }

    if (pitlaneMode && slot.exitPuckIdx >= 0) {
        if (!slot.exitRunning && !slot.exitWaitingForButton && slot.exitDoneAt == 0 && !slot.exitPending) {
            setPuck(slot.exitPuckIdx, EFF_DOUBLE_CHASE, CRGB::Red, 20, 200);
        }
    }

    updateStatusPuck();
}

void Game_Pitstop::completeEntry(int slotIdx) {
    PitstopSlot& slot = slots[slotIdx];
    slot.entryRunning = false;
    slot.penaltyMs = 0; 
    
    if (!pitlaneMode) {
        totalStops++;
        slot.stopCount++;
        slot.entryDoneAt = millis(); 
        if (slot.entryPuckIdx >= 0) {
            sendSequence(slot.entryPuckIdx, SEQ_DINGDONG);
            setPuck(slot.entryPuckIdx, EFF_DOUBLE_CHASE, CRGB::Green, 20, 255);
        }
    } else {
        slot.entryDoneAt = millis();
        if (slot.entryPuckIdx >= 0) {
            setPuck(slot.entryPuckIdx, EFF_DOUBLE_CHASE, CRGB::Green, 20, 255);
        }
        
        if (!slot.exitRunning && !slot.exitWaitingForButton && slot.exitDoneAt == 0 && !slot.exitPending) {
            startExitCountdown(slotIdx, 0); // Niemand steht im Weg, Countdown normal bei 0 starten
        } else {
            slot.exitPending = true;
            slot.exitPendingStart = millis(); // Hintergrundtimer startet! Fahrer fährt schon den Gang hinunter.
        }
    }
    updateStatusPuck();
}

// NIMMT NUN ELAPSED ALS OFFSET!
void Game_Pitstop::startExitCountdown(int slotIdx, unsigned long elapsed) {
    PitstopSlot& slot = slots[slotIdx];
    slot.exitRunning = true;
    slot.exitStart = millis() - elapsed; // Schiebt den internen Start-Zeitpunkt in die Vergangenheit
    slot.exitDoneAt = 0;
    slot.exitWaitingForButton = false;

    if (slot.exitPuckIdx >= 0) {
        if (showCountdown) {
            unsigned long rem = (elapsed < pitlaneExitMs) ? pitlaneExitMs - elapsed : 0;
            int spd = constrain((int)(rem / 35), 1, 1500);
            setPuck(slot.exitPuckIdx, EFF_COUNTDOWN, CRGB::Red, spd, 220);
        } else {
            setPuck(slot.exitPuckIdx, EFF_DOUBLE_CHASE, CRGB::Red, 20, 200);
        }
        sendSound(slot.exitPuckIdx, 50);
    }
}

void Game_Pitstop::completeExit(int slotIdx) {
    PitstopSlot& slot = slots[slotIdx];
    slot.exitRunning = false;
    slot.exitWaitingForButton = false;
    
    totalStops++;
    slot.stopCount++;
    slot.exitDoneAt = millis(); 
    
    if (slot.exitPuckIdx >= 0) {
        sendSequence(slot.exitPuckIdx, SEQ_DINGDONG);
        setPuck(slot.exitPuckIdx, EFF_DOUBLE_CHASE, CRGB::Green, 20, 255);
    }
}

void Game_Pitstop::handleBlinking() {
    unsigned long now = millis();
    unsigned long interval = blinkStateOn ? 333 : 666; 

    if (now - lastBlinkToggle >= interval) {
        blinkStateOn = !blinkStateOn;
        lastBlinkToggle = now;

        for (int i = 0; i < numPlayers; i++) {
            if (!slots[i].isActive) continue;
            
            if (!slots[i].entryRunning && slots[i].entryDoneAt == 0 && slots[i].entryPuckIdx >= 0) {
                setSlotIdleVisual(i, slots[i].entryPuckIdx);
            }
            
            if (pitlaneMode && !slots[i].exitRunning && !slots[i].exitWaitingForButton && slots[i].exitDoneAt == 0 && slots[i].exitPuckIdx >= 0) {
                if (!slots[i].exitPending && !slots[i].entryRunning && slots[i].entryDoneAt == 0) {
                    setSlotIdleVisual(i, slots[i].exitPuckIdx);
                }
            }
        }
    }
}

void Game_Pitstop::setSlotIdleVisual(int slotIdx, int puckIdx) {
    CRGB col = alwaysYellow ? CRGB::Yellow : PLAYER_COLORS[slotIdx % 10];
    if (blinkStateOn) {
        setPuck(puckIdx, EFF_STATIC, col, 0, 200);
    } else {
        setPuck(puckIdx, EFF_STATIC, CRGB::Black, 0, 0);
    }
}

void Game_Pitstop::loop() {
    if (gameState != PS_RUNNING) return;

    handleBlinking();

    for (int i = 0; i < numPlayers; i++) {
        PitstopSlot& slot = slots[i];
        if (!slot.isActive) continue;

        if (slot.entryRunning && (millis() - slot.entryStart >= slot.entryDuration)) {
            completeEntry(i);
        }

        if (!slot.entryRunning && slot.entryDoneAt > 0 && (millis() - slot.entryDoneAt >= 2000)) {
            slot.entryDoneAt = 0;
            updateStatusPuck(); 
            if (slot.entryPuckIdx >= 0) setSlotIdleVisual(i, slot.entryPuckIdx);
        }

        if (pitlaneMode && slot.exitRunning && (millis() - slot.exitStart >= pitlaneExitMs)) {
            if (exitButtonMode) {
                slot.exitRunning = false;
                slot.exitWaitingForButton = true;
                if (slot.exitPuckIdx >= 0) setPuck(slot.exitPuckIdx, EFF_DOUBLE_CHASE, CRGB::Yellow, 20, 255);
            } else {
                completeExit(i);
            }
        }

        if (pitlaneMode && !slot.exitRunning && !slot.exitWaitingForButton && slot.exitDoneAt > 0 && (millis() - slot.exitDoneAt >= 2000)) {
            slot.exitDoneAt = 0;
            
            if (slot.exitPending) {
                slot.exitPending = false;
                unsigned long elapsed = millis() - slot.exitPendingStart; // Zeit abziehen, die Fahrer 2 schon gefahren ist!
                startExitCountdown(i, elapsed);
            } else {
                if (slot.exitPuckIdx >= 0) {
                    if (!slot.entryRunning && slot.entryDoneAt == 0) {
                        setSlotIdleVisual(i, slot.exitPuckIdx);
                    } else {
                        setPuck(slot.exitPuckIdx, EFF_DOUBLE_CHASE, CRGB::Red, 20, 200);
                    }
                }
            }
        }
    }
}

void Game_Pitstop::handleEvent(int puckIndex, EventPacket event) {
    if (gameState != PS_RUNNING) return;
    if (event.type != EVT_BTN_CLICK) return;

    for (int i = 0; i < numPlayers; i++) {
        if (!slots[i].isActive) continue;

        if (puckIndex == slots[i].entryPuckIdx && !slots[i].entryRunning && slots[i].entryDoneAt == 0) {
            triggerEntry(i);
            return;
        }

        if (exitButtonMode && pitlaneMode && puckIndex == slots[i].exitPuckIdx) {
            if (slots[i].exitWaitingForButton) {
                completeExit(i);
                return;
            }
        }
    }
}

String Game_Pitstop::getStatusJSON() {
    String json = "{";
    json += "\"st\":"    + String(gameState)            + ",";
    json += "\"total\":" + String(totalStops)           + ",";
    json += "\"pit\":"   + String(pitlaneMode ? 1 : 0)  + ",";
    json += "\"slots\":[";
    
    for (int i = 0; i < numPlayers; i++) {
        if (i > 0) json += ",";
        PitstopSlot& s = slots[i];

        long erem = 0, xrem = 0;
        if (s.entryRunning) {
            unsigned long elapsed = millis() - s.entryStart;
            erem = (long)s.entryDuration - (long)elapsed;
            if (erem < 0) erem = 0;
        }
        if (pitlaneMode) {
            if (s.exitRunning) {
                unsigned long elapsed = millis() - s.exitStart;
                xrem = (long)pitlaneExitMs - (long)elapsed;
            } else if (s.exitPending) {
                // Die ablaufende Zeit wird nun auch im Pending-Modus berechnet, damit die UI sie anzeigen kann
                unsigned long elapsed = millis() - s.exitPendingStart;
                xrem = (long)pitlaneExitMs - (long)elapsed;
            }
            if (xrem < 0) xrem = 0;
        }

        json += "{";
        json += "\"entr\":"  + String(s.entryRunning ? 1 : 0)               + ",";
        json += "\"edone\":" + String(s.entryDoneAt > 0 ? 1 : 0)            + ",";
        json += "\"exit\":"  + String((pitlaneMode && s.exitRunning) ? 1 : 0) + ",";
        json += "\"xdone\":" + String(s.exitDoneAt > 0 ? 1 : 0)             + ",";
        json += "\"erem\":"  + String(erem)                                  + ",";
        json += "\"xrem\":"  + String(xrem)                                  + ",";
        json += "\"sc\":"    + String(s.stopCount)                           + ",";
        json += "\"act\":"   + String(s.isActive ? 1 : 0)                   + ",";
        json += "\"ewfb\":"  + String(s.exitWaitingForButton ? 1 : 0)       + ",";
        json += "\"pend\":"  + String(s.exitPending ? 1 : 0)                + ",";
        json += "\"pen\":"   + String(s.penaltyMs);
        json += "}";
    }
    json += "]}";
    return json;
}

void Game_Pitstop::updateStatusPuck() {
    if (!statusPuckEnabled || statusPuckIdx < 0) return;
    if (gameState != PS_RUNNING && gameState != PS_PAUSED) return;

    // State 1 = Grün (mindestens eine Box frei)
    // State 2 = Rot  (Pause oder alle Lanes deaktiviert)
    // State 3 = Gelb (alle Boxen belegt, aber Lanes aktiv & Spiel läuft)
    int newState = 2;
    if (gameState == PS_RUNNING) {
        bool anyOpen = false;
        bool anyActive = false;
        for (int i = 0; i < numPlayers; i++) {
            if (!slots[i].isActive) continue;
            anyActive = true;
            if (!slots[i].entryRunning && slots[i].entryDoneAt == 0) {
                anyOpen = true;
                break;
            }
        }
        if (anyOpen)       newState = 1; // Grün: mindestens eine Box frei
        else if (anyActive) newState = 3; // Gelb: alle belegt, aber Spiel läuft
        else                newState = 2; // Rot:  alle deaktiviert
    }

    if (newState == lastStatusState) return;
    lastStatusState = newState;
    CRGB col;
    if      (newState == 1) col = CRGB::Green;
    else if (newState == 3) col = CRGB::Yellow;
    else                    col = CRGB::Red;
    setPuck(statusPuckIdx, EFF_DOUBLE_CHASE, col, 20, 220);
}

void Game_Pitstop::restoreSlotVisuals(int slotIdx) {
    PitstopSlot& slot = slots[slotIdx];
    
    if (slot.entryPuckIdx >= 0) {
        if (slot.entryRunning) {
            if (showCountdown) {
                unsigned long rem = 0;
                unsigned long elapsed = millis() - slot.entryStart;
                if (elapsed < slot.entryDuration) rem = slot.entryDuration - elapsed;
                int spd = constrain((int)(rem / 35), 1, 1500);
                setPuck(slot.entryPuckIdx, EFF_COUNTDOWN, CRGB::Red, spd, 255);
            } else {
                setPuck(slot.entryPuckIdx, EFF_DOUBLE_CHASE, CRGB::Red, 20, 255);
            }
        } else if (slot.entryDoneAt > 0) {
            setPuck(slot.entryPuckIdx, EFF_DOUBLE_CHASE, CRGB::Green, 20, 255);
        } else {
            setSlotIdleVisual(slotIdx, slot.entryPuckIdx);
        }
    }

    if (pitlaneMode && slot.exitPuckIdx >= 0) {
        if (slot.exitWaitingForButton) {
            setPuck(slot.exitPuckIdx, EFF_DOUBLE_CHASE, CRGB::Yellow, 20, 255);
        } else if (slot.exitRunning) {
            if (showCountdown) {
                unsigned long elapsed = millis() - slot.exitStart;
                unsigned long rem = (elapsed < pitlaneExitMs) ? pitlaneExitMs - elapsed : 0;
                int spd = constrain((int)(rem / 35), 1, 1500);
                setPuck(slot.exitPuckIdx, EFF_COUNTDOWN, CRGB::Red, spd, 220);
            } else {
                setPuck(slot.exitPuckIdx, EFF_DOUBLE_CHASE, CRGB::Red, 20, 200);
            }
        } else if (slot.exitDoneAt > 0) {
            setPuck(slot.exitPuckIdx, EFF_DOUBLE_CHASE, CRGB::Green, 20, 255);
        } else {
            if (!slot.exitPending && !slot.entryRunning && slot.entryDoneAt == 0) {
                setSlotIdleVisual(slotIdx, slot.exitPuckIdx);
            } else {
                setPuck(slot.exitPuckIdx, EFF_DOUBLE_CHASE, CRGB::Red, 20, 200);
            }
        }
    }
}

void Game_Pitstop::setPuck(int globalIdx, int effect, CRGB color, int speed, int bright) {
    CommandPacket cp;
    memset(&cp, 0, sizeof(cp));
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[globalIdx].mac, cp);
}

void Game_Pitstop::sendSound(int globalIdx, int duration) {
    CommandPacket snd;
    memset(&snd, 0, sizeof(snd));
    snd.cmd = CMD_SOUND; snd.duration = duration;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[globalIdx].mac, snd);
}

void Game_Pitstop::sendSequence(int globalIdx, int seqID) {
    CommandPacket seq; memset(&seq, 0, sizeof(seq));
    seq.cmd = CMD_SEQUENCE; seq.extra = seqID;
    PuckNetwork::sendToPuck(PuckNetwork::getPucks()[globalIdx].mac, seq);
}