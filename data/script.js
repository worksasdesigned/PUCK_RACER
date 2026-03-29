// === CACHE BUSTING ===
// Holt die Firmware-Version vom ESP und hängt ?v=VERSION an alle lokalen Links,
// Stylesheets und Script-Referenzen. So wird bei einem Firmware-Update
// automatisch der Browser-Cache umgangen.
var _fwVer = '';
fetch('/api/version')
    .then(r => r.text())
    .then(ver => {
        _fwVer = ver.trim().replace(/^v/, '');
        console.log('[CacheBust] v' + _fwVer);
        document.querySelectorAll('a[href]').forEach(el => {
            const h = el.getAttribute('href');
            if (h && !h.startsWith('http') && !h.startsWith('#') && !h.startsWith('mailto:'))
                el.setAttribute('href', _vUrl(h));
        });
        document.querySelectorAll('link[rel="stylesheet"]').forEach(el => {
            const h = el.getAttribute('href');
            if (h && !h.startsWith('http')) el.setAttribute('href', _vUrl(h));
        });
    })
    .catch(() => {});

function _vUrl(url) { return url.split('?')[0] + (_fwVer ? '?v=' + _fwVer : ''); }

function nav(url) {
    // Auto-save: Formularwerte sichern wenn man eine Setup-Seite verlässt
    if (_isSetupPage()) _saveFormState();
    // Navigation zum Dashboard → gespeicherte Formwerte löschen (frischer Start)
    if (url === '/' || url === '/index.html')
        Object.keys(sessionStorage).forEach(k => { if (k.startsWith('_fs_')) sessionStorage.removeItem(k); });
    location.href = _vUrl(url);
}

// === FORM STATE PERSISTENCE ===
// Speichert/stellt Formularwerte auf game_*_setup Seiten automatisch wieder her.
// Eigener _fs_ Namespace, kollidiert nicht mit tc_ (Training) oder Game-Keys.
function _isSetupPage() {
    return location.pathname.replace(/\\/g, '/').split('/').pop().indexOf('_setup') > -1;
}
function _fsPrefix() {
    var m = location.pathname.replace(/\\/g, '/').split('/').pop().match(/^game_([^_]+)_/);
    return m ? '_fs_' + m[1] + '_' : '';
}
function _saveFormState() {
    var px = _fsPrefix(); if (!px) return;
    document.querySelectorAll('input[id], select[id]').forEach(function(el) {
        if (el.type === 'hidden' || el.type === 'button' || el.type === 'submit') return;
        sessionStorage.setItem(px + el.id, el.type === 'checkbox' ? (el.checked ? '1' : '0') : el.value);
    });
}
function _restoreFormState() {
    var px = _fsPrefix(); if (!px) return;
    var found = false;
    document.querySelectorAll('input[id], select[id]').forEach(function(el) {
        var val = sessionStorage.getItem(px + el.id);
        if (val === null) return;
        if (el.type === 'checkbox') el.checked = val === '1';
        else el.value = val;
        try { el.dispatchEvent(new Event('input', {bubbles:true})); el.dispatchEvent(new Event('change', {bubbles:true})); } catch(e) {}
        found = true;
    });
    if (found) console.log('[FormRestore] ' + px);
}
// Auto-Restore auf Setup-Seiten (nicht im Training-Modus)
if (_isSetupPage() && !sessionStorage.getItem('training_config_id') && !sessionStorage.getItem('training_play_mode')) {
    setTimeout(_restoreFormState, 500);
}

// Globale Funktion zum Laden des Status
function startStatusLoop() {
    updateStatus();
    setInterval(updateStatus, 1000);
}

function updateStatus() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            updateHeader(data);
            if(document.getElementById('puckTableBody')) {
                updateSettingsTable(data);
            }
            checkBatteryLevels(data);
            checkTemperature(data);
        })
        .catch(err => console.error("API Error:", err));
}

function updateHeader(pucks) {
    let good = 0, mid = 0, bad = 0;

    pucks.forEach(p => {
        if (p.active) {
            if (p.rssi >= -65) good++;
            else if (p.rssi >= -80) mid++;
            else bad++;
        }
    });

    document.getElementById('cnt-green').innerText = good;
    document.getElementById('cnt-yellow').innerText = mid;
    document.getElementById('cnt-red').innerText = bad;
}

