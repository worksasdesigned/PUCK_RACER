<?php
/**
 * PuckRacer Registrierungs-Skript
 * * Verarbeitet die Formulareingaben, speichert die Daten in der Datenbank
 * und bettet das Formular in das bestehende PUCK RACER Design ein.
 */

// --- 1. DATENBANK-KONFIGURATION ---
$db_host = 'localhost';
$db_name = 'h1170139';
$db_user = 'h1170139';
// WICHTIG: Setze hier dein echtes Passwort ein. 
$db_pass = 'Asni1sP!xitdb';

// Verbindung herstellen
$conn = new mysqli($db_host, $db_user, $db_pass, $db_name);

// Verbindungsfehler abfangen
if ($conn->connect_error) {
    die("Datenbankverbindung fehlgeschlagen: " . $conn->connect_error);
}

// --- 2. DATENBANK-TABELLE PRÜFEN / ANLEGEN ---
// Erstellt die Tabelle 'puckracer_geomaps', falls sie noch nicht existiert.
$sql_create_table = "CREATE TABLE IF NOT EXISTS puckracer_geomaps (
    id INT AUTO_INCREMENT PRIMARY KEY,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    latitude DECIMAL(10, 8) NOT NULL,
    longitude DECIMAL(11, 8) NOT NULL,
    country VARCHAR(100) NOT NULL,
    city VARCHAR(100) NOT NULL,
    pucks_count INT NOT NULL,
    email VARCHAR(255) DEFAULT NULL,
    pseudonym VARCHAR(100) DEFAULT NULL,
    newsletter TINYINT(1) DEFAULT 0,
    validated VARCHAR(1) DEFAULT ''
)";
$conn->query($sql_create_table);

// --- 3. FORMULAR-VERARBEITUNG ---
$message = ''; // Speichert Erfolgs- oder Fehlermeldungen für die HTML-Ausgabe

