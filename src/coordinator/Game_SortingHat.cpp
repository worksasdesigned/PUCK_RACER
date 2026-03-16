#include "Game_SortingHat.h"
#include "PuckNetwork.h"

void Game_SortingHat::setup() {
    Serial.println("GAME: Setup Sorting Hat");
    state = SORT_SETUP;
    initGame();
}

void Game_SortingHat::processCommand(String cmd, int value) {
    if (cmd == "setup") { initGame(); }
    else if (cmd == "cfg_groups") { numGroups = constrain(value, 2, 8); }
    else if (cmd == "cfg_players") { targetPlayers = constrain(value, 2, 100); }
    else if (cmd == "cfg_pucks") {
        activePucks = constrain(value, 1, 3);
        updatePuckList(); 
    }
    else if (cmd == "preview") {
        updatePuckList();
        for(int idx : puckIndices) setPuck(idx, EFF_RAINBOW, CRGB::Black, 0, 100);
    }
    else if (cmd == "start") {
        initGame(); // Reset Stats & Bag
        triggerStartSequence(); // Startet non-blocking Ablauf
    }
    else if (cmd == "stop") {
        state = SORT_FINISHED;
        for(int idx : puckIndices) setPuck(idx, EFF_OFF, CRGB::Black);
    }
    else if (cmd == "exit") {
        state = SORT_SETUP;
        CommandPacket cp; cp.cmd=CMD_EFFECT; cp.effectID=EFF_STATUS; cp.r=0; cp.g=255; cp.b=0; cp.extra=255; cp.duration=0;
        PuckNetwork::broadcast(cp);
    }
}

void Game_SortingHat::initGame() {
    assignedPlayers = 0;
    for(int i=0; i<8; i++) groupCounts[i] = 0;
    for(int i=0; i<MAX_PEERS; i++) lastTriggerTime[i] = 0;
    
    updatePuckList();
    fillTicketBag();
}

void Game_SortingHat::updatePuckList() {
    puckIndices.clear();
    PuckInfo* p = PuckNetwork::getPucks();
    int found = 0;
    for(int i=0; i<MAX_PEERS; i++) {
        if(p[i].active && found < activePucks) {
            puckIndices.push_back(i);
            found++;
        }
    }
}

void Game_SortingHat::fillTicketBag() {
    ticketBag.clear();
    for(int i=0; i<targetPlayers; i++) {
        ticketBag.push_back(i % numGroups);
    }
}

void Game_SortingHat::triggerStartSequence() {
    state = SORT_STARTING;
    startSeqStep = 0;
    startSeqTimer = millis();
    
    // Sofort: Alle Pucks weißes Chase
    for(int idx : puckIndices) {
        setPuck(idx, EFF_SINGLE_CHASE, CRGB::White, 40, 150);
    }
    // Erster Beep sofort
    CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 100;
    PuckNetwork::broadcast(snd);
}

