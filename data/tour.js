// --- ZENTRALE TUTORIAL ENGINE (v2 - i18n + Autostart + Dynamische Texte + Magnet Arrow + Smart Selectors) ---

// =============================================
// TUTORIAL TEXTE (EN / DE)
// =============================================
const tourTexts = {
    en: {
        welcome_title: "Welcome to Puck Racer!",
        welcome_sub: "Let's get started!",
        btn_next: "Next ➡",
        btn_cancel: "Cancel",
        btn_finish: "Finish ✔",
        btn_start: "Let's go! 🏁",

        step_lang: "First, choose your language!<br><br>Open the menu and select your preferred language. The tutorial will adapt automatically.",
        step_status: "Welcome to Puck Racer! 🏁<br><br>Up here you can always see how many pucks are currently connected to the system. The colors indicate the connection quality of each puck.",
        step_filter: "Use this bar to quickly sort the 20+ games by training goal. You can also swipe left later to see more filters.",
        step_shuttle: "Let's set up your first game!<br><br>Click the <b>Shuttle Run</b> tile to enter the game settings.",

        step_setup_card: "Great! Here you are in the game settings.<br><br>Choose how many runners compete against each other. Note that this game requires 2 pucks per player and you need at least 2 pucks connected.",
        step_setup_rounds: "Shuttle Run is very simple. You only need to set how many rounds should be run. Other games have 5-6 settings here!",
        step_setup_next: "All set? Then click NEXT to get to the puck assignment.",

        step_names_card: "Here puck groups are formed so you can physically set up the game. You can also enter a name for each group.<br><br>Look at your pucks: One should now be breathing blue (the target puck), one has a circle effect (the start puck).",
        step_names_start: "Once you've placed the pucks, click START GAME.",

        step_run_timer: "This is the actual game dashboard!<br><br>Here you usually have a stopwatch or a countdown.",
        step_run_status: "Usually you see a status that tells you the current game state (Ready, False Start, Running, Time Up...)",
        step_run_score: "Here you see the results table.<br><br>In many games you can also give extra time, stop individual groups, or just see the live result.",
        step_run_home: "Here you can go back to the home screen.<br><br>Now test a round of Shuttle Run! Have fun with Puck Racer!"
    },
    de: {
        welcome_title: "Willkommen bei Puck Racer!",
        welcome_sub: "Los geht's!",
        btn_next: "Weiter ➡",
        btn_cancel: "Abbrechen",
        btn_finish: "Beenden ✔",
        btn_start: "Los geht's! 🏁",

        step_lang: "Wähle zuerst deine Sprache!<br><br>Öffne das Menü und wähle deine bevorzugte Sprache. Das Tutorial passt sich automatisch an.",
        step_status: "Willkommen beim Puck Racer! 🏁<br><br>Hier oben siehst du jederzeit, wie viele Pucks gerade aktiv mit dem System verbunden sind. Die Farben zeigen dir die jeweilige Verbindungsqualität.",
        step_filter: "Mit dieser Leiste kannst du die über 20 Spiele blitzschnell nach Trainingsziel sortieren. Nachher kannst du hier auch nach links swipen um noch mehr Filter zu sehen.",
        step_shuttle: "Lass uns dein erstes Spiel einstellen!<br><br>Klicke auf die Kachel <b>Pendellauf (Shuttle Run)</b>, um in die Spieleinstellungen zu gelangen.",

        step_setup_card: "Perfekt! Hier bist du in den Spieleinstellungen.<br><br>Hier wählst du z.B. aus, wie viele Läufer gegeneinander antreten sollen. Achte darauf, dass in diesem Spiel 2 Pucks pro Spieler benötigt werden und dass du mind. 2 Pucks verbunden hast.",
        step_setup_rounds: "Pendellauf ist sehr einfach. Du musst nur einstellen wie viele Runden gelaufen werden sollen. Andere Spiele haben hier 5-6 Einstellmöglichkeiten!",
        step_setup_next: "Alles klar? Dann klicke auf WEITER, um zur Puck-Zuweisung zu kommen.",

        step_names_card: "Hier werden Puckgruppen gebildet, damit du das Spiel physisch aufbauen kannst. Zudem kannst du je Gruppe einen Namen eingeben.<br><br>Schau auf deine Pucks: Einer sollte jetzt blau atmen (der Ziel-Puck), einer hat einen Kreislauf-Effekt (der Start-Puck).",
        step_names_start: "Wenn du die Pucks verteilt hast, klicke auf START GAME / ZUM SPIEL.",

        step_run_timer: "Das ist das eigentliche Spiel-Dashboard!<br><br>Hier hast du meistens eine Stoppuhr oder einen Countdown.",
        step_run_status: "Meistens siehst du einen Status, der dir sagt, in welchem Zustand das Spiel gerade ist (Bereit, Fehlstart, Running, Time Up...)",
        step_run_score: "Hier siehst du die Auswertungstabelle.<br><br>In vielen Spielen kannst du hier auch Extra-Zeit geben, einzelne Gruppen stoppen oder siehst einfach das Live-Ergebnis des Spiels.",
        step_run_home: "Hier gelangst du zurück zur Startseite.<br><br>Jetzt teste aber eine Runde Shuttle Run! Viel Spaß mit Puck Racer!"
    }
};

