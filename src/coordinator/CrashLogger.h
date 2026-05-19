#ifndef CRASHLOGGER_H
#define CRASHLOGGER_H

#include <Arduino.h>

// File-basierte Black-Box zur Diagnose von Coordinator-Hangs.
// Schreibt alle SAMPLE_INTERVAL_MS eine CSV-Zeile mit Heap-, TX/RX- und
// Stack-Stats nach /crashlog.csv. Beim Boot wird der Reset-Reason und die
// letzte gesampelte Zeile vor dem Reset in eine BOOT-Markerzeile geschrieben,
// damit man post-mortem sieht ob es ein echter Crash (Panic/WDT) oder ein
// Hang (Reset erst durch Power-Cycle) war.
//
// Trade-off: das Schreiben belastet LittleFS. Bei 5s-Sampling und ~80B/Zeile
// reden wir über ~60KB/h. LittleFS wear-leveling fängt das ab, ist aber
// bewusst auf 256KB Rotation begrenzt (siehe MAX_LOG_BYTES).
class CrashLogger {
public:
    static void begin();
    static void update();

    // Schreibt den aktuellen Buffer auf jeden Fall raus + setzt
    // einen "GRACEFUL_SHUTDOWN" Marker. Aktuell nicht aufgerufen, aber
    // vorhanden für künftige geplante Restarts.
    static void flush();

private:
    static unsigned long lastSample;
    static unsigned long startMs;
    static bool initialized;

    static void writeBootMarker();
    static void writeSample();
    static void rotateIfNeeded();
    static const char* resetReasonStr(int reason);
};

#endif
