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
        html = '<tr><td colspan="4">No Pucks connected</td></tr>';
    } else {
        pucks.forEach((p, i) => {
            if(p.active) {
                // RSSI Farbe berechnen
                let color = '#ff0000';
                if(p.rssi >= -65) color = '#00ff00';
                else if(p.rssi >= -80) color = '#ffff00';

                html += `<tr>
                    <td>#${i + 1}</td>
                    <td>${p.bat} mV</td>
                    <td style="color:${color}">${p.rssi} dBm</td>
                    <td>v${p.ver}</td>
                </tr>`;
            }
        });
    }
    document.getElementById('puckTableBody').innerHTML = html;
}

function triggerPuckUpdate() {
    if(confirm("Force ALL Pucks into Update Mode?")) {
        fetch('/api/trigger_ota')
            .then(() => alert("Command sent!"))
            .catch(e => alert("Error: " + e));
    }
}

// === RAM WATCHDOG ===
setInterval(function() {
    fetch('/api/heap').then(r=>r.json()).then(d => {
        if (d.free < 20000 && !document.getElementById('ramWarn')) {
            let t = document.createElement('div');
            t.id = 'ramWarn';
            t.style.cssText = 'position:fixed;top:0;left:0;right:0;background:#aa0000;color:#fff;padding:10px 15px;z-index:99999;text-align:center;font-size:0.85rem;font-weight:bold;animation:fade 0.3s;';
            t.innerHTML = '⚠️ LOW MEMORY: ' + Math.round(d.free/1024) + 'KB free – System may become unstable!';
            document.body.appendChild(t);
        } else if (d.free >= 25000 && document.getElementById('ramWarn')) {
            document.getElementById('ramWarn').remove();
        }
    }).catch(()=>{});
}, 10000);

// Haptisches Feedback auf der Webseite, wenn man einen Knopf drückt.
function haptic(ms) { if(navigator.vibrate) navigator.vibrate(ms || 20); }
document.addEventListener('click', function(e) {
    if (e.target.closest('button, .btn-primary, .btn-warn, .btn-danger, .btn-scan, .tile, .tool-btn')) {
        haptic(15);
    }
});