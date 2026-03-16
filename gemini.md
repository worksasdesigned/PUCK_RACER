# Projektkontext: PuckRacer
Dies ist ein ESP32 Reaktionsspiel. Ein ESP32-S3 (Coordinator) steuert 4 bis 12 ESP32-C3 (Pucks). 
Die Kommunikation läuft ausschließlich über ESP-NOW. 
Die Pucks haben 35 WS2812B LEDs, sind batteriebetrieben und besitzen einen Buzzer sowie einen Arcade-Knopf zur Eingabe. 
Das System bietet ca. 20-25 Spiele für den Sportunterricht oder das Vereinstraining. 
Ein Trainer verbindet sich per Handy-WLAN mit dem Access Point des Coordinators und bedient das System über einen asynchronen Webserver (LittleFS).

# Globale Verhaltensregeln
- Fasse dich kurz, aber detailliert bei deinen Antworten.
- Antworte mit Fakten und rede nichts schön.
- Falls Parameter unlogisch oder falsch sind, weise mich direkt darauf hin. Stelle Rückfragen bei unpräzisen Anforderungen.
- Kommentiere produzierten Code ausführlich. Erzeuge gut strukturierten Code, der möglichst anfängerfreundlich zu lesen ist.
- Sende bei Programmierungen möglichst das ganze Skript und nicht nur kleine Ausschnitte.

# Spezifische Vorgaben für HTML / Web-Dateien
Beim Überarbeiten oder Erstellen von HTML, CSS oder JS Dateien sind folgende Prüfungen zwingend durchzuführen:
1. Offline-Fähigkeit: Es dürfen keine externen CDNs (wie Google Fonts, externe Bootstrap- oder jQuery-Links) verwendet werden, da das System in einem isolierten WLAN ohne Internet läuft.
2. Responsiveness: Das Design muss zwingend auf mobilen Bildschirmen (Trainer-Handy) lesbar und bedienbar bleiben.
3. Caching: Vermeide Inline-Skripte oder Inline-CSS, wenn diese in die `style.css` oder `script.js` ausgelagert werden können.
4. Darstellung: Tabellarische Zahlenwerte (wie RSSI, Batterie oder Zeiten) müssen mit `font-variant-numeric: tabular-nums;` formatiert werden, damit die Spalten nicht springen.
5. prüfe immer ob die auf den Pucks angezeigte Puckfarbe auch zur Gruppenfarbe (bzw Spielerfarbe) auf den HTML Seiten passt.
6. prüfe ob du hard gecodete englische oder deutsche Begriffe findest, die noch in der lang.js übersetzt werden müssen.
7. Prüfe nach dem Ändern einer webseite ob noch alle Funktionen vorhanden sind oder ob due etwas vergessen hast.


# Spezifische Vorgaben für C++ (Arduino Core)
- Blockierende Funktionen wie `delay()` sind im Haupt-Loop strikt verboten. Nutze stattdessen `millis()` für zeitgesteuerte Abläufe.
- Nutze FreeRTOS-Spinlocks (`portMUX_TYPE`), wenn Variablen sowohl in Interrupts (z.B. ESP-NOW Callbacks) als auch im Loop verwendet werden.