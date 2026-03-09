/*
 * PROJEKT: Puck Race - COORDINATOR (S3)
 * IP: 192.168.42.1 | SSID: PuckRace_Trainer (No PW)
 * printStats() gibt info über pakete (wenn man auf die settings.html geht) einfach im Serialen console eingeben
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <LittleFS.h>
#include "Common.h"
#include "WebHandler.h"
#include "GameManager.h"
#include "PuckNetwork.h" 
#include "WifiScanner.h"

#define SYSTEM_VERSION "1.6-Debug"

void setup() {
    Serial.begin(115200);
    delay(2000); // Warten damit man den Serial Monitor öffnen kann
    Serial.println("\n\n--- COORDINATOR BOOT ---");

    // 1. Dateisystem
    if (!LittleFS.begin(true)) {
        Serial.println("FATAL: LittleFS Mount Fail");
        return;
    }
    
    // --- SPEICHER CHECK ---
    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    Serial.printf("LittleFS Total: %u Bytes\n", total);
    Serial.printf("LittleFS Used:  %u Bytes\n", used);
    Serial.printf("LittleFS Free:  %u Bytes\n", total - used);
    
    if ((total - used) < 300000) { // Warnung bei weniger als 300kb
        Serial.println("!!! ACHTUNG: Speicher fast voll! Uploads könnten fehlschlagen !!!");
    }
    // ----------------------

    // 2. Netzwerk Start
    PuckNetwork::begin(); 

    // 3. Webserver Start
    WebHandler::begin();

    // 4. Game Engine Start
    GameManager::begin();

    Serial.println("System Bereit. IP: 192.168.42.1");
}

void loop() {
    PuckNetwork::update(); 
    WebHandler::update();      
    GameManager::update();  
    WifiScanner::loop();   
}