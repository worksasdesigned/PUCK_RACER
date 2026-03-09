/*
 * PLAYER PICKER - Wiederverwendbare Spieler-Auswahl-Komponente
 * Version: 1.0
 * 
 * Einbindung in jede *_names.html:
 *   <script src="player_picker.js"></script>
 *   <button onclick="openPlayerPicker(numSlots, onPlayersSelected)" class="btn-primary">👥 Spieler laden</button>
 *
 * Callback: onPlayersSelected([{id: 101, name: "Lena M."}, ...])
 */

const PlayerPicker = (function() {

    let _overlay = null;
    let _data = null;       // { groups: [...] }
    let _numSlots = 0;
    let _callback = null;
    let _selected = [];     // [{id, name}, ...]
    let _currentGroupId = null;

    // ==================== SANITIZATION ====================
    function sanitize(str, maxLen) {
        if (typeof str !== 'string') return '';
        return str
            .replace(/[{}\[\]"'\\<>&`]/g, '')
            .replace(/\s+/g, ' ')
            .trim()
            .substring(0, maxLen || 20);
    }

    // ==================== UI ERSTELLEN ====================
    function createOverlay() {
        if (_overlay) _overlay.remove();

        _overlay = document.createElement('div');
        _overlay.id = 'playerPickerOverlay';
        _overlay.innerHTML = `
            <div class="pp-sheet">
                <div class="pp-header">
                    <span class="pp-title">👥 ${_tr('pp_title')}</span>
                    <button class="pp-close" onclick="PlayerPicker.close()">✕</button>
                </div>
                <div class="pp-body">
                    <div class="pp-group-bar">
                        <select id="ppGroupSelect" onchange="PlayerPicker.selectGroup(this.value)">
                            <option value="">— ${_tr('pp_select_group')} —</option>
                        </select>
                    </div>
                    <div class="pp-counter" id="ppCounter"></div>
                    <div class="pp-list" id="ppPlayerList">
                        <div class="pp-hint">${_tr('pp_hint')}</div>
                    </div>
                </div>
                <div class="pp-footer">
                    <button class="pp-btn-cancel" onclick="PlayerPicker.close()">${_tr('btn_cancel')}</button>
                    <button class="pp-btn-apply" id="ppApplyBtn" onclick="PlayerPicker.apply()" disabled>${_tr('pp_apply')}</button>
                </div>
            </div>
        `;
        document.body.appendChild(_overlay);

        // Animation: einblenden
        requestAnimationFrame(() => _overlay.classList.add('pp-visible'));
    }

    // ==================== DATEN LADEN ====================
    function loadData() {
        fetch('/api/players')
            .then(r => r.json())
            .then(data => {
                _data = data;
                populateGroups();
                // Letzte Gruppe wiederherstellen
                let lastGrp = sessionStorage.getItem('pp_lastGroup');
                if (lastGrp) {
                    let sel = document.getElementById('ppGroupSelect');
                    sel.value = lastGrp;
                    if (sel.value === lastGrp) selectGroup(lastGrp);
                }
            })
            .catch(e => {
                console.error('PlayerPicker: Load failed', e);
                document.getElementById('ppPlayerList').innerHTML = 
                    '<div class="pp-hint" style="color:var(--danger)">Fehler beim Laden!</div>';
            });
    }

    function populateGroups() {
        let sel = document.getElementById('ppGroupSelect');
        if (!_data || !_data.groups) return;
        _data.groups.forEach(g => {
            let opt = document.createElement('option');
            opt.value = g.id;
            opt.textContent = `${g.name} (${g.players.length})`;
            sel.appendChild(opt);
        });
    }

    // ==================== GRUPPE AUSWÄHLEN ====================
    function selectGroup(groupId) {
        _currentGroupId = parseInt(groupId);
        _selected = [];
        sessionStorage.setItem('pp_lastGroup', groupId);

        let group = _data.groups.find(g => g.id === _currentGroupId);
        let container = document.getElementById('ppPlayerList');

        if (!group || !group.players.length) {
            container.innerHTML = '<div class="pp-hint">' + _tr('pp_empty') + '</div>';
            updateCounter();
            return;
        }

        // Letzte Auswahl wiederherstellen
        let lastSel = JSON.parse(sessionStorage.getItem('pp_lastSelected_' + groupId) || '[]');

        let html = '';
        group.players.forEach(p => {
            let checked = lastSel.includes(p.id);
            if (checked) _selected.push({id: p.id, name: p.name});
            html += `
                <label class="pp-player ${checked ? 'pp-checked' : ''}" id="pp-p-${p.id}">
                    <input type="checkbox" ${checked ? 'checked' : ''} 
                           onchange="PlayerPicker.togglePlayer(${p.id}, '${sanitize(p.name, 20).replace(/'/g, "\\'")}', this.checked)">
                    <span class="pp-check">${checked ? '✓' : ''}</span>
                    <span class="pp-name">${sanitize(p.name, 20)}</span>
                </label>`;
        });

        // "Alle auswählen" Toggle
        html = `<label class="pp-select-all" onclick="PlayerPicker.toggleAll()">
                    <span id="ppToggleAllTxt">${_tr('pp_select_all')}</span>
                </label>` + html;

        container.innerHTML = html;
        updateCounter();
    }

    // ==================== SPIELER TOGGLEN ====================
    function togglePlayer(id, name, checked) {
        if (checked) {
            if (!_selected.find(s => s.id === id)) {
                _selected.push({id, name});
            }
        } else {
            _selected = _selected.filter(s => s.id !== id);
        }

        // Visual Feedback
        let label = document.getElementById('pp-p-' + id);
        if (label) {
            label.classList.toggle('pp-checked', checked);
            label.querySelector('.pp-check').textContent = checked ? '✓' : '';
        }

        updateCounter();
    }

    function toggleAll() {
        let group = _data.groups.find(g => g.id === _currentGroupId);
        if (!group) return;

        let allSelected = _selected.length === group.players.length;
        
        if (allSelected) {
            // Deselect all
            _selected = [];
            group.players.forEach(p => {
                let cb = document.querySelector(`#pp-p-${p.id} input`);
                if (cb) cb.checked = false;
                let label = document.getElementById('pp-p-' + p.id);
                if (label) { label.classList.remove('pp-checked'); label.querySelector('.pp-check').textContent = ''; }
            });
        } else {
            // Select all
            _selected = group.players.map(p => ({id: p.id, name: p.name}));
            group.players.forEach(p => {
                let cb = document.querySelector(`#pp-p-${p.id} input`);
                if (cb) cb.checked = true;
                let label = document.getElementById('pp-p-' + p.id);
                if (label) { label.classList.add('pp-checked'); label.querySelector('.pp-check').textContent = '✓'; }
            });
        }
        updateCounter();
    }

    // ==================== COUNTER + VALIDIERUNG ====================
    function updateCounter() {
        let el = document.getElementById('ppCounter');
        let btn = document.getElementById('ppApplyBtn');
        let n = _selected.length;

        let cls = '';
        if (n === 0) { cls = 'pp-cnt-empty'; }
        else if (n > _numSlots) { cls = 'pp-cnt-warn'; }
        else if (n === _numSlots) { cls = 'pp-cnt-ok'; }
        else { cls = 'pp-cnt-partial'; }

        let warnText = '';
        if (n > _numSlots) {
            warnText = `<div class="pp-warn">⚠️ ${_tr('pp_too_many').replace('{n}', n - _numSlots)}</div>`;
        }

        el.innerHTML = `<span class="${cls}">✓ ${n} / ${_numSlots}</span>${warnText}`;
        btn.disabled = (n === 0);
    }

    // ==================== APPLY ====================
    function apply() {
        // Auswahl in sessionStorage merken
        if (_currentGroupId) {
            sessionStorage.setItem('pp_lastSelected_' + _currentGroupId, 
                JSON.stringify(_selected.map(s => s.id)));
        }

        // Auf numSlots begrenzen
        let result = _selected.slice(0, _numSlots);

        close();
        if (_callback) _callback(result);
    }

    // ==================== CLOSE ====================
    function close() {
        if (_overlay) {
            _overlay.classList.remove('pp-visible');
            setTimeout(() => { if (_overlay) _overlay.remove(); _overlay = null; }, 300);
        }
    }

    // ==================== i18n HELPER ====================
    function _tr(key) {
        if (typeof getTranslation === 'function') return getTranslation(key);
        // Fallback Englisch
        const fb = {
            'pp_title': 'Select Players',
            'pp_select_group': 'Select Group',
            'pp_hint': 'Select a group to see players',
            'pp_empty': 'No players in this group',
            'pp_select_all': '☐ Select All / Deselect All',
            'pp_apply': '✓ APPLY',
            'pp_too_many': '{n} too many — extras will be skipped',
            'btn_cancel': 'Cancel'
        };
        return fb[key] || key;
    }

    // ==================== PUBLIC API ====================
    return {
        open: function(numSlots, callback) {
            _numSlots = numSlots;
            _callback = callback;
            _selected = [];
            _currentGroupId = null;
            createOverlay();
            loadData();
        },
        close: close,
        selectGroup: selectGroup,
        togglePlayer: togglePlayer,
        toggleAll: toggleAll,
        apply: apply
    };
})();

// Globale Convenience-Funktion
function openPlayerPicker(numSlots, callback) {
    PlayerPicker.open(numSlots, callback);
}

// ==================== STYLES (einmalig injiziert) ====================
(function injectPickerCSS() {
    if (document.getElementById('ppStyles')) return;
    let s = document.createElement('style');
    s.id = 'ppStyles';
    s.textContent = `
        #playerPickerOverlay {
            position:fixed; top:0; left:0; width:100%; height:100%;
            background:rgba(0,0,0,0.7); backdrop-filter:blur(4px);
            z-index:5000; display:flex; align-items:flex-end; justify-content:center;
            opacity:0; transition:opacity 0.3s;
        }
        #playerPickerOverlay.pp-visible { opacity:1; }
        .pp-sheet {
            background:var(--card-bg, #1e1e1e); border:1px solid var(--border-color, #333);
            border-radius:16px 16px 0 0; width:100%; max-width:500px;
            max-height:85vh; display:flex; flex-direction:column;
            transform:translateY(100%); transition:transform 0.3s ease-out;
            box-shadow:0 -10px 40px rgba(0,0,0,0.6);
        }
        .pp-visible .pp-sheet { transform:translateY(0); }
        .pp-header {
            display:flex; justify-content:space-between; align-items:center;
            padding:16px 20px; border-bottom:1px solid var(--border-color, #333);
        }
        .pp-title { font-size:1.2rem; font-weight:bold; color:var(--text-main, #e0e0e0); }
        .pp-close {
            background:none; border:none; color:var(--text-muted, #888);
            font-size:1.5rem; cursor:pointer; padding:0; margin:0; width:auto; line-height:1;
        }
        .pp-body { flex:1; overflow-y:auto; padding:12px 20px; }
        .pp-group-bar { margin-bottom:12px; }
        .pp-group-bar select {
            width:100%; padding:12px; font-size:1.1rem;
            background:var(--bg-color, #121212); color:var(--text-main, #e0e0e0);
            border:1px solid var(--border-color, #333); border-radius:8px; outline:none;
        }
        .pp-counter {
            text-align:center; padding:8px 0; font-size:1.1rem; font-weight:bold;
        }
        .pp-cnt-empty { color:var(--text-muted, #888); }
        .pp-cnt-partial { color:var(--warning, #ffc107); }
        .pp-cnt-ok { color:var(--success, #00ff00); }
        .pp-cnt-warn { color:var(--danger, #dc3545); }
        .pp-warn {
            font-size:0.85rem; color:var(--danger, #dc3545);
            margin-top:4px; font-weight:normal;
        }
        .pp-hint { text-align:center; color:var(--text-muted, #888); padding:30px 10px; }
        .pp-select-all {
            display:block; text-align:center; padding:10px;
            color:var(--primary, #0d6efd); cursor:pointer; font-size:0.9rem;
            border-bottom:1px solid var(--border-color, #333); margin-bottom:6px;
            user-select:none;
        }
        .pp-player {
            display:flex; align-items:center; gap:12px;
            padding:14px 12px; margin-bottom:4px;
            background:var(--bg-color, #121212); border-radius:8px;
            border:2px solid transparent; cursor:pointer;
            transition:border-color 0.15s, background 0.15s;
            user-select:none;
        }
        .pp-player input { display:none; }
        .pp-player.pp-checked {
            border-color:var(--primary, #0d6efd);
            background:rgba(13,110,253,0.1);
        }
        .pp-check {
            width:28px; height:28px; border-radius:6px; flex-shrink:0;
            border:2px solid var(--border-color, #333);
            display:flex; align-items:center; justify-content:center;
            font-size:1rem; color:var(--primary, #0d6efd); font-weight:bold;
            transition:border-color 0.15s;
        }
        .pp-checked .pp-check { border-color:var(--primary, #0d6efd); background:rgba(13,110,253,0.2); }
        .pp-name { font-size:1.1rem; color:var(--text-main, #e0e0e0); }
        .pp-footer {
            display:flex; gap:10px; padding:12px 20px;
            border-top:1px solid var(--border-color, #333);
        }
        .pp-btn-cancel {
            flex:1; padding:14px; border:1px solid var(--border-color, #333);
            background:transparent; color:var(--text-muted, #888);
            border-radius:8px; font-size:1rem; cursor:pointer;
        }
        .pp-btn-apply {
            flex:2; padding:14px; border:none;
            background:var(--primary, #0d6efd); color:white;
            border-radius:8px; font-size:1.1rem; font-weight:bold; cursor:pointer;
            transition:opacity 0.2s;
        }
        .pp-btn-apply:disabled { opacity:0.4; cursor:default; }
    `;
    document.head.appendChild(s);
})();