// =============================================
// TUTORIAL SCHRITTE (Drehbuch)
// =============================================
const tourSteps = [
    { page: 'index', type: 'welcome' },

    { page: 'index', target: '#mainMenu', textKey: 'step_lang', requireInteraction: 'lang', placement: 'right' },

    { page: 'index', target: '#statusBarGroup', textKey: 'step_status', arrow: 'right' },
    { page: 'index', target: '#filterBar', textKey: 'step_filter' },
    { page: 'index', target: '.tile[data-id="shuttle"]', textKey: 'step_shuttle', requireClick: true },

    // Nutze nth-of-type, damit wir keine IDs im HTML brauchen! 
    { page: 'shuttle_setup', target: 'main .card:nth-of-type(1)', textKey: 'step_setup_card', allowInteraction: true },
    { page: 'shuttle_setup', target: 'main .card:nth-of-type(2)', textKey: 'step_setup_rounds', placement: 'top', allowInteraction: true },
    { page: 'shuttle_setup', target: '.btn-primary', textKey: 'step_setup_next', requireClick: true, placement: 'top' },

    // Nutze direkt #playerList für das Input-Feld
    { page: 'shuttle_names', target: '#playerList', textKey: 'step_names_card', allowInteraction: true },
    { page: 'shuttle_names', target: '.btn-primary', textKey: 'step_names_start', requireClick: true, placement: 'top' },

    { page: 'shuttle_run', target: '#timer', textKey: 'step_run_timer' },
    { page: 'shuttle_run', target: '#statusText', textKey: 'step_run_status', placement: 'top' },
    { page: 'shuttle_run', target: '#scoreboard', textKey: 'step_run_score', placement: 'top' },
    { page: 'shuttle_run', target: '.bottom-nav button', textKey: 'step_run_home', placement: 'top' }
];

// =============================================
// STATE
// =============================================
let currentTourStep = parseInt(sessionStorage.getItem('pr_tour_step')) || 0;
let tourLang = sessionStorage.getItem('pr_tour_lang') || 'en';

function getTourText(key) {
    let texts = tourTexts[tourLang] || tourTexts['en'];
    let baseText = texts[key] || tourTexts['en'][key] || key;

    if (key === 'step_status') {
        let green = parseInt(document.getElementById('cnt-green')?.innerText || '0');
        let yellow = parseInt(document.getElementById('cnt-yellow')?.innerText || '0');
        let red = parseInt(document.getElementById('cnt-red')?.innerText || '0');
        let total = green + yellow + red;

        if (tourLang === 'de') {
            if (total < 2) {
                baseText += "<br><br><span style='color:var(--warning)'>Schalte jetzt 2 Pucks ein. Sie sollten hier auftauchen.</span>";
            } else {
                baseText += `<br><br><span style='color:var(--success)'>Du hast schon ${total} Pucks verbunden. Super!</span><br><span style='font-size:0.9em; color:#aaa;'>(Verbindung: ${green} gut, ${yellow} mittel, ${red} schwach)</span>`;
            }
        } else {
            if (total < 2) {
                baseText += "<br><br><span style='color:var(--warning)'>Please turn on 2 Pucks now. They should appear here shortly.</span>";
            } else {
                baseText += `<br><br><span style='color:var(--success)'>You already have ${total} Pucks connected. Great!</span><br><span style='font-size:0.9em; color:#aaa;'>(Connection: ${green} good, ${yellow} mid, ${red} bad)</span>`;
            }
        }
    }
    return baseText;
}