if ($_SERVER["REQUEST_METHOD"] == "POST") {
    // Prüfen, ob die Geodaten per JavaScript erfolgreich befüllt wurden
    if (empty($_POST['latitude']) || empty($_POST['longitude'])) {
        $message = "<div class='msg-error'>Bitte wähle eine Adresse aus den Vorschlägen aus, damit die genauen Koordinaten ermittelt werden können.</div>";
    } else {
        // Prepared Statement bereitet die SQL-Abfrage sicher vor (Schutz vor SQL-Injection)
        $stmt = $conn->prepare("INSERT INTO puckracer_geomaps (latitude, longitude, country, city, pucks_count, email, pseudonym, newsletter, validated) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
        
        // Formulardaten in Variablen speichern
        $lat = $_POST['latitude'];
        $lng = $_POST['longitude'];
        $country = $_POST['country'];
        $city = $_POST['city'];
        $pucks = (int)$_POST['pucks_count'];
        $email = $_POST['email'] ?? '';
        $pseudonym = $_POST['pseudonym'] ?? '';
        // Checkbox liefert '1' wenn gesetzt, sonst '0'
        $newsletter = isset($_POST['newsletter']) ? 1 : 0; 
        $validated = ''; // Standardmäßig leer für spätere Freigabe
        
        // Variablen an das SQL-Statement binden (Datentypen: d=double, s=string, i=integer)
        $stmt->bind_param("ddssissis", $lat, $lng, $country, $city, $pucks, $email, $pseudonym, $newsletter, $validated);
        
        // Query ausführen und Erfolg prüfen
        if ($stmt->execute()) {
            $message = "<div class='msg-success'>Danke! Deine Daten wurden erfolgreich gespeichert und warten auf die Freigabe.</div>";
        } else {
            $message = "<div class='msg-error'>Fehler beim Speichern: " . $conn->error . "</div>";
        }
        $stmt->close();
    }
}
?>
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PUCK RACER | Pucks registrieren</title>
    <link rel="stylesheet" href="style.css">
    <style>
        /* --- SPEZIFISCHES FORMULAR-STYLING --- */
        /* Diese Styles ergänzen die style.css, um die Formularelemente passend zum Dark-Theme zu gestalten */
        
        .form-wrapper {
            width: 100%;
            max-width: 600px;
            margin: 20px auto;
            text-align: left;
        }

        .form-group {
            margin-bottom: 20px;
            position: relative;
        }

        .form-group label {
            display: block;
            margin-bottom: 8px;
            color: #8aece5;
            font-weight: bold;
            font-size: 0.95rem;
            text-transform: uppercase;
            letter-spacing: 1px;
        }

        .form-group input[type="text"],
        .form-group input[type="number"],
        .form-group input[type="email"] {
            width: 100%;
            padding: 12px;
            background: rgba(43, 43, 54, 0.7);
            border: 1px solid rgba(255, 255, 255, 0.2);
            border-radius: 10px;
            color: #fff;
            font-size: 1rem;
            outline: none;
            transition: border-color 0.3s ease;
        }

        .form-group input:focus {
            border-color: #8aece5;
        }

        .checkbox-group {
            display: flex;
            align-items: flex-start;
            gap: 10px;
            margin-bottom: 15px;
        }

        .checkbox-group input[type="checkbox"] {
            margin-top: 4px;
            cursor: pointer;
        }

        .checkbox-group label {
            color: #ddd;
            font-size: 0.9rem;
            text-transform: none;
            font-weight: normal;
            letter-spacing: normal;
        }

        /* Styling für die Autocomplete-Vorschlagsliste von OpenStreetMap */
        .suggestions {
            position: absolute;
            top: 100%;
            left: 0;
            right: 0;
            background: #2b2b36;
            border: 1px solid #8aece5;
            border-top: none;
            z-index: 1000;
            max-height: 200px;
            overflow-y: auto;
            border-radius: 0 0 10px 10px;
            box-shadow: 0 5px 15px rgba(0,0,0,0.5);
        }

        .suggestion-item {
            padding: 12px;
            cursor: pointer;
            border-bottom: 1px solid rgba(255,255,255,0.1);
            color: #ddd;
            font-size: 0.9rem;
        }

        .suggestion-item:hover {
            background: rgba(138, 236, 229, 0.2);
            color: #fff;
        }

        /* Styling für den Absende-Button (adaptiert den rainbow-border-wrapper Effekt) */
        .submit-btn {
            background: linear-gradient(45deg, #ff0000, #ff7f00, #ffff00, #00ff00, #0000ff, #4b0082, #9400d3);
            background-size: 400% 400%;
            animation: rainbow-glow 8s ease infinite;
            color: white;
            border: none;
            padding: 15px 30px;
            border-radius: 50px;
            font-size: 1.1rem;
            font-weight: bold;
            text-transform: uppercase;
            letter-spacing: 1px;
            cursor: pointer;
            width: 100%;
            margin-top: 10px;
            box-shadow: 0 0 15px rgba(0,0,0,0.5);
            transition: transform 0.2s;
        }

        .submit-btn:hover {
            transform: scale(1.02);
        }

        /* Meldungen nach dem Absenden */
        .msg-success {
            background: rgba(0, 255, 0, 0.1);
            border: 1px solid #00ff00;
            color: #00ff00;
            padding: 15px;
            border-radius: 10px;
            margin-bottom: 20px;
            text-align: center;
        }

        .msg-error {
            background: rgba(255, 0, 0, 0.1);
            border: 1px solid #ff0000;
            color: #ffcccc;
            padding: 15px;
            border-radius: 10px;
            margin-bottom: 20px;
            text-align: center;
        }
    </style>
</head>
<body>
    <header>
        <nav class="main-nav">
            <button class="nav-toggle" aria-label="Menü öffnen" onclick="document.querySelector('.main-nav').classList.toggle('open')">&#9776; Menü</button>
            <ul>
                <li><a href="index.html">Home</a></li>
                <li><a href="features.html">Features</a></li>
                <li><a href="downloads.html">Downloads</a></li>
                <li><a href="hardware.html">Hardware</a></li>
                <li><a href="spiele.html">Spiele</a></li>
                <li><a href="map.html">PUCK RACER Map</a></li>
                <li><a href="demo.html">Demo</a></li>
                <li><a href="infos.html">Infos</a></li>
                <li><a href="faq.html">FAQ</a></li>
                <li><a href="en/map.html" title="English">&#127468;&#127463; EN</a></li>
            </ul>
        </nav>
    </header>

    <main>
        <div class="rainbow-border-wrapper">
            <div class="main-container">
                <div class="header-bar">
                    <img src="pictures/logo_klein.png" alt="PUCK RACER Logo" class="logo">
                </div>
                
                <h1>Pucks registrieren</h1>
                <p style="text-align: center;">Trage deine Pucks in die Map ein und werde Teil der Community.</p>

                <div class="form-wrapper">
                    <?php echo $message; ?>

                    <form method="POST" action="">
                        <div class="form-group">
                            <label for="address_input">Adresse (Ort und Land reichen)</label>
                            <input type="text" id="address_input" placeholder="Tippe deinen Standort..." autocomplete="off" required>
                            <div id="autocomplete_results" class="suggestions"></div>
                        </div>

                        <input type="hidden" id="latitude" name="latitude">
                        <input type="hidden" id="longitude" name="longitude">
                        <input type="hidden" id="country" name="country">
                        <input type="hidden" id="city" name="city">

                        <div class="form-group">
                            <label for="pucks_count">Anzahl Pucks</label>
                            <input type="number" id="pucks_count" name="pucks_count" min="1" max="100" required>
                        </div>

                        <div class="form-group">
                            <label for="email">E-Mail-Adresse (Freiwillig)</label>
                            <input type="email" id="email" name="email" placeholder="max@beispiel.de">
                        </div>

                        <div class="form-group">
                            <label for="pseudonym">Pseudonym (Freiwillig)</label>
                            <input type="text" id="pseudonym" name="pseudonym" placeholder="Dein Spieler- oder Vereinsname">
                        </div>

                        <div class="checkbox-group">
                            <input type="checkbox" id="data_consent" required>
                            <label for="data_consent">Ich bin damit einverstanden, dass diese Daten gespeichert werden. Der Eintrag wird erst nach einer Prüfung freigeschaltet.</label>
                        </div>

                        <div class="checkbox-group">
                            <input type="checkbox" id="newsletter" name="newsletter" value="1">
                            <label for="newsletter">Ich möchte einen Hinweis per E-Mail erhalten, wenn es eine neue Firmware Version gibt.</label>
                        </div>

                        <button type="submit" class="submit-btn">Daten senden</button>
                    </form>
                </div>

                <footer>
                    <a href="Impressum.html">Impressum</a> · <a href="datenschutz.html">Datenschutz</a>
                </footer>
            </div>
        </div>
    </main>

    <script>
        /**
         * OpenStreetMap (Nominatim) Autocomplete Logik
         * Löst die Benutzereingabe asynchron in echte Koordinaten auf.
         */
        const addressInput = document.getElementById('address_input');
        const resultsDiv = document.getElementById('autocomplete_results');
        
        // Referenzen zu den versteckten HTML-Feldern
        const latInput = document.getElementById('latitude');
        const lngInput = document.getElementById('longitude');
        const countryInput = document.getElementById('country');
        const cityInput = document.getElementById('city');

        let debounceTimer;

        // Wird bei jedem Tastendruck im Adressfeld ausgelöst
        addressInput.addEventListener('input', function() {
            // Timer zurücksetzen, um zu viele API-Anfragen zu vermeiden
            clearTimeout(debounceTimer);
            let query = this.value;
            
            // Erst ab 3 eingegebenen Zeichen suchen
            if (query.length < 3) {
                resultsDiv.innerHTML = '';
                return;
            }
            
            // Warte 500ms nach der letzten Eingabe, bevor die API aufgerufen wird
            debounceTimer = setTimeout(() => {
                fetch(`https://nominatim.openstreetmap.org/search?format=json&q=${encodeURIComponent(query)}&addressdetails=1&limit=5`)
                    .then(response => response.json())
                    .then(data => {
                        resultsDiv.innerHTML = '';
                        
                        // Jeden gefundenen Treffer als Div anlegen
                        data.forEach(item => {
                            let div = document.createElement('div');
                            div.className = 'suggestion-item';
                            div.textContent = item.display_name;
                            
                            // Klick-Event für einen Vorschlag
                            div.onclick = function() {
                                addressInput.value = item.display_name; // Sichtbares Feld befüllen
                                latInput.value = item.lat;              // Geodaten in versteckte Felder speichern
                                lngInput.value = item.lon;
                                
                                // Versuch, Stadt und Land aus den API-Details zu lesen
                                let address = item.address;
                                cityInput.value = address.city || address.town || address.village || address.county || '';
                                countryInput.value = address.country || '';
                                
                                // Dropdown schließen
                                resultsDiv.innerHTML = '';
                            };
                            resultsDiv.appendChild(div);
                        });
                    })
                    .catch(err => console.error('Fehler bei der Adressauflösung:', err));
            }, 500);
        });

        // Schließt das Dropdown, wenn der User irgendwo anders auf die Seite klickt
        document.addEventListener('click', function(e) {
            if (e.target !== addressInput) {
                resultsDiv.innerHTML = '';
            }
        });
    </script>
</body>
</html>