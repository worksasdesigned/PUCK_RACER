// ============================================================================
//  training_config.js — Training Configuration & Play Mode Helper
//  
//  Wird von allen game_*_setup.html Seiten eingebunden.
//  Erkennt automatisch ob die Seite im Training-Konfigurationsmodus geöffnet
//  wurde (via sessionStorage 'training_config_id') und:
//   - Zeigt ein Info-Banner
//   - Ersetzt den NEXT/START Button durch "Einstellungen speichern"
//   - Ändert den Back-Button zur Rückkehr zum Training Manager
//   - Speichert die Formular-Werte in training_config_return
//   - Stellt gespeicherte Werte wieder her (Restore)
//
//  Erkennt auch den Play-Modus (via sessionStorage 'training_play_mode'):
//   - Stellt nur gespeicherte Werte wieder her (Restore)
//   - Keine UI-Änderungen (kein Banner, kein Save-Button)
//
//  Jede Setup-Seite muss VOR diesem Script window.TRAINING_FIELDS definieren:
//    window.TRAINING_FIELDS = [
//      { key:'mySlider',  el:'mySlider' },                  // range input
//      { key:'myCheck',   el:'myCheck',   type:'check' },   // checkbox
//      { key:'mySelect',  el:'mySelect',  type:'select' },  // <select>
//      { key:'myMode',    get:()=>myVar,  set:(v)=>{...} }  // custom getter/setter
//    ];
// ============================================================================
(function() {
    var tcId = sessionStorage.getItem('training_config_id');
    var playMode = sessionStorage.getItem('training_play_mode');

    // Weder Config- noch Play-Modus → nichts tun
    if (!tcId && !playMode) return;

    var tcIdx = sessionStorage.getItem('training_config_idx');

    // --- RESTORE: Gespeicherte Werte in Formular laden ---
    function doRestore() {
        var fields = window.TRAINING_FIELDS;
        if (!fields || !fields.length) return;

        fields.forEach(function(f) {
            var val = sessionStorage.getItem('tc_' + f.key);
            if (val === null) return;

            // Custom setter
            if (typeof f.set === 'function') {
                try { f.set(val); } catch(e) { console.warn('TC restore error:', f.key, e); }
                return;
            }

            var el = document.getElementById(f.el);
            if (!el) return;

            if (f.type === 'check') {
                el.checked = (val === '1' || val === 'true');
            } else {
                el.value = val;
            }

            // Trigger handlers to update display
            try {
                el.dispatchEvent(new Event('input', {bubbles:true}));
                el.dispatchEvent(new Event('change', {bubbles:true}));
            } catch(e) {}
        });
    }

    // --- COLLECT: Aktuelle Formular-Werte sammeln ---
    function collectSettings() {
        var settings = {};
        var fields = window.TRAINING_FIELDS;
        if (!fields || !fields.length) return settings;

        fields.forEach(function(f) {
            // Custom getter
            if (typeof f.get === 'function') {
                settings['tc_' + f.key] = String(f.get());
                return;
            }

            var el = document.getElementById(f.el);
            if (!el) return;

            if (f.type === 'check') {
                settings['tc_' + f.key] = el.checked ? '1' : '0';
            } else {
                settings['tc_' + f.key] = String(el.value);
            }
        });
        return settings;
    }

    // --- UI: Banner, Save Button, Back Button (nur Config-Modus) ---
    function setupUI() {
        var main = document.querySelector('main');
        if (!main) return;

        // 1) Info Banner oben
        var banner = document.createElement('div');
        banner.style.cssText = 'background:rgba(0,255,0,0.1); border:1px solid var(--success); border-radius:8px; padding:10px 16px; margin-bottom:16px; text-align:center; font-size:0.9rem; color:var(--success);';
        var bTitle = (typeof getTranslation === 'function') ? getTranslation('tm_cfg_banner') : 'Training Configuration';
        var bSub = (typeof getTranslation === 'function') ? getTranslation('tm_cfg_banner_sub') : 'Settings will be saved to the training';
        banner.innerHTML = '⚙️ <b>' + bTitle + '</b> \u2014 ' + bSub;
        main.insertBefore(banner, main.firstChild);

        // 2) Finde die action buttons am Ende von <main>
        //    Wir suchen direkte Kinder von main die .btn-primary oder .btn-back sind
        var children = main.children;
        var primaryBtn = null;
        var backBtn = null;

        for (var i = 0; i < children.length; i++) {
            var ch = children[i];
            if (ch.tagName !== 'BUTTON') continue;
            if (ch.classList.contains('btn-primary') && !primaryBtn) primaryBtn = ch;
            if (ch.classList.contains('btn-back') && !backBtn) backBtn = ch;
        }

        // 3) Speichern-Button einfügen
        var saveBtn = document.createElement('button');
        saveBtn.className = 'btn-primary';
        saveBtn.style.cssText = 'font-size:1.3rem; margin-top:20px; width:100%; background:var(--success); border:none; color:#000;';
        saveBtn.innerHTML = (typeof getTranslation === 'function') ? getTranslation('tm_cfg_save') : '💾 Save Settings for Training';
        saveBtn.onclick = function() {
            var settings = collectSettings();
            var returnObj = {
                tid: parseInt(tcId),
                idx: parseInt(tcIdx),
                settings: settings
            };
            sessionStorage.setItem('training_config_return', JSON.stringify(returnObj));
            sessionStorage.removeItem('training_config_id');
            sessionStorage.removeItem('training_config_idx');
            location.href = '/training.html';
        };

        // Primären Button verstecken und Save-Button davor einfügen
        if (primaryBtn) {
            primaryBtn.style.display = 'none';
            main.insertBefore(saveBtn, primaryBtn);
        } else if (backBtn) {
            main.insertBefore(saveBtn, backBtn);
        } else {
            main.appendChild(saveBtn);
        }

        // 4) Back Button umleiten
        if (backBtn) {
            backBtn.setAttribute('onclick', '');
            backBtn.onclick = function(e) {
                e.preventDefault();
                e.stopPropagation();
                sessionStorage.removeItem('training_config_id');
                sessionStorage.removeItem('training_config_idx');
                location.href = '/training.html';
            };
            var backLbl = (typeof getTranslation === 'function') ? getTranslation('tm_back_training') : '↩ Back to Training';
            backBtn.innerHTML = backLbl;
        }
    }

    // --- INIT ---
    function initConfig() {
        setupUI();
        setTimeout(doRestore, 300);
    }

    function initPlay() {
        // Play-Modus: nur Werte wiederherstellen, keine UI-Änderungen
        sessionStorage.removeItem('training_play_mode');
        setTimeout(doRestore, 300);
    }

    // Warte auf DOMContentLoaded + kurzer Delay damit die Seite ihre
    // eigenen Init-Funktionen (fetch, Slider-Limits etc.) ausführen kann
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', function() {
            if (tcId) initConfig();
            else if (playMode) initPlay();
        });
    } else {
        if (tcId) initConfig();
        else if (playMode) initPlay();
    }
})();