// =============================================
// HTML INJECTION
// =============================================
function injectTourHTML() {
    if (document.getElementById('tourOverlay')) return;

    const html = `
        <div id="tourOverlay" class="tour-overlay"></div>
        <div id="tourBubble" class="tour-bubble">
            <div id="tourText">Tutorial Text</div>
            <div class="tour-bubble-buttons">
                <button class="tour-btn-cancel" onclick="endTutorial()">${getTourText('btn_cancel')}</button>
                <button class="tour-btn" id="tourNextBtn" onclick="tourNextStep()">${getTourText('btn_next')}</button>
            </div>
        </div>

        <div id="tourWelcomeOverlay" style="
            display:none; position:fixed; top:0; left:0; width:100%; height:100%;
            background:rgba(0,0,0,0.95); z-index:4000;
            justify-content:center; align-items:center; flex-direction:column;
            opacity:0; transition:opacity 0.6s ease;
        ">
            <div style="text-align:center; color:#fff;">
                <style>
                    @keyframes tour-button-press {
                        0%, 100% { transform: translateZ(var(--z)); }
                        50% { transform: translateZ(calc(var(--z) - 4px)); }
                    }
                    @keyframes tour-rgb-glow {
                        0%   { background: #ff0000; border-color: #ffaa00; filter: blur(1px) brightness(1.5); }
                        33%  { background: #00ff00; border-color: #aaffaa; filter: blur(1px) brightness(1.5); }
                        66%  { background: #0000ff; border-color: #aaaaff; filter: blur(1px) brightness(1.5); }
                        100% { background: #ff0000; border-color: #ffaa00; filter: blur(1px) brightness(1.5); }
                    }
                    @keyframes tour-rgb-pulse-glow {
                        0%   { box-shadow: 0 0 50px 20px rgba(255,0,0,0.8), inset 0 0 20px rgba(255,0,0,0.9); filter: blur(2px) brightness(2); }
                        33%  { box-shadow: 0 0 50px 20px rgba(0,255,0,0.8), inset 0 0 20px rgba(0,255,0,0.9); filter: blur(2px) brightness(2); }
                        66%  { box-shadow: 0 0 50px 20px rgba(0,0,255,0.8), inset 0 0 20px rgba(0,0,255,0.9); filter: blur(2px) brightness(2); }
                        100% { box-shadow: 0 0 50px 20px rgba(255,0,0,0.8), inset 0 0 20px rgba(255,0,0,0.9); filter: blur(2px) brightness(2); }
                    }
                    #tourPuckAssembly.glowing .puck-led { animation: tour-rgb-glow 3s linear infinite; }
                    #tourPuckAssembly.glowing .puck-led:last-of-type { animation: tour-rgb-pulse-glow 3s linear infinite; }
                    #tourPuckAssembly.glowing .puck-button { animation: tour-button-press 2.5s ease-in-out infinite; }
                </style>
                <div class="puck-scene" style="width:300px; height:300px; perspective:1200px; display:flex; justify-content:center; align-items:center; margin:0 auto;">
                    <div class="puck-assembly" id="tourPuckAssembly" style="position:relative; width:180px; height:180px; transform-style:preserve-3d; transform:rotateX(70deg) rotateZ(0deg); transition:transform 2s cubic-bezier(0.25,1,0.5,1);">
                        <div class="puck-part puck-base" style="--z:0px;"></div>
                        <div class="puck-part puck-base" style="--z:2px;"></div>
                        <div class="puck-part puck-base" style="--z:4px;"></div>
                        <div class="puck-part puck-base" style="--z:6px;"></div>
                        <div class="puck-part puck-base" style="--z:8px;"></div>
                        <div class="puck-part puck-base" style="--z:10px;"></div>
                        <div class="puck-part puck-base" style="--z:12px;"></div>
                        <div class="puck-part puck-base" style="--z:14px;"></div>
                        <div class="puck-part puck-base" style="--z:16px;"></div>
                        <div class="puck-part puck-base" style="--z:18px;"></div>
                        <div class="puck-part puck-base" style="--z:20px;"></div>
                        <div class="puck-part puck-base" style="--z:22px;"></div>
                        <div class="puck-part puck-base" style="--z:24px;"></div>
                        <div class="puck-part puck-base" style="--z:26px;"></div>
                        <div class="puck-part puck-base" style="--z:28px;"></div>
                        <div class="puck-part puck-base" style="--z:30px;"></div>
                        <div class="puck-part puck-base" style="--z:32px;"></div>
                        <div class="puck-part puck-base" style="--z:34px;"></div>
                        <div class="puck-part puck-base" style="--z:36px;"></div>
                        <div class="puck-part puck-base" style="--z:38px;"></div>
                        <div class="puck-part puck-base" style="--z:40px;"></div>
                        <div class="puck-part puck-base" style="--z:42px;"></div>
                        <div class="puck-part puck-base" style="--z:44px;"></div>
                        <div class="puck-part puck-base" style="--z:46px;"></div>
                        <div class="puck-part puck-base" style="--z:48px;"></div>
                        <div class="puck-part puck-base" style="--z:50px;"></div>
                        <div class="puck-part puck-base" style="--z:52px;"></div>
                        <div class="puck-part puck-base" style="--z:54px;"></div>
                        <div class="puck-part puck-base" style="--z:56px;"></div>
                        <div class="puck-part puck-base" style="--z:58px;"></div>
                        <div class="puck-part puck-base" style="--z:60px;"></div>
                        <div class="puck-part puck-base" style="--z:62px;"></div>
                        <div class="puck-part puck-led" style="--z:64px;"></div>
                        <div class="puck-part puck-led" style="--z:66px;"></div>
                        <div class="puck-part puck-led" style="--z:68px;"></div>
                        <div class="puck-part puck-led" style="--z:70px;"></div>
                        <div class="puck-part puck-button" style="--z:72px;"></div>
                        <div class="puck-part puck-button" style="--z:74px;"></div>
                        <div class="puck-part puck-button" style="--z:76px;"></div>
                        <div class="puck-part puck-button" style="--z:78px;"></div>
                        <div class="puck-part puck-button" style="--z:80px;"></div>
                        <div class="puck-part puck-button" style="--z:82px;"></div>
                    </div>
                </div>

                <h1 id="tourWelcomeTitle" style="
                    font-family:'puckracer', sans-serif; letter-spacing:3px;
                    font-size:2rem; margin-top:30px; color:var(--primary, #00bfff);
                    opacity:0; transform:translateY(20px); transition:all 0.8s ease 1s;
                ">Welcome to Puck Racer!</h1>

                <p id="tourWelcomeSub" style="
                    font-size:1.2rem; color:#aaa; margin-top:10px;
                    opacity:0; transform:translateY(20px); transition:all 0.8s ease 1.3s;
                ">Let's get started!</p>

                <div id="tourWelcomeVersion" style="
                    color:var(--primary, #00bfff); font-size:1rem; font-family:monospace;
                    font-weight:bold; margin-top:15px; letter-spacing:2px;
                    opacity:0; transform:translateY(20px); transition:all 0.8s ease 1.6s;
                ">CORE v---</div>

                <button id="tourWelcomeBtn" onclick="closeWelcomeAndContinue()" style="
                    margin-top:40px; padding:15px 40px; font-size:1.3rem; font-weight:bold;
                    background:var(--primary, #00bfff); color:#000; border:none; border-radius:10px;
                    cursor:pointer; opacity:0; transform:translateY(20px); transition:all 0.8s ease 1.9s;
                ">Let's go! 🏁</button>
            </div>
        </div>
    `;
    document.body.insertAdjacentHTML('beforeend', html);
}