function updateSettingsTable(pucks) {
    let html = '';
    if (pucks.length === 0) {
        html = `<tr><td colspan="4">${getTranslation('err_no_pucks')}</td></tr>`;
    } else {
        pucks.forEach((p, i) => {
            if(p.active) {
                // RSSI Farbe berechnen
                let color = '#ff0000';
                if(p.rssi >= -65) color = '#00ff00';
                else if(p.rssi >= -80) color = '#ffff00';

                let batText = p.bat < 1000 ? '???' : p.bat + ' mV';
                html += `<tr>
                    <td>#${i + 1}</td>
                    <td>${batText}</td>
                    <td style="color:${color}">${p.rssi} dBm</td>
                    <td>v${p.ver}</td>
                </tr>`;
            }
        });
    }
    document.getElementById('puckTableBody').innerHTML = html;
}

function triggerPuckUpdate() {
    if(confirm(getTranslation('confirm_force_ota'))) {
        fetch('/api/trigger_ota')
            .then(() => alert(getTranslation('alert_cmd_sent')))
            .catch(e => alert(getTranslation('alert_error') + e));
    }
}

// === SYSTEM PREFERENCES ===
function _getSysPrefs() {
    try { return JSON.parse(localStorage.getItem('sys_prefs') || '{}'); } catch(e) { return {}; }
}

// === RAM WATCHDOG ===
setInterval(function() {
    if (!_getSysPrefs().heap_warn) return;
    fetch('/api/heap').then(r=>r.json()).then(d => {
        if (d.free < 20000 && !document.getElementById('ramWarn')) {
            _showSysToast('ramWarn', (typeof getTranslation === 'function' ? getTranslation('warn_low_memory') : 'LOW MEMORY: {kb}KB free').replace('{kb}', Math.round(d.free/1024)), '#aa0000');
        } else if (d.free >= 25000 && document.getElementById('ramWarn')) {
            document.getElementById('ramWarn').remove();
        }
    }).catch(()=>{});
}, 10000);

// === LITTLEFS STORAGE CHECK ===
// Prüft einmalig ob der Speicherplatz knapp ist (nur auf index.html und game_musical_setup.html)
(function() {
    var page = location.pathname.replace(/\\/g, '/').split('/').pop() || 'index.html';
    if (page !== 'index.html' && page !== 'game_musical_setup.html') return;
    if (!_getSysPrefs().fs_warn) return;
    fetch('/api/sysinfo').then(r => r.json()).then(d => {
        if (!d.fsTotal || !d.fsUsed) return;
        var freePct = Math.round((1 - d.fsUsed / d.fsTotal) * 100);
        var freeKB = Math.round((d.fsTotal - d.fsUsed) / 1024);
        if (freePct < 15) {
            _showSysToast('fsWarn', (typeof getTranslation === 'function' ? getTranslation('warn_low_storage') : 'LOW STORAGE: {kb}KB free ({pct}%)').replace('{kb}', freeKB).replace('{pct}', freePct), '#b45309');
        }
    }).catch(()=>{});
})();

// === BATTERY TOAST SYSTEM ===
const _batDismissed = new Set();

function checkBatteryLevels(pucks) {
    var prefs = _getSysPrefs();
    var critMv = prefs.bat_crit_mv || 3400;
    var warnMv = prefs.bat_warn_mv || 3600;
    pucks.forEach((p, i) => {
        if (!p.active) return;
        const num = i + 1;
        if (p.bat < 1000) return;
        if (p.bat <= critMv && prefs.bat_crit !== false) {
            _showBatToast(i, 'critical', (typeof getTranslation === 'function' ? getTranslation('bat_critical') : 'Puck #{n} battery empty!').replace('{n}', num), '#c53030');
        } else if (p.bat <= warnMv && prefs.bat_low !== false) {
            _showBatToast(i, 'warning', (typeof getTranslation === 'function' ? getTranslation('bat_warning') : 'Puck #{n} battery low!').replace('{n}', num), '#c05621');
        }
    });
}

