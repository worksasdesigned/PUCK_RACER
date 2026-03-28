 #ifndef COMMON_H
#define COMMON_H

#include <Arduino.h>

// --- KONFIGURATION ---
#define WIFI_CHANNEL 1              
#define MAX_PEERS 20                
#define BROADCAST_MAC {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

// --- EFFEKT LISTE (IDs) ---
enum EffectID {
    EFF_OFF = 0,            
    EFF_STATIC = 1,         
    EFF_BLINK = 2,          
    EFF_BREATHE = 3,        
    EFF_BREATHE_MOD2 = 4,   
    EFF_BREATHE_MOD4 = 5,   
    EFF_SINGLE_CHASE = 6,   
    EFF_DOUBLE_CHASE = 7,   
    EFF_SPLIT_ROT = 8,      
    EFF_RAINBOW = 9,        
    EFF_COUNTDOWN = 10,     
    EFF_LOADING = 11,       
    EFF_WIN = 12,           
    EFF_FAIL = 13,          
    EFF_FLASH = 14,         
    EFF_SPARKLE = 15,       
    EFF_POLICE = 16,        
    EFF_STATUS = 17,
    EFF_PROGRESS = 18,
    EFF_BLINK_COUNT = 19,
    EFF_FLASH_BEEP = 20
};

// --- BEFEHLSTYPEN ---
enum CommandType {
    CMD_PING = 0,
    CMD_PAIR_ACK = 1,
    CMD_UPDATE_MODE = 2,
    CMD_KEEPALIVE = 3,       // Bidirektionaler Heartbeat: Coordinator → Puck (zyklisch)
                             // Dient der Verbindungsüberwachung: Puck erkennt damit,
                             // ob der Coordinator noch erreichbar ist.
    CMD_EFFECT = 10,
    CMD_SOUND = 11,
    CMD_SEQUENCE = 12,
    CMD_SET_BRIGHTNESS = 13,  // Max-Helligkeit in % (extra = 10..100)
    CMD_RESET = 99
};

// --- SOUND IDs ---
enum SoundSeqID {
    SEQ_OFF = 0,
    SEQ_FANFARE = 1,
    SEQ_ERROR = 2,
    SEQ_EXPLOSION = 3,
    SEQ_HERO = 4,
    SEQ_MARIO = 5,
    SEQ_TETRIS = 6,
    SEQ_RACE = 7,
    SEQ_SKI = 8,
    SEQ_DINGDONG = 9
};

// --- EVENT TYPEN ---
enum EventType {
    EVT_HELLO = 0,            
    EVT_HEARTBEAT = 1,        
    EVT_BTN_CLICK = 10,       
    EVT_BTN_HOLD = 11,        
    EVT_BTN_DOUBLE = 12,
    EVT_BTN_RELEASE = 20      
};

// --- DATENSTRUKTUREN ---

// Befehl vom Coordinator an den Puck
typedef struct __attribute__((packed)) {
    uint8_t cmd;              
    uint8_t effectID;         
    uint8_t r, g, b;          
    uint16_t duration;        
    uint8_t extra;
    uint8_t seqNr;  // NEU: Sequenz-Nummer für Befehle (gegen Stottern bei 2x Senden)
} CommandPacket;

// Event vom Puck an den Coordinator
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint16_t battery_mv;
    uint8_t version;
    uint8_t seqNr;  // NEU: Sequenz-Nummer gegen Doppler bei 3x Senden
    int16_t temp_c10;  // Temperatur in 0.1°C Einheiten (z.B. 253 = 25.3°C), -999 = kein Sensor
} EventPacket;

// OTA Update Credentials
typedef struct __attribute__((packed)) {
    uint8_t cmd;              
    char ssid[17];            
    char password[11];        
} UpdateCredentialsPacket;

#endif