// =============================================
// WELCOME SCREEN
// =============================================
function showWelcomeScreen() {
    let overlay = document.getElementById('tourWelcomeOverlay');
    let assembly = document.getElementById('tourPuckAssembly');

    document.getElementById('tourWelcomeTitle').innerText = getTourText('welcome_title');
    document.getElementById('tourWelcomeSub').innerText = getTourText('welcome_sub');
    document.getElementById('tourWelcomeBtn').innerText = getTourText('btn_start');

    let verEl = document.getElementById('tourWelcomeVersion');
    if (typeof currentCoreVersion !== 'undefined') {
        verEl.innerText = 'CORE ' + currentCoreVersion;
    } else {
        fetch('/api/version').then(r => r.text()).then(v => {
            verEl.innerText = 'CORE ' + v;
        }).catch(() => { verEl.innerText = ''; });
    }

    overlay.style.display = 'flex';
    requestAnimationFrame(() => {
        overlay.style.opacity = '1';
        setTimeout(() => { assembly.classList.add('assembled'); }, 100);
        setTimeout(() => { assembly.classList.add('glowing'); }, 1300);

        document.getElementById('tourWelcomeTitle').style.opacity = '1';
        document.getElementById('tourWelcomeTitle').style.transform = 'translateY(0)';
        document.getElementById('tourWelcomeSub').style.opacity = '1';
        document.getElementById('tourWelcomeSub').style.transform = 'translateY(0)';
        verEl.style.opacity = '1';
        verEl.style.transform = 'translateY(0)';
        document.getElementById('tourWelcomeBtn').style.opacity = '1';
        document.getElementById('tourWelcomeBtn').style.transform = 'translateY(0)';
    });
}