function _showBatToast(puckIdx, level, msg, color) {
    const key = puckIdx + '-' + level;
    if (_batDismissed.has(key)) return;
    if (document.getElementById('bat-t-' + key)) return;

    let c = document.getElementById('toast-container');
    if (!c) {
        c = document.createElement('div');
        c.id = 'toast-container';
        c.style.cssText = 'position:fixed;top:60px;right:15px;z-index:100000;display:flex;flex-direction:column;gap:8px;max-width:340px;';
        document.body.appendChild(c);
    }

    const t = document.createElement('div');
    t.id = 'bat-t-' + key;
    t.style.cssText = 'display:flex;align-items:center;justify-content:space-between;padding:10px 14px;border-radius:8px;color:#fff;font-size:0.85rem;font-weight:bold;box-shadow:0 4px 12px rgba(0,0,0,0.3);animation:toastIn 0.3s ease;background:' + color + ';';
    t.innerHTML = '<span>' + msg + '</span><button onclick="_dismissBatToast(\'' + key + '\')" style="background:none;border:none;color:#fff;font-size:1.2rem;cursor:pointer;margin-left:12px;padding:0 4px;">✕</button>';
    c.appendChild(t);
    if (navigator.vibrate) navigator.vibrate([100, 50, 100]);
}

function _dismissBatToast(key) {
    _batDismissed.add(key);
    const el = document.getElementById('bat-t-' + key);
    if (el) el.remove();
}

function _showSysToast(id, msg, color) {
    if (document.getElementById(id)) return;
    var c = document.getElementById('toast-container');
    if (!c) {
        c = document.createElement('div');
        c.id = 'toast-container';
        c.style.cssText = 'position:fixed;top:60px;right:15px;z-index:100000;display:flex;flex-direction:column;gap:8px;max-width:340px;';
        document.body.appendChild(c);
    }
    var t = document.createElement('div');
    t.id = id;
    t.style.cssText = 'display:flex;align-items:center;justify-content:space-between;padding:10px 14px;border-radius:8px;color:#fff;font-size:0.85rem;font-weight:bold;box-shadow:0 4px 12px rgba(0,0,0,0.3);animation:toastIn 0.3s ease;background:' + color + ';';
    t.innerHTML = '<span>' + msg + '</span><button onclick="this.parentElement.remove()" style="background:none;border:none;color:#fff;font-size:1.2rem;cursor:pointer;margin-left:12px;padding:0 4px;">&#10005;</button>';
    c.appendChild(t);
}

// === TEMPERATURE OVERHEAT WARNING ===
const _tempAlertActive = new Set();
var _tempAudioCtx = null;

function checkTemperature(pucks) {
    pucks.forEach(function(p, i) {
        if (!p.active) return;
        var num = i + 1;
        // temp ist in 0.1°C Einheiten, -999 = kein Sensor
        if (p.temp === -999 || p.temp === undefined) {
            // Sensor nicht vorhanden oder alte Firmware → Warnung entfernen falls vorhanden
            _removeTempToast(i);
            return;
        }
        // Schwelle: 600 = 60.0°C (in 0.1°C Einheiten)
        if (p.temp >= 600) {
            if (!_tempAlertActive.has(i)) {
                _tempAlertActive.add(i);
                _showTempWarning(i, num, (p.temp / 10).toFixed(1));
                _playTempAlarm();
            }
        } else if (p.temp < 550) {
            // Hysterese: erst unter 55°C entwarnen
            _removeTempToast(i);
        }
    });
}

function _showTempWarning(puckIdx, puckNum, tempC) {
    var id = 'temp-warn-' + puckIdx;
    if (document.getElementById(id)) return;

    var c = document.getElementById('toast-container');
    if (!c) {
        c = document.createElement('div');
        c.id = 'toast-container';
        c.style.cssText = 'position:fixed;top:60px;right:15px;z-index:100000;display:flex;flex-direction:column;gap:8px;max-width:340px;';
        document.body.appendChild(c);
    }

    var t = document.createElement('div');
    t.id = id;
    t.style.cssText = 'display:flex;align-items:center;justify-content:space-between;padding:12px 14px;border-radius:8px;color:#fff;font-size:0.9rem;font-weight:bold;box-shadow:0 4px 12px rgba(0,0,0,0.4);background:#cc0000;animation:toastIn 0.3s ease, tempBlink 0.8s ease-in-out infinite;';
    var msg = (typeof getTranslation === 'function' ? getTranslation('temp_overheat') : 'PUCK #{n} hat Temperatur Probleme ({t}\u00b0C)').replace('{n}', puckNum).replace('{t}', tempC);
    t.innerHTML = '<span>' + msg + '</span><button onclick="_dismissTempToast(' + puckIdx + ')" style="background:none;border:none;color:#fff;font-size:1.2rem;cursor:pointer;margin-left:12px;padding:0 4px;">&#10005;</button>';
    c.appendChild(t);
    if (navigator.vibrate) navigator.vibrate([200, 100, 200, 100, 200]);
}

