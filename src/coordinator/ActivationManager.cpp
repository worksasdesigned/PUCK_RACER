// PuckRacer ActivationManager
// Copyright (C) 2026 Achim Stuy
//
// Diese Datei implementiert die Logik der ActivationManager-Klasse.

#include "ActivationManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h> // Notwendig für das Parsen der Lizenzdatei
#include <esp_mac.h>     // Notwendig für den Zugriff auf die MAC-Adresse
#include <esp_rom_crc.h> // CRC32 aus dem ESP32-ROM

// Definiert den Zeichensatz für den Gerätecode. Ohne I, O, L, Z um Verwechslungen zu vermeiden.
const char DEVICE_CODE_ALPHABET[] = "ABCDEFGHJKMNPQRSTUVWXY";
const int ALPHABET_SIZE = sizeof(DEVICE_CODE_ALPHABET) - 1; // Größe ohne Null-Terminator

// Konstruktor ist leer, die eigentliche Arbeit wird in begin() gemacht.
ActivationManager::ActivationManager() {}

void ActivationManager::begin() {
    // Lade die gespeicherten Lizenzschlüssel aus dem Dateisystem.
    loadLicense();

    // Generiere den Gerätecode und validiere die geladenen Schlüssel.
    // Dies setzt den initialen Status (FREE, EXTENDED, FULL).
    validateLicense();
}

void ActivationManager::loadLicense() {
    if (!LittleFS.exists(_licenseFilePath)) {
        // Wenn keine Lizenzdatei existiert, ist das nicht schlimm.
        // Die Schlüssel bleiben leer und das Gerät im FREE-Modus.
        return;
    }

    File file = LittleFS.open(_licenseFilePath, "r");
    if (!file) {
        Serial.println(F("Fehler beim Öffnen der Lizenzdatei zum Lesen."));
        return;
    }

    // JSON-Dokument zum Parsen der Datei erstellen.
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.print(F("Fehler beim Parsen der Lizenzdatei: "));
        Serial.println(error.c_str());
        return;
    }

    // Die Schlüssel aus dem JSON-Dokument in die Member-Variablen laden.
    _fullVersionKey_from_file = doc["full_version_key"].as<String>();
    _timeExtensionKey_from_file = doc["time_extension_key"].as<String>();
}

bool ActivationManager::saveLicense() {
    File file = LittleFS.open(_licenseFilePath, "w");
    if (!file) {
        Serial.println(F("Fehler beim Öffnen der Lizenzdatei zum Schreiben."));
        return false;
    }

    JsonDocument doc;
    doc["full_version_key"] = _fullVersionKey_from_file;
    doc["time_extension_key"] = _timeExtensionKey_from_file;

    // JSON in die Datei schreiben.
    if (serializeJson(doc, file) == 0) {
        Serial.println(F("Fehler beim Schreiben in die Lizenzdatei."));
        file.close();
        return false;
    }

    file.close();
    return true;
}

String ActivationManager::getDeviceCode() {
    // Wenn der Code schon generiert wurde, direkt zurückgeben (Caching).
    if (_deviceCode.length() > 0) {
        return _deviceCode;
    }

    // MAC-Adresse des Geräts auslesen.
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // Wir verwenden die letzten 3 Bytes der MAC für den Code.
    char code[9]; // 6 Zeichen + 2 Leerzeichen + Null-Terminator
    
    // Byte 3 der MAC wird zu Zeichen 0 und 1 des Codes
    uint8_t high_nibble = (mac[3] >> 4) & 0x0F;
    uint8_t low_nibble = mac[3] & 0x0F;
    code[0] = DEVICE_CODE_ALPHABET[high_nibble % ALPHABET_SIZE];
    code[1] = DEVICE_CODE_ALPHABET[low_nibble % ALPHABET_SIZE];
    code[2] = ' ';

    // Byte 4 der MAC wird zu Zeichen 2 und 3 des Codes
    high_nibble = (mac[4] >> 4) & 0x0F;
    low_nibble = mac[4] & 0x0F;
    code[3] = DEVICE_CODE_ALPHABET[high_nibble % ALPHABET_SIZE];
    code[4] = DEVICE_CODE_ALPHABET[low_nibble % ALPHABET_SIZE];
    code[5] = ' ';

    // Byte 5 der MAC wird zu Zeichen 4 und 5 des Codes
    high_nibble = (mac[5] >> 4) & 0x0F;
    low_nibble = mac[5] & 0x0F;
    code[6] = DEVICE_CODE_ALPHABET[high_nibble % ALPHABET_SIZE];
    code[7] = DEVICE_CODE_ALPHABET[low_nibble % ALPHABET_SIZE];
    code[8] = '\0'; // String-Ende

    _deviceCode = String(code);
    return _deviceCode;
}