function closeWelcomeAndContinue() {
    let overlay = document.getElementById('tourWelcomeOverlay');
    let assembly = document.getElementById('tourPuckAssembly');

    overlay.style.opacity = '0';
    setTimeout(() => {
        overlay.style.display = 'none';
        assembly.classList.remove('assembled', 'glowing');
    }, 600);

    currentTourStep = 1;
    sessionStorage.setItem('pr_tour_step', currentTourStep);
    setTimeout(showTourStep, 700);
}

// =============================================
// LANGUAGE STEP HANDLING
// =============================================
function handleLangStep() {
    let menu = document.getElementById('mainMenu');
    if (menu) menu.classList.add('show');

    let langSelect = document.getElementById('langSelect');
    if (langSelect) {
        langSelect.style.setProperty('pointer-events', 'auto', 'important');

        const langChangeHandler = function () {
            let newLang = langSelect.value;
            if (newLang === 'de') tourLang = 'de';
            else tourLang = 'en';
            sessionStorage.setItem('pr_tour_lang', tourLang);

            updateTourButtonTexts();
            langSelect.removeEventListener('change', langChangeHandler);

            if (menu) menu.classList.remove('show');

            currentTourStep++;
            sessionStorage.setItem('pr_tour_step', currentTourStep);
            setTimeout(showTourStep, 300);
        };
        langSelect.addEventListener('change', langChangeHandler);
    }
}

function updateTourButtonTexts() {
    let cancelBtn = document.querySelector('.tour-btn-cancel');
    if (cancelBtn) cancelBtn.innerText = getTourText('btn_cancel');
}

// =============================================
// TUTORIAL START / STOP
// =============================================
function startTutorial() {
    sessionStorage.setItem('pr_tour_active', 'true');
    sessionStorage.setItem('pr_tour_step', '0');
    currentTourStep = 0;

    if (typeof currentLang !== 'undefined' && currentLang === 'de') tourLang = 'de';
    else tourLang = 'en';
    sessionStorage.setItem('pr_tour_lang', tourLang);

    if (!window.location.pathname.endsWith('/') && !window.location.pathname.endsWith('index.html')) {
        window.location.href = '/';
        return;
    }

    if (typeof currentFilter !== 'undefined' && currentFilter !== 'all') {
        currentFilter = 'all';
        if (typeof updateFilterUI === 'function') updateFilterUI();
        if (typeof renderGrid === 'function') renderGrid();
    }

    showTourStep();
}

function autoStartTutorial() {
    let path = window.location.pathname;
    if (!path.endsWith('/') && !path.endsWith('index.html')) return;
    if (localStorage.getItem('pr_tour_done') === 'true') return;

    localStorage.setItem('pr_tour_done', 'true');
    startTutorial();
}

function endTutorial() {
    sessionStorage.removeItem('pr_tour_active');
    sessionStorage.removeItem('pr_tour_step');
    sessionStorage.removeItem('pr_tour_lang');
    localStorage.setItem('pr_tour_done', 'true');

    let overlay = document.getElementById('tourOverlay');
    let bubble = document.getElementById('tourBubble');
    let welcome = document.getElementById('tourWelcomeOverlay');
    if (overlay) overlay.style.display = 'none';
    if (bubble) bubble.style.display = 'none';
    if (welcome) { welcome.style.opacity = '0'; setTimeout(() => { welcome.style.display = 'none'; }, 500); }

    document.querySelectorAll('.tour-highlight').forEach(el => {
        el.classList.remove('tour-highlight');
        el.style.pointerEvents = '';
        if (el.getAttribute('data-tour-orig-pos') === 'static') {
            el.style.position = '';
            el.removeAttribute('data-tour-orig-pos');
        }
    });

    let menu = document.getElementById('mainMenu');
    if (menu) menu.classList.remove('show');
    resetHeaderZIndex();
}

