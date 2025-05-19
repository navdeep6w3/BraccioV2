#include <Arduino.h>
#include <AccelStepper.h>
#include <Stepper.h>
#include <WiFi.h>
#include <WebServer.h>

// ===== PIN STEPPER PRINCIPALI =====
#define X_STEP_PIN     21
#define X_DIR_PIN      19
#define X_ENABLE_PIN   18

#define Y_STEP_PIN     5
#define Y_DIR_PIN      17
#define Y_ENABLE_PIN   16

#define Z_STEP_PIN     4
#define Z_DIR_PIN      2
#define Z_ENABLE_PIN   15

// ===== PIN STEPPER PINZA 28BYJ-48 =====
#define PINZA_IN1 27
#define PINZA_IN2 14
#define PINZA_IN3 12
#define PINZA_IN4 13

// ===== WIFI SETTINGS =====
const char* ssid = "BraccioRobotico";
const char* password = "12345678";

// ===== SETTINGS MOTORI =====
#define MAX_SPEED 20000
#define DEFAULT_SPEED 5000

// ===== SETTINGS PINZA =====
#define STEPS_PER_REV 2048
#define PINZA_MAX_SPEED 12

// ===== VARIABILI GLOBALI =====
bool motorsEnabled = false;
bool pinzaEnabled = false;
bool pinzaMoving = false;

// ===== OGGETTI =====
AccelStepper stepperX(AccelStepper::DRIVER, X_STEP_PIN, X_DIR_PIN);
AccelStepper stepperY(AccelStepper::DRIVER, Y_STEP_PIN, Y_DIR_PIN);
AccelStepper stepperZ(AccelStepper::DRIVER, Z_STEP_PIN, Z_DIR_PIN);
Stepper pinzaStepper(STEPS_PER_REV, PINZA_IN1, PINZA_IN3, PINZA_IN2, PINZA_IN4);
WebServer server(80);

// ===== VARIABILI WEB CONTROL =====
bool webXPos = false, webXNeg = false;
bool webYPos = false, webYNeg = false;
bool webZPos = false, webZNeg = false;
bool webPinzaOpen = false, webPinzaClose = false;

