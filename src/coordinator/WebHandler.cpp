#include "WebHandler.h"
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <Update.h>
#include "PuckNetwork.h"
#include "GameManager.h"
#include "StatsManager.h" 
#include "WifiScanner.h"

#define SYS_VER "v2.97.8" 

AsyncWebServer WebHandler::server(80);
DNSServer WebHandler::dnsServer;

IPAddress WebHandler::activeClientIP;
unsigned long WebHandler::lastActivityTime = 0;
String WebHandler::activeClientName = "";

#define SESSION_TIMEOUT_MS 45000UL

bool WebHandler::isClientAllowed(AsyncWebServerRequest *req) {
    unsigned long now = millis();
    IPAddress clientIP = req->client()->remoteIP();
    if (lastActivityTime == 0 || (now - lastActivityTime > SESSION_TIMEOUT_MS)) {
        activeClientIP = clientIP;
        lastActivityTime = now;
        activeClientName = clientIP.toString();
        return true;
    }
    if (clientIP == activeClientIP) {
        lastActivityTime = now;
        return true;
    }
    return false;
}

static size_t debugUploadSize = 0;

// --- Die Welcome / Setup Page (wird direkt aus dem RAM geladen, falls LittleFS leer ist) ---
const char* welcomeHTML PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PuckRacer Setup</title>
    <style>
        body { background: #121212; color: #e0e0e0; font-family: sans-serif; text-align: center; margin: 0; padding: 20px; }
        h1 { color: #0f0; margin-bottom: 5px; }
        .container { max-width: 650px; margin: 40px auto; background: #1e1e1e; padding: 30px; border-radius: 12px; border: 1px solid #333; box-shadow: 0 10px 30px rgba(0,0,0,0.5); }
        p { color: #aaa; line-height: 1.5; }
        .drop-zone { border: 2px dashed #444; border-radius: 8px; padding: 50px 20px; margin: 25px 0; cursor: pointer; transition: 0.2s; background: #151515; }
        .drop-zone:hover, .drop-zone.dragover { border-color: #0f0; background: #1a2a1a; }
        .drop-zone input[type="file"] { display: none; }
        .icon { font-size: 3rem; margin-bottom: 10px; display: block; }
        .progress-list { text-align: left; max-height: 250px; overflow-y: auto; margin-top: 20px; font-size: 0.9rem; background: #000; padding: 10px; border-radius: 6px; border: 1px solid #333; display: none; }
        .progress-list div { border-bottom: 1px solid #222; padding: 6px 4px; font-family: monospace; }
        .btn { background: #0f0; color: #000; padding: 15px 30px; border: none; border-radius: 6px; cursor: pointer; font-size: 1.2rem; font-weight: bold; margin-top: 20px; transition: 0.2s; }
        .btn:hover { transform: scale(1.05); }
    </style>
</head>
<body>
    <div class="container">
        <h1>Welcome to PuckRacer! 🏁</h1>
        <p style="color:#fff; font-size:1.1rem;">Your system has booted successfully! Perfect!</p>
        <p>Next, we need to upload all HTML, JS, and CSS files to the system.<br>Open your <b>data</b> folder on your PC, press <b>Ctrl+A</b> to select all files, and drag them into the box below.</p>
        
        <div class="drop-zone" id="dropZone">
            <span class="icon">📂</span>
            <h3 style="margin:0; color:#fff;">Drag & Drop all files here</h3>
            <p style="margin-top:5px;">or click to select files manually</p>
            <input type="file" id="fileInput" multiple>
        </div>

        <div class="progress-list" id="progressList"></div>

        <button class="btn" id="finishBtn" style="display:none;" onclick="location.reload()">Start Dashboard</button>
    </div>

    <script>
        const dropZone = document.getElementById('dropZone');
        const fileInput = document.getElementById('fileInput');
        const progressList = document.getElementById('progressList');
        const finishBtn = document.getElementById('finishBtn');

        dropZone.addEventListener('click', () => fileInput.click());
        dropZone.addEventListener('dragover', (e) => { e.preventDefault(); dropZone.classList.add('dragover'); });
        dropZone.addEventListener('dragleave', () => dropZone.classList.remove('dragover'));
        dropZone.addEventListener('drop', (e) => {
            e.preventDefault();
            dropZone.classList.remove('dragover');
            handleFiles(e.dataTransfer.files);
        });
        fileInput.addEventListener('change', (e) => handleFiles(e.target.files));

        async function handleFiles(files) {
            if (files.length === 0) return;
            dropZone.style.pointerEvents = 'none';
            dropZone.style.opacity = '0.5';
            progressList.style.display = 'block';
            
            let successCount = 0;

            for (let i = 0; i < files.length; i++) {
                const file = files[i];
                const id = 'f-' + i;
                
                progressList.insertAdjacentHTML('beforeend', `<div id="${id}" style="color:#fa0;">[WAIT] Uploading ${file.name}...</div>`);
                const el = document.getElementById(id);
                progressList.scrollTop = progressList.scrollHeight;

                const formData = new FormData();
                formData.append('file', file, file.name);

                try {
                    const response = await fetch('/upload_file', { method: 'POST', body: formData });
                    if (response.ok) {
                        el.innerHTML = `<span style="color:#0f0;">[ OK ]</span> ${file.name}`;
                        el.style.color = '#aaa';
                        successCount++;
                    } else {
                        el.innerHTML = `<span style="color:#f00;">[FAIL]</span> ${file.name} (Server Error)`;
                    }
                } catch (err) {
                    el.innerHTML = `<span style="color:#f00;">[ERR ]</span> ${file.name} (Network Error)`;
                }
            }

            dropZone.style.display = 'none';
            finishBtn.style.display = 'inline-block';
            finishBtn.innerText = `Finish & Start (${successCount}/${files.length} Files)`;
        }
    </script>
</body>
</html>
)=====";


void WebHandler::begin() {
    
    // --- BASIS ROUTING & CAPTIVE PORTAL ---
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
        if (!isClientAllowed(req)) {
            req->redirect("/only1device.html");
            return;
        }
        if (LittleFS.exists("/index.html")) {
            req->send(LittleFS, "/index.html", "text/html");
        } else {
            req->send(200, "text/html", welcomeHTML);
        }
    });

    // --- SYSTEM & PUCK STATUS ---
    server.on("/api/version", HTTP_GET, [](AsyncWebServerRequest *req){
        req->send(200, "text/plain", SYS_VER);
    });

    server.on("/api/who_is_active", HTTP_GET, [](AsyncWebServerRequest *req){
        unsigned long now = millis();
        if (lastActivityTime > 0 && (now - lastActivityTime <= SESSION_TIMEOUT_MS)) {
            req->send(200, "text/plain", activeClientName);
        } else {
            req->send(200, "text/plain", "");
        }
    });

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req){
        IPAddress ip = req->client()->remoteIP();
        if (lastActivityTime > 0 && ip == activeClientIP) lastActivityTime = millis();
        String json = "[";
        PuckInfo* p = PuckNetwork::getPucks();
        bool first = true;
        unsigned long now = millis();

        for(int i=0; i<MAX_PEERS; i++) {
            if(p[i].active) {
                if(!first) json += ",";
                unsigned long clickDiff = (p[i].lastClickTime > 0) ? (now - p[i].lastClickTime) : 999999;
                
                json += "{";
                json += "\"active\":1,"; 
                json += "\"id\":" + String(i) + ",";
                json += "\"mac\":\"" + String(p[i].mac[0], HEX) + ":" + String(p[i].mac[5], HEX) + "\",";
                json += "\"bat\":" + String(p[i].battery) + ",";
                json += "\"rssi\":" + String(p[i].rssi) + ",";
                json += "\"ver\":" + String(p[i].version) + ",";
                json += "\"lastSeen\":" + String(now - p[i].lastSeen) + ",";
                json += "\"lastClick\":" + String(clickDiff) + ",";
                json += "\"clicks\":" + String(p[i].totalClicks) + ",";
                json += "\"time\":" + String(p[i].totalMinutes);
                json += "}";
                first = false;
            }
        }
        json += "]";
        req->send(200, "application/json", json);
    });

    // --- GAME ENGINE ROUTING ---
    server.on("/api/game/status", HTTP_GET, [](AsyncWebServerRequest *req){
        Game* g = GameManager::getCurrentGame();
        if (g) req->send(200, "application/json", g->getStatusJSON());
        else req->send(200, "application/json", "{}");
    });

    server.on("/api/game/load", HTTP_GET, [](AsyncWebServerRequest *req){
        if (req->hasParam("id")) {
            GameManager::startGame(req->getParam("id")->value().toInt());
            req->send(200, "text/plain", "Loaded");
        } else req->send(400);
    });

    server.on("/api/game/action", HTTP_GET, [](AsyncWebServerRequest *req){
        Game* g = GameManager::getCurrentGame();
        if (g) {
            String cmd = req->hasParam("cmd") ? req->getParam("cmd")->value() : "";
            int val = req->hasParam("val") ? req->getParam("val")->value().toInt() : 0;
            g->processCommand(cmd, val);
            
            if (cmd == "exit") {
                StatsManager::stopPlaytime();
                StatsManager::saveAll();
            }
            
            req->send(200, "text/plain", "OK");
        } else req->send(400);
    });

    server.on("/api/stats/games", HTTP_GET, [](AsyncWebServerRequest *req){
        String json = "{";
        for(int i=1; i<=27; i++) {
            json += "\"" + String(i) + "\":" + String(StatsManager::getGameStarts(i));
            if(i<27) json += ",";
        }
        json += "}";
        req->send(200, "application/json", json);
    });

    server.on("/api/stats/playtime", HTTP_GET, [](AsyncWebServerRequest *req){
        uint32_t m = StatsManager::getTotalPlaytimeMinutes();
        req->send(200, "text/plain", String(m));
    });

// =========================================================================
    // EXTENDED PLAYER STATISTICS (CSV LOGGING)
    // =========================================================================
    // Each line in the /s_<ID>.csv has exactly this format (8 columns):
    // Timestamp;GameID;Score;Ex1;Ex2;Ex3;Ex4;Ex5
    // 
    // DATA STRUCTURE DEPENDING ON GAME-ID:
    // GameID  1 (Shuttle Run)   : Score = Time (ms)             | Ex1 = Rounds
    // GameID  3 (Countdown)     : Score = Time (ms)             | Ex1 = Clicks
    // GameID  4 (Simon Says)    : Score = Time (ms)             | Ex1 = Level
    // GameID  8 (Bomb Defusal)  : Score = Avg Deviation (ms)    | Ex1 = Best Round (ms)
    // GameID 13 (React 2P)      : Score = Reaction Time (ms)    | Ex1 = 1 (Win) or 0 (Loss)
    // GameID 14 (T-Test)        : Score = Time (ms)             | Ex1 = 0
    // GameID 15 (Target)        : Score = Hits                  | Ex1 = Time (ms)
    // GameID 21 (Catch the Light Pro) : Score = Avg Reaction (ms)     | Ex1 = Hits | Ex2 = Misses | Ex3 = Time (s) | Ex4 = Level | Ex5 = Pucks
    // GameID 24 (Whac-A-Mole)   : Score = Hits                  | Ex1 = Fails
    // GameID 25 (Katzenreflexe) : Score = Avg Reaction (ms)     | Ex1 = Points
    // =========================================================================
    // API: Checks if a CSV already exists for a list of player IDs (e.g., "1,2,3")

    server.on("/api/stats/check", HTTP_GET, [](AsyncWebServerRequest *req){
        if(!req->hasParam("pids")) {
            req->send(400, "application/json", "{\"error\":\"missing pids\"}");
            return;
        }
        
        String pidsParam = req->getParam("pids")->value();
        String json = "{";
        int start = 0;
        bool first = true;
        
        // Komma-separierte Liste aufsplitten und abarbeiten
        while(start < pidsParam.length()) {
            int idx = pidsParam.indexOf(',', start);
            if(idx == -1) idx = pidsParam.length();
            
            String pid = pidsParam.substring(start, idx);
            start = idx + 1; // Für den nächsten Durchlauf
            pid.trim();
            
            if(pid.length() > 0) {
                // Prüfen ob die Datei existiert
                String path = "/s_" + pid + ".csv";
                bool exists = LittleFS.exists(path);

                if(!first) json += ",";
                json += "\"" + pid + "\":" + (exists ? "true" : "false");
                first = false;
            }
        }
        json += "}";
        req->send(200, "application/json", json);
    });

    server.on("/api/stats/player_save", HTTP_POST, [](AsyncWebServerRequest *req){
            if (req->hasParam("pid", true) && req->hasParam("game", true) && req->hasParam("score", true) && req->hasParam("ts", true)) {
                String pid = req->getParam("pid", true)->value();
                if (pid == "0" || pid == "") { 
                    req->send(200, "text/plain", "Gastspieler, wird nicht gespeichert."); 
                    return; 
                }
                
                String ts = req->getParam("ts", true)->value();
                String game = req->getParam("game", true)->value();
                String score = req->getParam("score", true)->value();
                
                // Abwärtskompatibilität: Nimmt "ex1", ansonsten "extra" (für die anderen Spiele)
                String ex1 = "0";
                if (req->hasParam("ex1", true)) ex1 = req->getParam("ex1", true)->value();
                else if (req->hasParam("extra", true)) ex1 = req->getParam("extra", true)->value();

                String ex2 = req->hasParam("ex2", true) ? req->getParam("ex2", true)->value() : "0";
                String ex3 = req->hasParam("ex3", true) ? req->getParam("ex3", true)->value() : "0";
                String ex4 = req->hasParam("ex4", true) ? req->getParam("ex4", true)->value() : "0";
                String ex5 = req->hasParam("ex5", true) ? req->getParam("ex5", true)->value() : "0";

                // Format: Timestamp;GameID;Score;Ex1;Ex2;Ex3;Ex4;Ex5
                String line = ts + ";" + game + ";" + score + ";" + ex1 + ";" + ex2 + ";" + ex3 + ";" + ex4 + ";" + ex5 + "\n";
                String filename = "/s_" + pid + ".csv";
                
                File f = LittleFS.open(filename, "a"); // Append (Anhängen)
                if (f) {
                    f.print(line);
                    f.close();
                    req->send(200, "text/plain", "OK");
                } else {
                    req->send(500, "text/plain", "Dateisystem Fehler");
                }
            } else {
                req->send(400, "text/plain", "Fehlende Parameter");
            }
        });

    server.on("/api/stats/player_del", HTTP_GET, [](AsyncWebServerRequest *req){
        if (req->hasParam("pid")) {
            String pid = req->getParam("pid")->value();
            String filename = "/s_" + pid + ".csv";
            if (LittleFS.exists(filename)) {
                LittleFS.remove(filename);
            }
            req->send(200, "text/plain", "OK");
        } else {
            req->send(400);
        }
    });

    // --- NEU: Globale Highscore API ---
    server.on("/api/highscore/get", HTTP_GET, [](AsyncWebServerRequest *req){
        String game = req->hasParam("game") ? req->getParam("game")->value() : "";
        String path = "/hs_" + game + ".json";
        if(LittleFS.exists(path)){
            req->send(LittleFS, path, "application/json");
        } else {
            req->send(200, "application/json", "{}");
        }
    });

    server.on("/api/highscore/save", HTTP_POST, [](AsyncWebServerRequest *req){
        if(req->hasParam("game", true) && req->hasParam("data", true)) {
            String game = req->getParam("game", true)->value();
            String data = req->getParam("data", true)->value();
            File f = LittleFS.open("/hs_" + game + ".json", "w");
            if(f){
                f.print(data);
                f.close();
                req->send(200, "text/plain", "OK");
            } else {
                req->send(500, "text/plain", "FS Error");
            }
        } else {
            req->send(400, "text/plain", "Missing args");
        }
    });

    // ---------------------------------------------------------
    // API: Aktuelle Effekte der Pucks auslesen für *_names.html
    // ---------------------------------------------------------
    server.on("/api/effects", HTTP_GET, [](AsyncWebServerRequest *req){
        String json = "[";
        PuckInfo* p = PuckNetwork::getPucks();
        bool first = true;
        
        for(int i=0; i<MAX_PEERS; i++) {
            if(p[i].active && p[i].hasLastEffect) {
                if(!first) json += ",";
                json += "{";
                json += "\"id\":" + String(i) + ",";
                json += "\"eff\":" + String(p[i].lastEffect.effectID) + ",";
                json += "\"r\":" + String(p[i].lastEffect.r) + ",";
                json += "\"g\":" + String(p[i].lastEffect.g) + ",";
                json += "\"b\":" + String(p[i].lastEffect.b) + ",";
                json += "\"dur\":" + String(p[i].lastEffect.duration) + ",";
                json += "\"ex\":" + String(p[i].lastEffect.extra);
                json += "}";
                first = false;
            }
        }
        json += "]";
        req->send(200, "application/json", json);
    });

    // --- NETZWERK & EINSTELLUNGEN ---
    server.on("/api/rssi_mode", HTTP_GET, [](AsyncWebServerRequest *req){
        int val = req->hasParam("val") ? req->getParam("val")->value().toInt() : 0;
        PuckNetwork::setRssiMode(val == 1);
        req->send(200, "text/plain", val ? "RSSI ON" : "RSSI OFF");
    });
    
    server.on("/api/quiet_mode", HTTP_GET, [](AsyncWebServerRequest *req){
        if (req->hasParam("val")) {
            PuckNetwork::setQuietMode(req->getParam("val")->value().toInt() == 1);
        }
        req->send(200, "text/plain", PuckNetwork::getQuietMode() ? "1" : "0");
    });

    server.on("/api/brightness_limit", HTTP_GET, [](AsyncWebServerRequest *req){
        if (req->hasParam("val")) {
            PuckNetwork::setBrightnessLimit(req->getParam("val")->value().toInt());
        }
        req->send(200, "text/plain", String(PuckNetwork::getBrightnessLimit()));
    });

    server.on("/api/puck_reset", HTTP_GET, [](AsyncWebServerRequest *request){
        PuckNetwork::clearList();
        request->send(200, "text/plain", "OK");
    });

    server.on("/api/wifi_config", HTTP_POST, [](AsyncWebServerRequest *req){
        if(req->hasParam("ssid", true)) {
            String s = req->getParam("ssid", true)->value();
            String p = req->getParam("pw", true)->value();
            PuckNetwork::setCredentials(s, p);
            req->send(200, "text/html", "<h3>Gespeichert! Neustart...</h3>");
            delay(1000); ESP.restart(); 
        } else req->send(400);
    });

    server.on("/api/wifi_info", HTTP_GET, [](AsyncWebServerRequest *req){
        String json = "{\"ssid\":\"" + PuckNetwork::getSSID() + "\", \"secured\":" + (PuckNetwork::getPassword().length() > 0 ? "true" : "false") + "}";
        req->send(200, "application/json", json);
    });

    // --- SEQUENZIELLES OTA UPDATE ---
    server.on("/api/trigger_ota", HTTP_GET, [](AsyncWebServerRequest *req){
        Serial.println("WEB: Trigger Sequential OTA requested.");
        PuckNetwork::triggerUpdateSequential(); 
        req->send(200, "text/plain", "OK");
    });

    server.on("/api/ota_status", HTTP_GET, [](AsyncWebServerRequest *req){
        req->send(200, "application/json", PuckNetwork::getOtaStatusJSON());
    });

    
    // MP3 AUDIO MANAGEMENT (Reise nach Jerusalem)
    server.on("/api/music_info", HTTP_GET, [](AsyncWebServerRequest *req){
        String fname = "";
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while(file) {
            String n = file.name();
            if(n.endsWith(".mp3")) { fname = "/" + n; break; }
            file = root.openNextFile();
        }
        req->send(200, "application/json", "{\"file\":\"" + fname + "\"}");
    });

    server.on("/api/upload_mp3", HTTP_POST, [](AsyncWebServerRequest *req){
        req->send(200, "text/plain", "OK");
    }, [](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data, size_t len, bool final){
        if (!index) {
            File root = LittleFS.open("/");
            File file = root.openNextFile();
            while(file) {
                String fname = file.name();
                if(fname.endsWith(".mp3")) {
                    LittleFS.remove("/" + fname);
                }
                file = root.openNextFile();
            }
            if(!filename.startsWith("/")) filename = "/" + filename;
            req->_tempFile = LittleFS.open(filename, "w");
        }
        if(req->_tempFile) req->_tempFile.write(data, len);
        if(final && req->_tempFile) req->_tempFile.close();
    });


    // --- JSON DATEIVERWALTUNG (SPIELER & TRAINING) ---
    server.on("/api/players", HTTP_GET, [](AsyncWebServerRequest *req){
        if (LittleFS.exists("/players.json")) {
            req->send(LittleFS, "/players.json", "application/json");
        } else {
            req->send(200, "application/json", "{\"groups\":[]}");
        }
    });

    {
        static File _playersFile;
        static bool _playersSaveOk = false;

        server.on("/api/players", HTTP_POST,
            [](AsyncWebServerRequest *req){
                if (_playersSaveOk) req->send(200, "text/plain", "OK");
                else req->send(400, "text/plain", "FAIL");
                _playersSaveOk = false;
            },
            NULL, 
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
                if (total > 20480) return; // Limit 20KB
                if (index == 0) {
                    _playersFile = LittleFS.open("/players.json", "w");
                    _playersSaveOk = false;
                }
                if (_playersFile) _playersFile.write(data, len);
                if (index + len == total) {
                    if (_playersFile) {
                        _playersFile.close();
                        _playersSaveOk = true;
                    }
                }
            }
        );
    }

    // GET: Trainings laden
    server.on("/api/trainings", HTTP_GET, [](AsyncWebServerRequest *req){
        if (LittleFS.exists("/trainings.json")) {
            req->send(LittleFS, "/trainings.json", "application/json");
        } else {
            req->send(200, "application/json", "{\"trainings\":[]}");
        }
    });

    // POST: Trainings speichern
    {
        static File _trainingsFile;
        static bool _trainingsSaveOk = false;

        server.on("/api/trainings", HTTP_POST,
            [](AsyncWebServerRequest *req){
                if (_trainingsSaveOk) req->send(200, "text/plain", "OK");
                else req->send(400, "text/plain", "FAIL");
                _trainingsSaveOk = false;
            },
            NULL,
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
                if (total > 30720) { // Limit 30KB
                    Serial.println("TRAININGS: JSON too large, rejected!");
                    return;
                }
                if (index == 0) {
                    _trainingsFile = LittleFS.open("/trainings.json", "w");
                    _trainingsSaveOk = false;
                }
                if (_trainingsFile) _trainingsFile.write(data, len);
                if (index + len == total) {
                    if (_trainingsFile) {
                        _trainingsFile.close();
                        _trainingsSaveOk = true;
                        Serial.println("TRAININGS: Saved OK.");
                    }
                }
            }
        );
    }

    // --- SYSTEM DIAGNOSE ---
    server.on("/api/sysinfo", HTTP_GET, [](AsyncWebServerRequest *req){
        String json = "{";
        json += "\"uptime\":" + String(millis()) + ",";
        json += "\"chip\":\"" + String(ESP.getChipModel()) + "\",";
        json += "\"chipRev\":" + String(ESP.getChipRevision()) + ",";
        json += "\"cores\":" + String(ESP.getChipCores()) + ",";
        json += "\"cpuMhz\":" + String(ESP.getCpuFreqMHz()) + ",";
        json += "\"fw\":\"" + String(SYS_VER) + "\",";
        json += "\"heapFree\":" + String(ESP.getFreeHeap()) + ",";
        json += "\"heapMin\":" + String(ESP.getMinFreeHeap()) + ",";
        json += "\"lowHeapGame\":" + String(lowestHeapGameId) + ",";
        json += "\"lowHeapVal\":" + String(lowestHeapValue) + ",";
        json += "\"heapTotal\":" + String(ESP.getHeapSize()) + ",";
        json += "\"flashSize\":" + String(ESP.getFlashChipSize()) + ",";
        json += "\"sketchSize\":" + String(ESP.getSketchSize()) + ",";
        json += "\"fsUsed\":" + String(LittleFS.usedBytes()) + ",";
        json += "\"fsTotal\":" + String(LittleFS.totalBytes()) + ",";
        json += "\"ssid\":\"" + PuckNetwork::getSSID() + "\",";
        json += "\"wifiCh\":" + String(WiFi.channel()) + ",";
        json += "\"clients\":" + String(WiFi.softAPgetStationNum()) + ",";
        json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\"";
        json += "}";
        req->send(200, "application/json", json);
    });

    // NEU: DATEI EXPLORER ROUTE
    server.on("/api/fs_list", HTTP_GET, [](AsyncWebServerRequest *req){
        String json = "[";
        File root = LittleFS.open("/");
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            bool first = true;
            while(file) {
                if(!first) json += ",";
                json += "{\"name\":\"" + String(file.name()) + "\",\"size\":" + String(file.size()) + "}";
                first = false;
                file = root.openNextFile();
            }
        }
        json += "]";
        req->send(200, "application/json", json);
    });

    server.on("/api/netstats", HTTP_GET, [](AsyncWebServerRequest *req){
        NetworkStats s = PuckNetwork::getStats();
        String json = "{";
        json += "\"totalPacketsRx\":" + String(s.totalPacketsRx) + ",";
        json += "\"validEvents\":" + String(s.validEvents) + ",";
        json += "\"duplicates\":" + String(s.duplicates) + ",";
        json += "\"queueOverflows\":" + String(s.queueOverflows) + ",";
        json += "\"successfulTx\":" + String(s.successfulTx) + ",";
        json += "\"failedTx\":" + String(s.failedTx);
        json += "}";
        req->send(200, "application/json", json);
    });

    server.on("/api/netstats/reset", HTTP_GET, [](AsyncWebServerRequest *req){
        PuckNetwork::resetStats();
        req->send(200, "text/plain", "OK");
    });

    server.on("/api/heap", HTTP_GET, [](AsyncWebServerRequest *req){
        String json = "{\"free\":" + String(ESP.getFreeHeap()) + 
                      ",\"min\":" + String(ESP.getMinFreeHeap()) + "}";
        req->send(200, "application/json", json);
    });

    // --- WIFI SCANNER ---
    server.on("/api/scanner/start", HTTP_GET, [](AsyncWebServerRequest *req){
        if (WifiScanner::isScanning()) {
            req->send(409, "application/json", "{\"error\":\"scan in progress\"}");
            return;
        }
        int ch  = req->hasParam("ch")  ? req->getParam("ch")->value().toInt()  : 1;
        int dur = req->hasParam("dur") ? req->getParam("dur")->value().toInt() : 3;
        WifiScanner::startScan(ch, dur);
        req->send(200, "application/json", "{\"status\":\"started\",\"ch\":" + String(ch) + "}");
    });

    server.on("/api/scanner/status", HTTP_GET, [](AsyncWebServerRequest *req){
        if (WifiScanner::isScanning()) {
            req->send(200, "application/json", "{\"status\":\"scanning\"}");
        } else if (WifiScanner::hasResult()) {
            ScanResult r = WifiScanner::getResult();
            String json = "{\"status\":\"done\",";
            json += "\"ch\":" + String(r.channel) + ",";
            json += "\"packets\":" + String(r.packets) + ",";
            json += "\"busy_ms\":" + String(r.busy_ms) + ",";
            json += "\"total_ms\":" + String(r.total_ms) + "}";
            req->send(200, "application/json", json);
        } else {
            req->send(200, "application/json", "{\"status\":\"idle\"}");
        }
    });

    server.on("/api/scanner/restore", HTTP_GET, [](AsyncWebServerRequest *req){
        WifiScanner::restoreChannel(WIFI_CHANNEL);
        req->send(200, "text/plain", "OK");
    });

    // --- UPLOAD HANDLER ---
    server.on("/upload_puck", HTTP_POST, [](AsyncWebServerRequest *req){
        String msg = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><link rel='stylesheet' href='style.css'></head>";
        msg += "<body style='background:#121212; color:#e0e0e0; font-family:sans-serif; text-align:center; padding:20px;'>";
        msg += "<h3>Upload Status</h3>";
        msg += "<div class='card' style='text-align:left; max-width:400px; margin:0 auto;'>";
        msg += "Bytes empfangen: <b>" + String(debugUploadSize) + "</b><br>";
        
        if(debugUploadSize > 100000) msg += "<b style='color:#00ff00'>✔ Größe plausibel. Update bereit.</b>";
        else msg += "<b style='color:#ff4444'>❌ Fehler: Datei zu klein oder leer!</b>";
        
        msg += "</div><br><br>";
        msg += "<a href='/firmware_update.html' class='btn-primary' style='padding:12px 20px; text-decoration:none; display:inline-block; border-radius:6px;'>Zurück zum Update Center</a>";
        msg += "</body></html>";
        
        req->send(200, "text/html", msg);
        
    }, [](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data, size_t len, bool final){
        if(filename == "") return;

        if(!index) {
            Serial.printf("UPLOAD START: %s\n", filename.c_str());
            debugUploadSize = 0; 
            req->_tempFile = LittleFS.open("/puck_update.bin", "w");
            if(!req->_tempFile) Serial.println("CRITICAL ERROR: Konnte Datei nicht im Flash erstellen!");
        }
        
        if(req->_tempFile) {
            size_t written = req->_tempFile.write(data, len);
            debugUploadSize += written; 
            if (written != len) Serial.printf("WRITE ERROR: Wollte %u schreiben, konnte nur %u schreiben.\n", len, written);
        }
        
        if(final) {
            if(req->_tempFile) {
                req->_tempFile.close(); 
                Serial.printf("UPLOAD COMPLETE. Empfangene Bytes: %u\n", debugUploadSize);
            }
        }
    });

    server.on("/upload_file", HTTP_POST, [](AsyncWebServerRequest *req){
        String msg = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><link rel='stylesheet' href='style.css'></head>";
        msg += "<body style='background:#121212; color:#e0e0e0; font-family:sans-serif; text-align:center; padding:20px;'>";
        msg += "<h3>File Upload OK ✅</h3>";
        msg += "<p>File was stored in flash storage!</p>";
        msg += "<br><a href='/firmware_update.html' class='btn-back' style='padding:12px 20px; text-decoration:none; display:inline-block; border-radius:6px;'>Zurück</a>";
        msg += "</body></html>";
        req->send(200, "text/html", msg);
        
    }, [](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data, size_t len, bool final){
        if(!index) {
            if(!filename.startsWith("/")) filename = "/" + filename;
            req->_tempFile = LittleFS.open(filename, "w");
        }
        if(req->_tempFile) req->_tempFile.write(data, len);
        if(final && req->_tempFile) req->_tempFile.close();
    });

    server.on("/update_system", HTTP_POST, [](AsyncWebServerRequest *req){
        bool success = !Update.hasError();
        String msg = "<!DOCTYPE html><html><head><meta charset='UTF-8'><link rel='stylesheet' href='style.css'></head><body style='background:#121212; color:#e0e0e0; text-align:center; padding:50px;'>";
        if (success) {
            msg += "<h2 style='color:#0f0'>Flash sucessfull!</h2><h3>System restarts...</h3><p>Please wait 10 seconds.</p>";
            msg += "<script>setTimeout(function(){window.location.href='/firmware_update.html';}, 10000);</script>";
        } else {
            msg += "<h2 style='color:#f00'>Update not sucessfull!</h2><a href='/firmware_update.html' class='btn-back'>Back</a>";
        }
        msg += "</body></html>";
        req->send(200, "text/html", msg);
        
        if (success) { delay(1000); ESP.restart(); }
    }, [](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data, size_t len, bool final){
        if (!index) Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
        if (!Update.hasError()) Update.write(data, len);
        if (final) Update.end(true);
    });

    // Static files last — all /api/ routes are matched first
    server.serveStatic("/", LittleFS, "/")
          .setDefaultFile("index.html")
          .setCacheControl("max-age=86400");

    server.begin();

    // DNS: Alle Hostnamen → 192.168.42.1 (ermöglicht http://puck.racer und Captive Portal)
    dnsServer.start(53, "*", IPAddress(192, 168, 42, 1));
}

void WebHandler::update() {
    dnsServer.processNextRequest();
}