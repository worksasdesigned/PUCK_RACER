// PuckRacer ActivationManager
// Copyright (C) 2026 Achim Stuy
//
// Dieser Header definiert die Klasse ActivationManager, die für die Verwaltung
// der Lizenz- und Freischaltlogik des PuckRacer-Systems zuständig ist.
// Sie kümmert sich um die Generierung des Gerätecodes, die Validierung
// von Freischalt-Schlüsseln und die Bereitstellung des aktuellen Lizenzstatus.

#pragma once // Stellt sicher, dass dieser Header nur einmal pro Kompilierung eingebunden wird.

#include <Arduino.h>

// Definiert die verschiedenen Zustände, in denen sich die Lizenz befinden kann.
enum class LicenseStatus {
    FREE,       // Standard-Modus mit Zeitlimit.
    EXTENDED,   // Einmalig erweiterter Modus mit höherem Zeitlimit.
    FULL        // Vollversion ohne jegliche Einschränkungen.
};

class ActivationManager {
public:
    /**
     * @brief Konstruktor für den ActivationManager.
     */
    ActivationManager();

    /**
     * @brief Initialisiert den Manager.
     * Muss in der setup() Funktion des Hauptprogramms aufgerufen werden.
     * Lädt die Lizenzdatei und führt die erste Validierung durch.
     */
    void begin();

    /**
     * @brief Versucht, das Gerät mit einem gegebenen Schlüssel freizuschalten.
     * 
     * @param key Der 9-stellige Freischalt-Schlüssel, den der Benutzer eingegeben hat.
     * @return true, wenn der Schlüssel gültig und die Freischaltung erfolgreich war, andernfalls false.
     */
    bool attemptRegistration(String key);

    /**
     * @brief Gibt den aktuellen Lizenzstatus zurück.
     * 
     * @return LicenseStatus Der aktuelle Zustand (FREE, EXTENDED, FULL).
     */
    LicenseStatus getStatus();

    /**
     * @brief Gibt das aktuelle Spiellimit in Minuten zurück, basierend auf dem Lizenzstatus.
     * 
     * @return uint32_t Das Zeitlimit in Minuten.
     */
    uint32_t getPlaytimeLimitMinutes();

    /**
     * @brief Gibt den einzigartigen, lesbaren Gerätecode für diesen Coordinator zurück.
     * Dieser Code wird aus der MAC-Adresse generiert und dem Benutzer angezeigt.
     * 
     * @return String Der 6-stellige Gerätecode (z.B. "AB CD EF").
     */
    String getDeviceCode();

    #if defined(PUCK_RACER_DEBUG)
    /**
     * @brief [DEBUG] Setzt die Zeitlimits für den Test- und Erweiterten Modus in Minuten.
     * Nur für Testzwecke, wird im Release-Build nicht kompiliert.
     * @param free_mins Minuten für den FREE Modus.
     * @param extended_mins Minuten für den EXTENDED Modus.
     */
    void setDebugTimeLimits(uint32_t free_mins, uint32_t extended_mins);

    /**
     * @brief [DEBUG] Simuliert das erfolgreiche Anwenden eines "+60h" Schlüssels.
     * Schreibt einen gültigen Schlüssel in die Datei und speichert sie.
     */
    bool applyTimeExtensionKey();
    #endif


private:
    /**
     * @brief Lädt die Lizenzinformationen aus der Datei im LittleFS.
     * Wenn die Datei nicht existiert, werden Standardwerte angenommen.
     */
    void loadLicense();

    /**
     * @brief Speichert die aktuellen Lizenzschlüssel in der Datei im LittleFS.
     * 
     * @return true, wenn das Speichern erfolgreich war, andernfalls false.
     */
    bool saveLicense();

    /**
     * @brief Validiert die geladenen Lizenzschlüssel gegen die Hardware-ID (MAC-Adresse).
     * Setzt den internen _status basierend auf dem Ergebnis.
     */
    void validateLicense();

    /**
     * @brief Generiert einen 9-stelligen Schlüssel basierend auf einem Präfix und dem Gerätecode.
     * Dies ist die Kernfunktion des Validierungsalgorithmus.
     * 
     * @param prefix Ein String-Präfix, um den Schlüsseltyp zu bestimmen (z.B. "FULL-" oder "TIME-").
     * @return String Der berechnete 9-stellige Schlüssel.
     */
    String generateKey(const char* prefix);

    // --- Private Member-Variablen ---

    LicenseStatus _status = LicenseStatus::FREE; // Interner Lizenzstatus, standardmäßig FREE.
    
    String _fullVersionKey_from_file;    // Der aus der Datei geladene Schlüssel für die Vollversion.
    String _timeExtensionKey_from_file;  // Der aus der Datei geladene Schlüssel für die Zeit-Erweiterung.
    
    String _deviceCode; // Zwischengespeicherter Gerätecode, um ständige Neuberechnung zu vermeiden.

    #if defined(PUCK_RACER_DEBUG)
    // --- DEBUG Member ---
    uint32_t _debug_free_limit_mins = 0; // 0 = inaktiv
    uint32_t _debug_extended_limit_mins = 0; // 0 = inaktiv
    #endif

    // Der geheime Salt, der zur Schlüsselgenerierung verwendet wird.
    // Muss mit dem im PHP-Skript auf der Webseite identisch sein.
    const char* _salt = "PleaseDontStealMyPr0j3ct42"; 

    // Pfad zur Lizenzdatei im LittleFS-Dateisystem.
    const char* _licenseFilePath = "/license.json";
};