void Game_SortingHat::loop() {
    unsigned long now = millis();

    // --- 1. NON-BLOCKING START SEQUENZ ---
    if (state == SORT_STARTING) {
        // Taktung: Alle 600ms nächster Schritt
        if (now - startSeqTimer > 600) {
            startSeqTimer = now;
            startSeqStep++;

            CommandPacket snd; snd.cmd = CMD_SOUND;
            
            if (startSeqStep == 1) { // Beep 2
                snd.duration = 100; PuckNetwork::broadcast(snd);
            }
            else if (startSeqStep == 2) { // Beep 3
                snd.duration = 100; PuckNetwork::broadcast(snd);
            }
            else if (startSeqStep == 3) { // GO!
                snd.duration = 800; PuckNetwork::broadcast(snd);
                
                // Alle auf Rainbow schalten
                for(int idx : puckIndices) {
                    setPuck(idx, EFF_RAINBOW, CRGB::Black, 0, 150);
                }
                state = SORT_RUNNING;
            }
        }
        return; // Während Startsequenz nichts anderes machen
    }

    // --- 2. DELAYED SOUND FEEDBACK ---
    // Sendet den Sound 50ms nach dem Licht, um Paketkollisionen zu vermeiden
    if (delayedSoundPuckIdx != -1) {
        if (now >= delayedSoundTimer) {
            CommandPacket snd; snd.cmd = CMD_SOUND; snd.duration = 150;
            PuckNetwork::sendToPuck(PuckNetwork::getPucks()[delayedSoundPuckIdx].mac, snd);
            delayedSoundPuckIdx = -1; // Done
        }
    }

    // --- 3. SPERRZEIT LOGIK ---
    if (state == SORT_RUNNING) {
        for(int idx : puckIndices) {
            // Wenn 1s Sperre vorbei ist -> Zurück zu Rainbow
            if (lastTriggerTime[idx] > 0 && (now - lastTriggerTime[idx] > 1000)) {
                lastTriggerTime[idx] = 0;
                if (assignedPlayers < targetPlayers) {
                    setPuck(idx, EFF_RAINBOW, CRGB::Black, 0, 150);
                }
            }
        }
    }
}

void Game_SortingHat::handleEvent(int puckIndex, EventPacket event) {
    if (state != SORT_RUNNING) return;
    if (event.type != EVT_BTN_CLICK) return;
    
    bool isGamePuck = false;
    for(int idx : puckIndices) if(idx == puckIndex) isGamePuck = true;
    if(!isGamePuck) return;

    if (lastTriggerTime[puckIndex] > 0) return; // Gesperrt

    if (ticketBag.empty()) {
        state = SORT_FINISHED;
        return;
    }

    // --- SPIELZUG ---
    int ticketIdx = random(ticketBag.size());
    int groupID = ticketBag[ticketIdx];
    ticketBag.erase(ticketBag.begin() + ticketIdx);
    
    groupCounts[groupID]++;
    assignedPlayers++;
    
    // --- FEEDBACK (NON-BLOCKING) ---
    lastTriggerTime[puckIndex] = millis();
    
    // 1. Licht SOFORT senden
    setPuck(puckIndex, EFF_STATIC, GROUP_COLORS[groupID], 0, 255);
    
    // 2. Sound für 50ms später vormerken (Loop kümmert sich drum)
    delayedSoundPuckIdx = puckIndex;
    delayedSoundTimer = millis() + 50; 

    // --- SPIELENDE CHECK ---
    if (ticketBag.empty()) {
        state = SORT_FINISHED;
        CommandPacket snd; snd.cmd=CMD_SOUND; snd.duration=1000;
        PuckNetwork::broadcast(snd);
        
        for(int idx : puckIndices) {
            if (idx == puckIndex) continue; // Letzter Spieler behält seine Gruppenfarbe
            setPuck(idx, EFF_WIN, CRGB::Green, 0, 200);
        }
    }
}

void Game_SortingHat::setPuck(int index, int effect, CRGB color, int speed, int bright) {
    if(index < 0 || index >= MAX_PEERS) return;
    PuckInfo* p = PuckNetwork::getPucks();
    if(!p[index].active) return;
    
    CommandPacket cp;
    cp.cmd = CMD_EFFECT; cp.effectID = effect;
    cp.r = color.r; cp.g = color.g; cp.b = color.b;
    cp.duration = speed; cp.extra = bright;
    PuckNetwork::sendToPuck(p[index].mac, cp);
}

String Game_SortingHat::getStatusJSON() {
    String json = "{";
    json += "\"state\":" + String(state) + ",";
    json += "\"total\":" + String(targetPlayers) + ",";
    json += "\"done\":" + String(assignedPlayers) + ",";
    json += "\"groups\":[";
    for(int i=0; i<numGroups; i++) {
        if(i>0) json += ",";
        json += String(groupCounts[i]);
    }
    json += "]}";
    return json;
}