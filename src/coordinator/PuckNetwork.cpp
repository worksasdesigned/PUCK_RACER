#include "PuckNetwork.h"
#include "GameManager.h"
#include <Preferences.h> 
#include "StatsManager.h"

PuckInfo PuckNetwork::pucks[MAX_PEERS];
volatile QueueItem PuckNetwork::eventQueue[QUEUE_SIZE];
volatile int PuckNetwork::queueHead = 0;
volatile int PuckNetwork::queueTail = 0;
bool PuckNetwork::rssiModeActive = false; 
static uint8_t globalCmdSeq = 0; 

NetworkStats PuckNetwork::stats = {0, 0, 0, 0, 0, 0};

volatile bool PuckNetwork::sendStatusReady = false;
volatile esp_now_send_status_t PuckNetwork::lastSendStatus = ESP_NOW_SEND_FAIL;

// OTA Zustand initialisieren
OtaStatus PuckNetwork::otaStatus;

Preferences prefs;
String currentSSID = "PuckRace_Trainer";
String currentPW = "";

IPAddress local_IP(192, 168, 42, 1);
IPAddress gateway(192, 168, 42, 1);
IPAddress subnet(255, 255, 255, 0);

void PuckNetwork::begin() {
    prefs.begin("puck_net", false); 
    currentSSID = prefs.getString("ssid", "PuckRace_Trainer");
    currentPW = prefs.getString("pw", "");
    prefs.end();

    // FIX 1: AP_STA Modus aktivieren! So können Pucks auf dem STA-Interface laufen 
    // und kollidieren nicht mit dem SoftAP Limit (Handys).
    WiFi.mode(WIFI_AP_STA); 
    WiFi.softAPConfig(local_IP, gateway, subnet);
    if (currentPW.length() > 0) WiFi.softAP(currentSSID.c_str(), currentPW.c_str(), WIFI_CHANNEL, 0, MAX_PEERS);
    else WiFi.softAP(currentSSID.c_str(), NULL, WIFI_CHANNEL, 0, MAX_PEERS);
    
    Serial.printf("PuckNetwork: AP Started '%s'\n", currentSSID.c_str());

    if (esp_now_init() != ESP_OK) { ESP.restart(); }
    
    esp_now_register_recv_cb(OnDataRecv);
    esp_now_register_send_cb((esp_now_send_cb_t)OnDataSent); 

    // FIX 2: Broadcast Peer auf das STA-Interface legen
    esp_now_peer_info_t peerInfo = {};
    const uint8_t broadcast[] = BROADCAST_MAC;
    memcpy(peerInfo.peer_addr, broadcast, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA; // GEÄNDERT!
    esp_now_add_peer(&peerInfo);

    delay(200);
    CommandPacket ping;
    memset(&ping, 0, sizeof(ping));
    ping.cmd = CMD_PING;
    const uint8_t bc[] = BROADCAST_MAC;
    for (int i = 0; i < 3; i++) {
        esp_now_send(bc, (uint8_t*)&ping, sizeof(ping));
        delay(20);
    }
    Serial.println("NETWORK: Re-Pair Ping broadcast sent.");
}

void PuckNetwork::clearList() {
    for(int i=0; i<MAX_PEERS; i++) {
        pucks[i].active = false;
        memset(pucks[i].mac, 0, 6);
        pucks[i].rssi = 0;
    }
    queueHead = 0;
    queueTail = 0;
    Serial.println("NETWORK: Puck List flushed!");
}

void PuckNetwork::setRssiMode(bool active) {
    rssiModeActive = active;
    if (!active) {
        CommandPacket light; light.cmd = CMD_EFFECT; light.effectID = EFF_STATUS; 
        light.r = 0; light.g = 255; light.b = 0; light.extra = 255; light.duration = 0; 
        broadcast(light);
    }
}

void PuckNetwork::update() {
    while (queueHead != queueTail) {
        QueueItem item;
        memcpy(&item, (void*)&eventQueue[queueTail], sizeof(QueueItem));
        queueTail = (queueTail + 1) % QUEUE_SIZE;
        
        int foundIdx = -1;
        for(int i=0; i<MAX_PEERS; i++) {
            if(pucks[i].active && memcmp(pucks[i].mac, item.mac, 6) == 0) {
                foundIdx = i; break;
            }
        }

        // FIX 3: Die Registrierung in den sicheren Main-Loop verlegt!
        // Hier kollidiert es nicht mit dem Wi-Fi Interrupt.
        if (!esp_now_is_peer_exist(item.mac)) {
            esp_now_peer_info_t peerInfo = {};
            memcpy(peerInfo.peer_addr, item.mac, 6);
            peerInfo.channel = WIFI_CHANNEL;
            peerInfo.encrypt = false;
            peerInfo.ifidx = WIFI_IF_STA; // Pucks auf das grenzenlose STA-Interface buchen
            
            if (esp_now_add_peer(&peerInfo) == ESP_OK) {
                Serial.println("NETWORK: Neuer Puck registriert (Slot belegt).");
            } else {
                Serial.println("NETWORK: FEHLER - Puck konnte nicht registriert werden!");
            }
        }

        if (item.evt.type == EVT_HELLO) { 
            CommandPacket ack; ack.cmd = CMD_PAIR_ACK;
            sendToPuck(item.mac, ack);

            bool restored = false;
            for (int i = 0; i < MAX_PEERS; i++) {
                if (pucks[i].hasLastEffect && memcmp(pucks[i].mac, item.mac, 6) == 0) {
                    Serial.printf("RECONNECT: Puck %d → restoring cached effect (ID=%d)\n", 
                                  i, pucks[i].lastEffect.effectID);
                    sendToPuck(item.mac, pucks[i].lastEffect);
                    restored = true;
                    break;
                }
            }
            if (!restored) {
                CommandPacket light; light.cmd = CMD_EFFECT; light.effectID = EFF_STATUS; 
                light.r = 0; light.g = 255; light.b = 0; light.extra = 255; light.duration = 0; 
                sendToPuck(item.mac, light);
            }
        }

        if (item.evt.type == EVT_BTN_CLICK && foundIdx != -1) {
            pucks[foundIdx].lastClickTime = millis();
        }

        GameManager::handlePuckEvent(item.mac, item.evt);
    }

    unsigned long now_ms = millis();
    for(int i=0; i<MAX_PEERS; i++) {
        if(pucks[i].active) {
            if(now_ms - pucks[i].lastMinuteTick >= 60000) {
                pucks[i].lastMinuteTick = now_ms;
                pucks[i].totalMinutes++;
            }
            // Timeout bei 30s
            if(pucks[i].lastSeen > 0 && now_ms - pucks[i].lastSeen > 30000) {
                Serial.printf("NETWORK: Puck %d timeout (lastSeen %lums ago) → removed.\n",
                              i, now_ms - pucks[i].lastSeen);
                pucks[i].active = false;
            }
        }
    }

    static unsigned long lastStatsPrint = 0;
    if (millis() - lastStatsPrint > 5000) { 
        lastStatsPrint = millis();
        printStats(); 
    }

    static unsigned long lastAutoSave = 0;
    if (millis() - lastAutoSave > 600000UL) {
        lastAutoSave = millis();
        StatsManager::saveAll();
        Serial.println("NETWORK: Auto-Save Stats.");
    }

    static unsigned long lastRssiUpdate = 0;
    if (rssiModeActive && millis() - lastRssiUpdate > 2000) {
        lastRssiUpdate = millis();
        for(int i = 0; i < MAX_PEERS; i++) {
            if(pucks[i].active) {
                int percent = map(constrain(pucks[i].rssi, -95, -55), -95, -55, 0, 255);
                CommandPacket cp;
                cp.cmd = CMD_EFFECT; cp.effectID = EFF_PROGRESS;
                cp.duration = percent; cp.extra = 40; 
                const uint8_t palette[][3] = {
                    {0,0,255}, {0,255,0}, {255,0,0}, {255,255,0}, {255,0,255}, {0,255,255}, 
                    {255,165,0}, {128,0,128}, {64,224,208}, {255,192,203}, {255,255,255}, {0,255,0}
                };
                cp.r = palette[i % 12][0]; cp.g = palette[i % 12][1]; cp.b = palette[i % 12][2];
                esp_now_send(pucks[i].mac, (uint8_t*)&cp, sizeof(cp));
            }
        }
    }

    if (otaStatus.state != OTA_IDLE) {
        updateOtaStateMachine();
    }
}

// =============================================================================
// SEQUENZIELLE OTA STATE MACHINE
// =============================================================================

void PuckNetwork::triggerUpdateSequential() {
    int count = 0;
    for(int i=0; i<MAX_PEERS; i++) {
        if(pucks[i].active) count++;
    }

    if (count == 0) {
        Serial.println("OTA SEQ: Keine aktiven Pucks gefunden!");
        return;
    }

    Serial.printf("OTA SEQ: Starte sequenzielles Update für %d Pucks...\n", count);
    setRssiMode(false);

    otaStatus.state = OTA_TRIGGERING;
    otaStatus.currentPuckIdx = 0;
    otaStatus.pucksTriggered = 0;
    otaStatus.totalPucks = count;
    otaStatus.triggerTime = 0; 
}

void PuckNetwork::updateOtaStateMachine() {
    if (otaStatus.state == OTA_IDLE || otaStatus.state == OTA_DONE) return;

    unsigned long now = millis();
    bool firstTrigger = (otaStatus.triggerTime == 0);

    if (firstTrigger || (now - otaStatus.triggerTime >= OTA_PUCK_TIMEOUT_MS)) {

        int nextIdx = -1;
        for(int i = otaStatus.currentPuckIdx; i < MAX_PEERS; i++) {
            if(pucks[i].active) {
                nextIdx = i;
                break;
            }
        }

        if (nextIdx == -1) {
            Serial.printf("OTA SEQ: Abgeschlossen. %d/%d Pucks getriggert.\n",
                          otaStatus.pucksTriggered, otaStatus.totalPucks);
            otaStatus.state = OTA_DONE;
            return;
        }

        sendOtaTriggerToPuck(pucks[nextIdx].mac);

        otaStatus.pucksTriggered++;
        otaStatus.currentPuckIdx = nextIdx + 1; 
        otaStatus.triggerTime = now;

        snprintf(otaStatus.currentMac, sizeof(otaStatus.currentMac),
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 pucks[nextIdx].mac[0], pucks[nextIdx].mac[1], pucks[nextIdx].mac[2],
                 pucks[nextIdx].mac[3], pucks[nextIdx].mac[4], pucks[nextIdx].mac[5]);

        Serial.printf("OTA SEQ: Trigger an Puck %d/%d (%s). Warte %lus...\n",
                      otaStatus.pucksTriggered, otaStatus.totalPucks,
                      otaStatus.currentMac, OTA_PUCK_TIMEOUT_MS / 1000);
    }
}

void PuckNetwork::sendOtaTriggerToPuck(const uint8_t* mac) {
    UpdateCredentialsPacket pkg;
    memset(&pkg, 0, sizeof(pkg));
    pkg.cmd = CMD_UPDATE_MODE;
    strncpy(pkg.ssid, currentSSID.c_str(), 16);
    strncpy(pkg.password, currentPW.c_str(), 10);

    for(int i = 0; i < 3; i++) {
        esp_now_send(mac, (uint8_t*)&pkg, sizeof(pkg));
        delay(30);
    }
}

String PuckNetwork::getOtaStatusJSON() {
    String json = "{";
    json += "\"active\":" + String(otaStatus.state == OTA_TRIGGERING ? "true" : "false") + ",";
    json += "\"done\":" + String(otaStatus.state == OTA_DONE ? "true" : "false") + ",";
    json += "\"triggered\":" + String(otaStatus.pucksTriggered) + ",";
    json += "\"total\":" + String(otaStatus.totalPucks) + ",";
    json += "\"current\":\"" + String(otaStatus.currentMac) + "\",";

    unsigned long elapsed = (otaStatus.triggerTime > 0) ? (millis() - otaStatus.triggerTime) : 0;
    unsigned long remaining = (elapsed < OTA_PUCK_TIMEOUT_MS) ? (OTA_PUCK_TIMEOUT_MS - elapsed) / 1000 : 0;
    json += "\"remaining_s\":" + String(remaining);
    json += "}";
    return json;
}

// Alter Broadcast (bleibt für Rückwärtskompatibilität)
void PuckNetwork::triggerUpdateBroadcast() {
    Serial.println("NETWORK: Broadcasting OTA Trigger...");
    setRssiMode(false);
    
    UpdateCredentialsPacket pkg;
    memset(&pkg, 0, sizeof(pkg));
    pkg.cmd = CMD_UPDATE_MODE;
    strncpy(pkg.ssid, currentSSID.c_str(), 16);
    strncpy(pkg.password, currentPW.c_str(), 10);
    
    const uint8_t b[] = BROADCAST_MAC;
    for(int i=0; i<5; i++) {
        esp_now_send(b, (uint8_t*)&pkg, sizeof(pkg));
        delay(40); 
    }
}

// =============================================================================
// ISR UND EMPFANG (Repariert)
// =============================================================================

void PuckNetwork::OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    lastSendStatus = status;
    sendStatusReady = true;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void PuckNetwork::OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *incomingData, int len) {
    const uint8_t *mac_addr = info->src_addr;
    int current_rssi = info->rx_ctrl ? info->rx_ctrl->rssi : -50;
#else
void PuckNetwork::OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
    int current_rssi = -50; // Fallback für Core 2.x
#endif

    if (len != sizeof(EventPacket)) return;
    stats.totalPacketsRx++; 

    EventPacket tempEvt;
    memcpy(&tempEvt, incomingData, sizeof(EventPacket));

    int puckIdx = -1;
    for(int i=0; i<MAX_PEERS; i++) {
        if(pucks[i].active && memcmp(pucks[i].mac, mac_addr, 6) == 0) {
            puckIdx = i; break;
        }
    }
    
    if (puckIdx == -1) {
        for(int i=0; i<MAX_PEERS; i++) {
            if(!pucks[i].active) {
                puckIdx = i;
                memcpy(pucks[i].mac, mac_addr, 6);
                pucks[i].active = true;
                
                pucks[i].lastSeqNr = tempEvt.seqNr - 1; 
                pucks[i].totalClicks = StatsManager::getPuckClicks(mac_addr);
                pucks[i].totalMinutes = StatsManager::getPuckTime(mac_addr);
                pucks[i].lastMinuteTick = millis();
                break;
            }
        }
    }

    if (puckIdx != -1) {
        if (pucks[puckIdx].lastSeqNr == tempEvt.seqNr) {
            stats.duplicates++; 
            pucks[puckIdx].rssi = current_rssi;
            return; 
        }
        
        pucks[puckIdx].lastSeqNr = tempEvt.seqNr;
        stats.validEvents++;

        int nextHead = (queueHead + 1) % QUEUE_SIZE;
        if (nextHead == queueTail) {
            stats.queueOverflows++; 
            Serial.println("!!! NETWORK QUEUE OVERFLOW !!!");
            return;
        }

        memcpy((void*)eventQueue[queueHead].mac, mac_addr, 6);
        memcpy((void*)&eventQueue[queueHead].evt, incomingData, sizeof(EventPacket));

        if (tempEvt.type == EVT_BTN_CLICK) {
            pucks[puckIdx].totalClicks++;
        }

        pucks[puckIdx].rssi = current_rssi;
        pucks[puckIdx].battery = tempEvt.battery_mv;
        pucks[puckIdx].version = tempEvt.version;
        pucks[puckIdx].lastSeen = millis();
        
        queueHead = nextHead;
    }
}

