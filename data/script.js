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
function nav(url) { location.href = _vUrl(url); }

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

// === RAM WATCHDOG ===
setInterval(function() {
    fetch('/api/heap').then(r=>r.json()).then(d => {
        if (d.free < 20000 && !document.getElementById('ramWarn')) {
            let t = document.createElement('div');
            t.id = 'ramWarn';
            t.style.cssText = 'position:fixed;top:0;left:0;right:0;background:#aa0000;color:#fff;padding:10px 15px;z-index:99999;text-align:center;font-size:0.85rem;font-weight:bold;animation:fade 0.3s;';
            t.innerHTML = getTranslation('warn_low_memory').replace('{kb}', Math.round(d.free/1024));
            document.body.appendChild(t);
        } else if (d.free >= 25000 && document.getElementById('ramWarn')) {
            document.getElementById('ramWarn').remove();
        }
    }).catch(()=>{});
}, 10000);

// === BATTERY TOAST SYSTEM ===
const _batDismissed = new Set();

function checkBatteryLevels(pucks) {
    pucks.forEach((p, i) => {
        if (!p.active) return;
        const num = i + 1;
        if (p.bat < 1000) return; // Kein Batterie-Sensor verbaut
        if (p.bat <= 3400) {
            _showBatToast(i, 'critical', (typeof getTranslation === 'function' ? getTranslation('bat_critical') : 'Puck #{n} battery empty!').replace('{n}', num), '#c53030');
        } else if (p.bat <= 3600) {
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

// Haptisches Feedback auf der Webseite, wenn man einen Knopf drückt.
function haptic(ms) { if(navigator.vibrate) navigator.vibrate(ms || 20); }
document.addEventListener('click', function(e) {
    if (e.target.closest('button, .btn-primary, .btn-warn, .btn-danger, .btn-scan, .tile, .tool-btn')) {
        haptic(15);
    }
});