String ActivationManager::generateKey(const char* prefix) {
    // Gerätecode ohne Leerzeichen holen für die Hash-Berechnung.
    String cleanDeviceCode = getDeviceCode();
    cleanDeviceCode.replace(" ", "");

    // Den String für die Hash-Berechnung zusammenbauen.
    String stringToHash = String(prefix) + cleanDeviceCode + String(_salt);

    // CRC32-Hash berechnen.
    // Standard CRC32 (init=0xFFFFFFFF, final XOR) — identisch mit Python zlib.crc32()
    uint32_t hashValue = ~esp_rom_crc32_le(~0U, (const uint8_t*)stringToHash.c_str(), stringToHash.length());

    // Den Hash auf eine 9-stellige Zahl bringen.
    long keyNum = hashValue % 1000000000;
    
    // Den Schlüssel als 9-stelligen String formatieren (mit führenden Nullen).
    char keyStr[10];
    sprintf(keyStr, "%09ld", keyNum);

    return String(keyStr);
}


void ActivationManager::validateLicense() {
    // Generiere die für dieses Gerät erwarteten Schlüssel.
    String expectedFullKey = generateKey("FULL-");
    String expectedTimeKey = generateKey("TIME-");

    // Prüfe, ob der gespeicherte Vollversionsschlüssel gültig ist.
    if (_fullVersionKey_from_file.length() > 0 && _fullVersionKey_from_file == expectedFullKey) {
        _status = LicenseStatus::FULL;
        return; // Wichtig: Prüfung hier beenden.
    }

    // Wenn keine Vollversion, prüfe, ob der Zeitschlüssel gültig ist.
    if (_timeExtensionKey_from_file.length() > 0 && _timeExtensionKey_from_file == expectedTimeKey) {
        _status = LicenseStatus::EXTENDED;
        return;
    }

    // Wenn kein Schlüssel passt, ist der Status FREE.
    _status = LicenseStatus::FREE;
}


bool ActivationManager::attemptRegistration(String key) {
    if (key.length() != 9) {
        return false; // Ungültiges Format
    }

    String expectedFullKey = generateKey("FULL-");
    if (key == expectedFullKey) {
        _fullVersionKey_from_file = key;
        _status = LicenseStatus::FULL;
        saveLicense(); // Neuen Schlüssel speichern.
        return true;
    }

    String expectedTimeKey = generateKey("TIME-");
    if (key == expectedTimeKey) {
        _timeExtensionKey_from_file = key;
        _status = LicenseStatus::EXTENDED;
        saveLicense(); // Neuen Schlüssel speichern.
        return true;
    }

    return false; // Der eingegebene Schlüssel ist für dieses Gerät ungültig.
}


LicenseStatus ActivationManager::getStatus() {
    return _status;
}

uint32_t ActivationManager::getPlaytimeLimitMinutes() {
    #if defined(PUCK_RACER_DEBUG)
    if (_debug_free_limit_mins > 0 && _status == LicenseStatus::FREE) {
        return _debug_free_limit_mins;
    }
    if (_debug_extended_limit_mins > 0 && _status == LicenseStatus::EXTENDED) {
        return _debug_extended_limit_mins;
    }
    #endif

    switch (_status) {
        case LicenseStatus::FULL:
            return 99999 * 60; // Praktisch unendlich
        case LicenseStatus::EXTENDED:
            return 80 * 60;
        case LicenseStatus::FREE:
        default:
            return 20 * 60;
    }
}

#if defined(PUCK_RACER_DEBUG)
void ActivationManager::setDebugTimeLimits(uint32_t free_mins, uint32_t extended_mins) {
    _debug_free_limit_mins = free_mins;
    _debug_extended_limit_mins = extended_mins;
    Serial.printf("[DEBUG] New time limits: FREE=%u min, EXTENDED=%u min\n", free_mins, extended_mins);
}

bool ActivationManager::applyTimeExtensionKey() {
    // Wenn schon Vollversion, nichts tun
    if (_status == LicenseStatus::FULL) {
        return false;
    }

    // Simuliere das Eintragen eines gültigen Zeit-Schlüssels
    _timeExtensionKey_from_file = generateKey("TIME-");
    _status = LicenseStatus::EXTENDED;
    
    // Speichere die Änderung in der Lizenzdatei
    return saveLicense();
}
#endif