// ===== HTML PAGE =====
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Braccio robotico V2</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f0f0f0;
            touch-action: manipulation;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            text-align: center;
            color: #333;
        }
        .control-section {
            margin: 20px 0;
            padding: 15px;
            border: 2px solid #ddd;
            border-radius: 8px;
        }
        .control-section h3 {
            margin-top: 0;
            color: #555;
        }
        .button-group {
            display: flex;
            justify-content: center;
            gap: 10px;
            margin: 10px 0;
        }
        .axis-control {
            display: flex;
            flex-direction: column;
            align-items: center;
            margin: 10px;
        }
        .control-btn {
            padding: 15px 25px;
            font-size: 16px;
            font-weight: bold;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            transition: all 0.3s;
            user-select: none;
            min-width: 80px;
        }
        .control-btn:active {
            transform: scale(0.95);
        }
        .axis-btn {
            background-color: #4CAF50;
            color: white;
        }
        .axis-btn:hover {
            background-color: #45a049;
        }
        .axis-btn:active {
            background-color: #3d8b40;
        }
        .pinza-btn {
            background-color: #2196F3;
            color: white;
        }
        .pinza-btn:hover {
            background-color: #1976D2;
        }
        .pinza-btn:active {
            background-color: #1565C0;
        }
        .enable-btn {
            background-color: #FF9800;
            color: white;
            padding: 10px 20px;
            margin: 5px;
        }
        .enable-btn:hover {
            background-color: #F57C00;
        }
        .enable-btn.enabled {
            background-color: #4CAF50;
        }
        .axis-group {
            display: flex;
            justify-content: space-around;
            flex-wrap: wrap;
        }
        .status {
            background-color: #f9f9f9;
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
        }
        @media (max-width: 600px) {
            .axis-group {
                flex-direction: column;
            }
            .control-btn {
                padding: 12px 20px;
                font-size: 14px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🤖 ESP32 CNC Control</h1>
        
        <!-- Sezione controlli principali del sistema -->
        <div class="control-section">
            <h3>⚡ Sistema</h3>
            <div class="button-group">
                <button class="control-btn enable-btn" id="motoriBtn" onclick="cambiaStatoMotori()">
                    Motori: <span id="statoMotori">Disabilitati</span>
                </button>
                <button class="control-btn enable-btn" id="pinzaBtn" onclick="cambiaStatoPinza()">
                    Pinza: <span id="statoPinza">Disabilitata</span>
                </button>
            </div>
        </div>

        <!-- Controlli per gli assi X, Y, Z -->
        <div class="control-section">
            <h3>🎮 Controllo Assi</h3>
            <div class="axis-group">
                <div class="axis-control">
                    <label><strong>Asse X</strong></label>
                    <div class="button-group">
                        <button class="control-btn axis-btn" 
                                onmousedown="iniziaMovimento('x', false)" 
                                onmouseup="fermaMovimento('x')"
                                ontouchstart="iniziaMovimento('x', false)" 
                                ontouchend="fermaMovimento('x')">← X-</button>
                        <button class="control-btn axis-btn" 
                                onmousedown="iniziaMovimento('x', true)" 
                                onmouseup="fermaMovimento('x')"
                                ontouchstart="iniziaMovimento('x', true)" 
                                ontouchend="fermaMovimento('x')">X+ →</button>
                    </div>
                </div>
                <div class="axis-control">
                    <label><strong>Asse Y</strong></label>
                    <div class="button-group">
                        <button class="control-btn axis-btn" 
                                onmousedown="iniziaMovimento('y', false)" 
                                onmouseup="fermaMovimento('y')"
                                ontouchstart="iniziaMovimento('y', false)" 
                                ontouchend="fermaMovimento('y')">← Y-</button>
                        <button class="control-btn axis-btn" 
                                onmousedown="iniziaMovimento('y', true)" 
                                onmouseup="fermaMovimento('y')"
                                ontouchstart="iniziaMovimento('y', true)" 
                                ontouchend="fermaMovimento('y')">Y+ →</button>
                    </div>
                </div>
                <div class="axis-control">
                    <label><strong>Asse Z</strong></label>
                    <div class="button-group">
                        <button class="control-btn axis-btn" 
                                onmousedown="iniziaMovimento('z', false)" 
                                onmouseup="fermaMovimento('z')"
                                ontouchstart="iniziaMovimento('z', false)" 
                                ontouchend="fermaMovimento('z')">↓ Z-</button>
                        <button class="control-btn axis-btn" 
                                onmousedown="iniziaMovimento('z', true)" 
                                onmouseup="fermaMovimento('z')"
                                ontouchstart="iniziaMovimento('z', true)" 
                                ontouchend="fermaMovimento('z')">Z+ ↑</button>
                    </div>
                </div>
            </div>
        </div>

        <!-- Controlli per la pinza -->
        <div class="control-section">
            <h3>🦾 Pinza</h3>
            <div class="button-group">
                <button class="control-btn pinza-btn" 
                        onmousedown="iniziaMovimentoPinza('close')" 
                        onmouseup="fermaMovimentoPinza()"
                        ontouchstart="iniziaMovimentoPinza('close')" 
                        ontouchend="fermaMovimentoPinza()">🤏 Chiudi</button>
                <button class="control-btn pinza-btn" 
                        onmousedown="iniziaMovimentoPinza('open')" 
                        onmouseup="fermaMovimentoPinza()"
                        ontouchstart="iniziaMovimentoPinza('open')" 
                        ontouchend="fermaMovimentoPinza()">✋ Apri</button>
            </div>
        </div>

        <!-- Area di visualizzazione dello stato del sistema -->
        <div class="status">
            <h3>📊 Stato</h3>
            <div id="infoStato">Sistema in attesa...</div>
        </div>
    </div>

    <script>
        // Variabili globali per gestire i timer dei movimenti continui
        let timerMovimentoAssi = null;  // Timer per movimenti degli assi X, Y, Z
        let timerMovimentoPinza = null; // Timer per movimenti della pinza

        /**
         * Attiva/disattiva i motori degli assi
         */
        function cambiaStatoMotori() {
            fetch('/toggleMotors')
                .then(response => response.text())
                .then(data => {
                    // Ottieni riferimenti agli elementi HTML
                    const bottone = document.getElementById('motoriBtn');
                    const stato = document.getElementById('statoMotori');
                    
                    // Aggiorna l'interfaccia in base alla risposta
                    if (data === 'enabled') {
                        stato.textContent = 'Abilitati';
                        bottone.classList.add('enabled');
                    } else {
                        stato.textContent = 'Disabilitati';
                        bottone.classList.remove('enabled');
                    }
                });
        }

        /**
         * Attiva/disattiva la pinza
         */
        function cambiaStatoPinza() {
            fetch('/togglePinza')
                .then(response => response.text())
                .then(data => {
                    // Ottieni riferimenti agli elementi HTML
                    const bottone = document.getElementById('pinzaBtn');
                    const stato = document.getElementById('statoPinza');
                    
                    // Aggiorna l'interfaccia in base alla risposta
                    if (data === 'enabled') {
                        stato.textContent = 'Abilitata';
                        bottone.classList.add('enabled');
                    } else {
                        stato.textContent = 'Disabilitata';
                        bottone.classList.remove('enabled');
                    }
                });
        }

        /**
         * Inizia il movimento di un asse
         * @param {string} asse - L'asse da muovere ('x', 'y', 'z')
         * @param {boolean} versoPositivo - True per movimento positivo, false per negativo
         */
        function iniziaMovimento(asse, versoPositivo) {
            // Determina la direzione del movimento
            const direzione = versoPositivo ? 'pos' : 'neg';
            
            // Invia il comando di inizio movimento
            fetch('/move?axis=' + asse + '&dir=' + direzione + '&action=start');
            
            // Avvia un timer per inviare comandi continui ogni 50ms
            // Questo permette un movimento fluido finché il bottone è premuto
            timerMovimentoAssi = setInterval(() => {
                fetch('/move?axis=' + asse + '&dir=' + direzione + '&action=continue');
            }, 50);
        }

        /**
         * Ferma il movimento di un asse
         * @param {string} asse - L'asse da fermare ('x', 'y', 'z')
         */
        function fermaMovimento(asse) {
            // Cancella il timer del movimento continuo
            if (timerMovimentoAssi) {
                clearInterval(timerMovimentoAssi);
                timerMovimentoAssi = null;
            }
            // Invia il comando di stop all'ESP32
            fetch('/move?axis=' + asse + '&action=stop');
        }

        /**
         * Inizia il movimento della pinza
         * @param {string} direzione - Direzione del movimento ('close' o 'open')
         */
        function iniziaMovimentoPinza(direzione) {
            // Invia il comando di inizio movimento pinza
            fetch('/pinza?dir=' + direzione + '&action=start');
            
            // Avvia un timer per inviare comandi continui ogni 50ms
            timerMovimentoPinza = setInterval(() => {
                fetch('/pinza?dir=' + direzione + '&action=continue');
            }, 50);
        }

        /**
         * Ferma il movimento della pinza
         */
        function fermaMovimentoPinza() {
            // Cancella il timer del movimento continuo
            if (timerMovimentoPinza) {
                clearInterval(timerMovimentoPinza);
                timerMovimentoPinza = null;
            }
            // Invia il comando di stop all'ESP32
            fetch('/pinza?action=stop');
        }

        // Gestione eventi touch per dispositivi mobili
        // Previeni comportamenti touch predefiniti per i bottoni di controllo
        document.addEventListener('touchstart', function(e) {
            if (e.target.classList.contains('control-btn')) {
                e.preventDefault();
            }
        }, {passive: false});

        document.addEventListener('touchend', function(e) {
            if (e.target.classList.contains('control-btn')) {
                e.preventDefault();
            }
        }, {passive: false});

        // Timer per aggiornamento periodico dello stato
        // Richiede lo stato del sistema all'ESP32 ogni 2 secondi
        setInterval(() => {
            fetch('/status')
                .then(response => response.text())
                .then(data => {
                    // Aggiorna l'area di stato con le informazioni ricevute
                    document.getElementById('infoStato').innerHTML = data;
                });
        }, 2000);
    </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  
  // Configurazione pin enable per motori principali
  pinMode(X_ENABLE_PIN, OUTPUT);
  pinMode(Y_ENABLE_PIN, OUTPUT);
  pinMode(Z_ENABLE_PIN, OUTPUT);
  digitalWrite(X_ENABLE_PIN, HIGH);
  digitalWrite(Y_ENABLE_PIN, HIGH);
  digitalWrite(Z_ENABLE_PIN, HIGH);

  // Configurazione stepper principali
  stepperX.setMaxSpeed(MAX_SPEED);
  stepperX.setSpeed(0);
  stepperY.setMaxSpeed(MAX_SPEED);
  stepperY.setSpeed(0);
  stepperZ.setMaxSpeed(MAX_SPEED);
  stepperZ.setSpeed(0);

  // Configurazione stepper pinza
  pinzaStepper.setSpeed(PINZA_MAX_SPEED);

  // Setup WiFi Access Point
  WiFi.softAP(ssid, password);
  Serial.println();
  Serial.print("WiFi AP avviato: ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/toggleMotors", handleToggleMotors);
  server.on("/togglePinza", handleTogglePinza);
  server.on("/move", handleMove);
  server.on("/pinza", handlePinzaControl);
  server.on("/status", handleStatus);
  
  server.begin();
  Serial.println("Web server avviato");
  Serial.println("Interfaccia web disponibile su: http://" + WiFi.softAPIP().toString());
}

// ===== WEB SERVER HANDLERS =====
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleToggleMotors() {
  motorsEnabled = !motorsEnabled;
  digitalWrite(X_ENABLE_PIN, motorsEnabled ? LOW : HIGH);
  digitalWrite(Y_ENABLE_PIN, motorsEnabled ? LOW : HIGH);
  digitalWrite(Z_ENABLE_PIN, motorsEnabled ? LOW : HIGH);
  
  server.send(200, "text/plain", motorsEnabled ? "enabled" : "disabled");
  Serial.println(motorsEnabled ? "Motori abilitati" : "Motori disabilitati");
}

void handleTogglePinza() {
  pinzaEnabled = !pinzaEnabled;
  if (!pinzaEnabled) {
    disablePinzaStepper();
  }
  
  server.send(200, "text/plain", pinzaEnabled ? "enabled" : "disabled");
  Serial.println(pinzaEnabled ? "Pinza abilitata" : "Pinza disabilitata");
}

void handleMove() {
  String axis = server.arg("axis");
  String direction = server.arg("dir");
  String action = server.arg("action");
  
  if (action == "start" || action == "continue") {
    if (axis == "x") {
      if (direction == "pos") {
        webXPos = true;
        webXNeg = false;
      } else {
        webXNeg = true;
        webXPos = false;
      }
    } else if (axis == "y") {
      if (direction == "pos") {
        webYPos = true;
        webYNeg = false;
      } else {
        webYNeg = true;
        webYPos = false;
      }
    } else if (axis == "z") {
      if (direction == "pos") {
        webZPos = true;
        webZNeg = false;
      } else {
        webZNeg = true;
        webZPos = false;
      }
    }
  } else if (action == "stop") {
    if (axis == "x") {
      webXPos = false;
      webXNeg = false;
    } else if (axis == "y") {
      webYPos = false;
      webYNeg = false;
    } else if (axis == "z") {
      webZPos = false;
      webZNeg = false;
    }
  }
  
  server.send(200, "text/plain", "OK");
}

void handlePinzaControl() {
  String direction = server.arg("dir");
  String action = server.arg("action");
  
  if (action == "start" || action == "continue") {
    if (direction == "open") {
      webPinzaOpen = true;
      webPinzaClose = false;
    } else if (direction == "close") {
      webPinzaClose = true;
      webPinzaOpen = false;
    }
  } else if (action == "stop") {
    webPinzaOpen = false;
    webPinzaClose = false;
  }
  
  server.send(200, "text/plain", "OK");
}

void handleStatus() {
  String status = "Motori: " + String(motorsEnabled ? "✅ Abilitati" : "❌ Disabilitati");
  status += "<br>Pinza: " + String(pinzaEnabled ? "✅ Abilitata" : "❌ Disabilitata");
  status += "<br>Velocità fissa: " + String(DEFAULT_SPEED);
  status += "<br>Controllo: 🌐 Solo Web Interface";
  
  server.send(200, "text/html", status);
}

// ===== FUNZIONI DI CONTROLLO =====
void controlPinza() {
  if (!pinzaEnabled) return;

  if (webPinzaClose) {
    pinzaStepper.step(-5);
    pinzaMoving = true;
  } else if (webPinzaOpen) {
    pinzaStepper.step(5);
    pinzaMoving = true;
  } else {
    if (pinzaMoving) {
      pinzaMoving = false;
    }
  }
}

void disablePinzaStepper() {
  digitalWrite(PINZA_IN1, LOW);
  digitalWrite(PINZA_IN2, LOW);
  digitalWrite(PINZA_IN3, LOW);
  digitalWrite(PINZA_IN4, LOW);
}

void loop() {
  // Gestione web server
  server.handleClient();
  
  // Controllo web per motori principali (velocità fissa)
  int xSpeed = 0, ySpeed = 0, zSpeed = 0;
  
  if (webXPos) xSpeed = DEFAULT_SPEED;
  else if (webXNeg) xSpeed = -DEFAULT_SPEED;
  
  if (webYPos) ySpeed = DEFAULT_SPEED;
  else if (webYNeg) ySpeed = -DEFAULT_SPEED;
  
  if (webZPos) zSpeed = DEFAULT_SPEED;
  else if (webZNeg) zSpeed = -DEFAULT_SPEED;
  
  stepperX.setSpeed(xSpeed);
  stepperY.setSpeed(ySpeed);
  stepperZ.setSpeed(zSpeed);

  // ===== CONTROLLO PINZA =====
  controlPinza();

  // Esegui movimenti motori principali
  stepperX.runSpeed();
  stepperY.runSpeed();
  stepperZ.runSpeed();
}