// =============================================
// HEADER Z-INDEX MANAGEMENT
// =============================================
function manageHeaderZIndex(targetEl) {
    let header = document.querySelector('header');
    let overlay = document.getElementById('tourOverlay');
    if (!header) return;

    if (targetEl && header.contains(targetEl)) {
        header.style.zIndex = '9999';
        header.style.position = 'relative';
        if (overlay) overlay.style.pointerEvents = 'none';
    } else {
        header.style.zIndex = '';
        if (overlay) overlay.style.pointerEvents = '';
    }
}

function resetHeaderZIndex() {
    let header = document.querySelector('header');
    let overlay = document.getElementById('tourOverlay');
    if (header) header.style.zIndex = '';
    if (overlay) overlay.style.pointerEvents = '';
}

// =============================================
// BUBBLE POSITIONING (MAGNET ARROW)
// =============================================
function updateBubblePosition() {
    if (currentTourStep >= tourSteps.length) return;
    let step = tourSteps[currentTourStep];
    if (step.type === 'welcome') return;

    let targetEl = document.querySelector(step.target);
    let bubble = document.getElementById('tourBubble');

    if (!targetEl || !bubble || bubble.style.display === 'none') return;

    let rect = targetEl.getBoundingClientRect();
    let bubbleH = bubble.offsetHeight || 150;
    let bubbleW = bubble.offsetWidth || 280;

    let targetCX = rect.left + (rect.width / 2);
    let targetCY = rect.top + (rect.height / 2);

    let top = rect.bottom + 15;
    let left = rect.left;

    let fitsRight = (rect.right + 25 + bubbleW) <= window.innerWidth;

    if (step.placement === 'right' && fitsRight) {
        left = rect.right + 25;
        top = targetCY - (bubbleH / 2);
    } else if (step.placement === 'top' || (top + bubbleH > window.innerHeight)) {
        top = rect.top - bubbleH - 15;
    } else {
        top = rect.bottom + 15;
    }

    if (!(step.placement === 'right' && fitsRight)) {
        left = targetCX - (bubbleW / 2);
    }

    if (left + bubbleW > window.innerWidth) left = window.innerWidth - bubbleW - 10;
    if (left < 10) left = 10;
    if (top < 10) top = 10;
    if (top + bubbleH > window.innerHeight) top = window.innerHeight - bubbleH - 10;

    bubble.style.top = top + 'px';
    bubble.style.left = left + 'px';

    if (step.placement === 'right' && fitsRight) {
        let arrowTop = targetCY - top - 10; 
        if (arrowTop < 15) arrowTop = 15;
        if (arrowTop > bubbleH - 35) arrowTop = bubbleH - 35;
        bubble.style.setProperty('--arrow-top', arrowTop + 'px');
        bubble.style.setProperty('--arrow-left', '-22px');
        bubble.style.setProperty('--arrow-color', 'transparent var(--primary) transparent transparent');
    } else if (step.placement === 'top' || (rect.bottom + 15 + bubbleH > window.innerHeight)) {
        let arrowLeft = targetCX - left - 10;
        if (arrowLeft < 15) arrowLeft = 15;
        if (arrowLeft > bubbleW - 35) arrowLeft = bubbleW - 35;
        bubble.style.setProperty('--arrow-top', '100%');
        bubble.style.setProperty('--arrow-left', arrowLeft + 'px');
        bubble.style.setProperty('--arrow-color', 'var(--primary) transparent transparent transparent');
    } else {
        let arrowLeft = targetCX - left - 10;
        if (arrowLeft < 15) arrowLeft = 15;
        if (arrowLeft > bubbleW - 35) arrowLeft = bubbleW - 35;
        bubble.style.setProperty('--arrow-top', '-22px');
        bubble.style.setProperty('--arrow-left', arrowLeft + 'px');
        bubble.style.setProperty('--arrow-color', 'transparent transparent var(--primary) transparent');
    }
}