function _removeTempToast(puckIdx) {
    _tempAlertActive.delete(puckIdx);
    var el = document.getElementById('temp-warn-' + puckIdx);
    if (el) el.remove();
}

function _dismissTempToast(puckIdx) {
    _removeTempToast(puckIdx);
}

function _playTempAlarm() {
    try {
        if (!_tempAudioCtx) _tempAudioCtx = new (window.AudioContext || window.webkitAudioContext)();
        var ctx = _tempAudioCtx;
        var now = ctx.currentTime;
        // 3x kurzer Warnton: 200ms an, 100ms aus
        for (var i = 0; i < 3; i++) {
            var osc = ctx.createOscillator();
            var gain = ctx.createGain();
            osc.type = 'square';
            osc.frequency.value = 1000;
            gain.gain.value = 0.3;
            osc.connect(gain);
            gain.connect(ctx.destination);
            osc.start(now + i * 0.3);
            osc.stop(now + i * 0.3 + 0.2);
        }
    } catch(e) {
        // Web Audio API nicht verfügbar oder blockiert
    }
}

// CSS für blinkende Temperatur-Warnung einfügen
(function() {
    var s = document.createElement('style');
    s.textContent = '@keyframes tempBlink { 0%,100% { opacity:1; } 50% { opacity:0.4; } }';
    document.head.appendChild(s);
})();

// === QUIET MODE INDICATOR ===
// Zeigt 🔇 oben links auf jeder Seite wenn Quiet Mode aktiv ist.
fetch('/api/quiet_mode').then(r => r.text()).then(v => {
    if (v.trim() === '1') _showQuietBadge();
}).catch(() => {});

function _showQuietBadge() {
    if (document.getElementById('quietBadge')) return;
    var b = document.createElement('div');
    b.id = 'quietBadge';
    b.style.cssText = 'font-size:1.1rem;opacity:0.7;margin-left:4px;';
    b.textContent = '\uD83D\uDD07';
    var logo = document.querySelector('.logo');
    if (logo) logo.insertAdjacentElement('afterend', b);
}
function _removeQuietBadge() {
    var b = document.getElementById('quietBadge');
    if (b) b.remove();
}
function toggleQuietMode(on) {
    return fetch('/api/quiet_mode?val=' + (on ? '1' : '0'))
        .then(r => r.text())
        .then(v => {
            if (v.trim() === '1') _showQuietBadge();
            else _removeQuietBadge();
            return v.trim() === '1';
        });
}

// Haptisches Feedback auf der Webseite, wenn man einen Knopf drückt.
function haptic(ms) { if(navigator.vibrate) navigator.vibrate(ms || 20); }
document.addEventListener('click', function(e) {
    if (e.target.closest('button, .btn-primary, .btn-warn, .btn-danger, .btn-scan, .tile, .tool-btn')) {
        haptic(15);
    }
});

// === NEU: LIZENZPRÜFUNG VOR SPIELSTART ===
function checkLicenseAndNavigate(url) {
    fetch('/api/license')
        .then(response => response.json())
        .then(license => {
            // Prüfen, ob die Testphase abgelaufen ist
            if (license.status !== 'FULL' && (license.playtime_hours * 60) >= license.playtime_limit_minutes) {
                // Die vom User gewünschte Warnmeldung
                const message = "Die Testphase ist abgelaufen. Offensichtlich macht das Spiel ja Spaß! Möchtest du einmalig die Vollversion kaufen und weiterspielen?";
                
                if (confirm(message)) {
                    // Nutzer zur Registrierungsseite weiterleiten
                    nav('/register.html');
                }
                // Wenn der Nutzer "Abbrechen" drückt, passiert nichts.

            } else {
                // Lizenz ist gültig oder Zeitlimit nicht erreicht -> zum Spiel weiterleiten
                nav(url);
            }
        })
        .catch(error => {
            console.error('Fehler bei der Lizenzprüfung:', error);
            // Im Fehlerfall den Nutzer sicherheitshalber trotzdem zum Spiel lassen
            nav(url);
        });
}