#ifndef PUCKNETWORK_H
#define PUCKNETWORK_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "Common.h"

struct QueueItem {
    uint8_t mac[6];
    EventPacket evt;
    bool valid;
};

struct PuckInfo {
    uint8_t mac[6];
    bool active;
    int rssi;
    int battery;
    int version;
    unsigned long lastSeen;
    unsigned long lastClickTime; 
    uint8_t lastSeqNr; // Deduplication Merker
    uint32_t totalClicks; // statistics
    uint32_t totalMinutes; // statistics
    unsigned long lastMinuteTick; // statistics
    CommandPacket lastEffect;     // Cache: letzter gesendeter Effekt
    bool hasLastEffect;           // Flag ob Cache gültig
};

// Statistik Struktur
struct NetworkStats {
    unsigned long totalPacketsRx;
    unsigned long duplicates;
    unsigned long validEvents;
    unsigned long queueOverflows;
    unsigned long successfulTx;
    unsigned long failedTx;
};

// --- NEU: OTA State Machine ---
enum OtaState {
    OTA_IDLE = 0,
    OTA_TRIGGERING,  // Trigger an aktuellen Puck gesendet, warte auf Timeout
    OTA_DONE         // Alle Pucks abgearbeitet
};

struct OtaStatus {
    OtaState state = OTA_IDLE;
    int currentPuckIdx = 0;   // Laufender Index in pucks[]
    int pucksTriggered = 0;   // Wie viele wurden bereits getriggert
    int totalPucks = 0;       // Gesamtzahl aktiver Pucks beim Start
    unsigned long triggerTime = 0; // Wann wurde aktueller Puck getriggert
    char currentMac[18] = ""; // MAC des aktuellen Pucks (für Anzeige)
};

class PuckNetwork {
public:
    static void begin();
    static void update(); 
    static void sendToPuck(const uint8_t* mac, CommandPacket cmd);
    static void broadcast(CommandPacket cmd);
    static PuckInfo* getPucks(); 
    static void setCredentials(String ssid, String password);
    static String getSSID();
    static String getPassword();

    // OTA: alter Broadcast (bleibt für Kompatibilität)
    static void triggerUpdateBroadcast();
    // OTA: neues sequenzielles Update
    static void triggerUpdateSequential();
    // OTA: Status für Web-API
    static String getOtaStatusJSON();

    static void setRssiMode(bool active);
    static void clearList();
    
    static NetworkStats getStats();
    static void printStats(); 
    static void resetStats();

private:
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    static void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *incomingData, int len);
#else
    static void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len);
#endif

    static void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

    // NEU: OTA State Machine Schritt (aufgerufen aus update())
    static void updateOtaStateMachine();
    // NEU: Sendet Trigger an einen einzelnen Puck per Unicast
    static void sendOtaTriggerToPuck(const uint8_t* mac);

    static PuckInfo pucks[MAX_PEERS];
    
    static const int QUEUE_SIZE = 100; 
    
    static volatile QueueItem eventQueue[QUEUE_SIZE];
    static volatile int queueHead; 
    static volatile int queueTail; 
    static bool rssiModeActive;
    
    static NetworkStats stats;

    static volatile bool sendStatusReady;
    static volatile esp_now_send_status_t lastSendStatus;

    // NEU: OTA Zustand
    static OtaStatus otaStatus;
    // Timeout pro Puck in ms (5s WiFi connect + ~10s Download + 10s Puffer)
    static const unsigned long OTA_PUCK_TIMEOUT_MS = 25000;
};

#endif