// =============================================
// SHOW STEP
// =============================================
function showTourStep() {
    if (currentTourStep >= tourSteps.length) {
        endTutorial();
        return;
    }

    let step = tourSteps[currentTourStep];

    if (step.type === 'welcome') {
        showWelcomeScreen();
        return;
    }

    let path = window.location.pathname;
    let isIndexPage = path.endsWith('/') || path.endsWith('index.html');
    let isShuttleSetup = path.includes('shuttle_setup');
    let isShuttleNames = path.includes('shuttle_names');
    let isShuttleRun = path.includes('shuttle_run');

    if (step.page === 'index' && !isIndexPage) return;
    if (step.page === 'shuttle_setup' && !isShuttleSetup) return;
    if (step.page === 'shuttle_names' && !isShuttleNames) return;
    if (step.page === 'shuttle_run' && !isShuttleRun) return;

    document.querySelectorAll('.tour-highlight').forEach(el => {
        el.classList.remove('tour-highlight');
        el.style.pointerEvents = '';
        if (el.getAttribute('data-tour-orig-pos') === 'static') {
            el.style.position = '';
            el.removeAttribute('data-tour-orig-pos');
        }
    });

    if (step.requireInteraction === 'lang') {
        let menu = document.getElementById('mainMenu');
        if (menu) menu.classList.add('show');
    }

    let targetEl = document.querySelector(step.target);
    
    // WICHTIG: Warte, bis das Element im DOM existiert UND physisch sichtbar ist 
    // (Löst das Problem bei nachladenden Inhalten wie der Namens-Liste).
    if (!targetEl || (targetEl.offsetHeight === 0 && targetEl.offsetWidth === 0)) { 
        setTimeout(showTourStep, 200); 
        return; 
    }

    manageHeaderZIndex(targetEl);

    let compStyle = window.getComputedStyle(targetEl);
    if (compStyle.position === 'static') {
        targetEl.setAttribute('data-tour-orig-pos', 'static');
        targetEl.style.setProperty('position', 'relative', 'important');
    }

    document.getElementById('tourOverlay').style.display = 'block';
    targetEl.classList.add('tour-highlight');
    targetEl.scrollIntoView({ behavior: 'smooth', block: 'center' });

    let bubble = document.getElementById('tourBubble');
    document.getElementById('tourText').innerHTML = getTourText(step.textKey);
    bubble.style.display = 'block';

    updateBubblePosition();
    setTimeout(updateBubblePosition, 100);
    setTimeout(updateBubblePosition, 300);

    if (step.requireInteraction === 'lang') {
        targetEl.style.setProperty('pointer-events', 'auto', 'important');
        let nextBtn = document.getElementById('tourNextBtn');
        nextBtn.style.display = 'block';
        nextBtn.innerText = getTourText('btn_next');
        handleLangStep();
        return;
    }

    setTimeout(updateBubblePosition, 500);

    if (step.requireClick) {
        targetEl.style.setProperty('pointer-events', 'auto', 'important');
        document.getElementById('tourNextBtn').style.display = 'none';

        const clickHandler = function (e) {
            if (e.target.classList.contains('help-btn') || e.target.classList.contains('fav-btn')) return;
            targetEl.removeEventListener('click', clickHandler);
            sessionStorage.setItem('pr_tour_step', currentTourStep + 1);
        };
        targetEl.addEventListener('click', clickHandler);

    } else {
        // Zwingt den Browser absolut dazu, Interaktionen im markierten Bereich durchzulassen
        targetEl.style.setProperty('pointer-events', step.allowInteraction ? 'auto' : 'none', 'important');
        
        let nextBtn = document.getElementById('tourNextBtn');
        nextBtn.style.display = 'block';
        if (currentTourStep === tourSteps.length - 1) {
            nextBtn.innerText = getTourText('btn_finish');
        } else {
            nextBtn.innerText = getTourText('btn_next');
        }
    }

    updateTourButtonTexts();
}

function tourNextStep() {
    currentTourStep++;
    sessionStorage.setItem('pr_tour_step', currentTourStep);
    showTourStep();
}

window.addEventListener('scroll', updateBubblePosition, { passive: true });
window.addEventListener('resize', updateBubblePosition, { passive: true });

document.addEventListener('DOMContentLoaded', () => {
    injectTourHTML();
    let savedLang = sessionStorage.getItem('pr_tour_lang');
    if (savedLang) tourLang = savedLang;

    if (sessionStorage.getItem('pr_tour_active') === 'true') {
        currentTourStep = parseInt(sessionStorage.getItem('pr_tour_step')) || 0;
        setTimeout(showTourStep, 400);
    } else {
        setTimeout(autoStartTutorial, 800);
    }
});