void PuckNetwork::sendToPuck(const uint8_t* mac, CommandPacket cmd) {
    globalCmdSeq++;
    cmd.seqNr = globalCmdSeq;

    for (int retries = 0; retries < 3; retries++) {
        sendStatusReady = false;
        esp_err_t result = esp_now_send(mac, (uint8_t*)&cmd, sizeof(cmd));
        
        if (result != ESP_OK) {
            delay(5);
            continue; 
        }
        
        unsigned long start = millis();
        while (!sendStatusReady && millis() - start < 15) {
            yield(); 
        }
        
        if (sendStatusReady && lastSendStatus == ESP_NOW_SEND_SUCCESS) {
            stats.successfulTx++;
            if (cmd.cmd == CMD_EFFECT) {
                for (int i = 0; i < MAX_PEERS; i++) {
                    if (pucks[i].active && memcmp(pucks[i].mac, mac, 6) == 0) {
                        pucks[i].lastEffect = cmd;
                        pucks[i].hasLastEffect = true;
                        break;
                    }
                }
            }
            return; 
        }
        
        delay(8);
    }
    stats.failedTx++; 
}

void PuckNetwork::broadcast(CommandPacket cmd) {
    globalCmdSeq++;
    cmd.seqNr = globalCmdSeq;

    const uint8_t b[] = BROADCAST_MAC;
    for(int i=0; i<3; i++) {
        esp_now_send(b, (uint8_t*)&cmd, sizeof(cmd));
        delay(10);
    }
    if (cmd.cmd == CMD_EFFECT) {
        for (int i = 0; i < MAX_PEERS; i++) {
            if (pucks[i].active) {
                pucks[i].lastEffect = cmd;
                pucks[i].hasLastEffect = true;
            }
        }
    }
}

PuckInfo* PuckNetwork::getPucks() { return pucks; }

NetworkStats PuckNetwork::getStats() { return stats; }
void PuckNetwork::resetStats() { stats = {0,0,0,0,0,0}; }

void PuckNetwork::printStats() {
    float qual = 0;
    if (stats.totalPacketsRx > 0) {
        qual = (float)stats.validEvents / (float)stats.totalPacketsRx * 100.0;
    }
    float txQual = 0;
    if ((stats.successfulTx + stats.failedTx) > 0) {
        txQual = (float)stats.successfulTx / (float)(stats.successfulTx + stats.failedTx) * 100.0;
    }

    Serial.printf("[NET] RX-Total: %lu | Valid: %lu | Dupes: %lu | RX-Qual: %.1f%%  ||  TX-OK: %lu | TX-FAIL: %lu | TX-Qual: %.1f%%\n", 
        stats.totalPacketsRx, stats.validEvents, stats.duplicates, qual,
        stats.successfulTx, stats.failedTx, txQual);
}

void PuckNetwork::setCredentials(String ssid, String password) {
    prefs.begin("puck_net", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pw", password);
    prefs.end();
}
String PuckNetwork::getSSID() { return currentSSID; }
String PuckNetwork::getPassword() { return currentPW; }