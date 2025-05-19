#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

// Configurazione WiFi
const char* ssid = "ESP32_Glove";        // Nome della rete Wi-Fi creata dal braccio robotico
const char* password = "ESP32Robot";      // Password della rete Wi-Fi
const char* robotIP = "192.168.4.1";     // IP predefinito dell'access point ESP32
const int robotPort = 80;                // Porta del server

// Configurazione ADXL345
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// Pin LED per feedback visivo
#define LED_PIN 2

// Configurazione invio dati
#define SEND_INTERVAL 20      // Invio 50 volte al secondo (20ms) - più veloce di prima (50ms)
#define RECONNECT_INTERVAL 500 // Tempo di attesa tra tentativi di riconnessione
#define READ_TIMEOUT 50       // Timeout per la lettura della risposta

WiFiClient client;
int messageCount = 0;
unsigned long lastSentTime = 0;
unsigned long lastLedBlinkTime = 0;
bool isConnected = false;

void setup() {
  Serial.begin(115200);
  
  // Configura il pin LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Inizializzazione del sensore ADXL345 con timeouts ridotti
  Wire.setClock(400000); // Imposta I2C a 400kHz per comunicazione più veloce con il sensore
  
  Serial.println("Inizializzazione del sensore ADXL345...");
  if(!accel.begin()) {
    Serial.println("ERRORE: Impossibile rilevare ADXL345!");
    // Lampeggia velocemente il LED per indicare errore sensore
    while(1) {
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
  }
  
  // Imposta il range di misurazione
  accel.setRange(ADXL345_RANGE_16_G);
  
  // Connessione al WiFi con ottimizzazioni
  Serial.println("\nConnessione a " + String(ssid));
  
  // Impostazioni per connessione WiFi più veloce
  WiFi.setSleep(false); // Disabilita la modalità risparmio energetico WiFi
  WiFi.mode(WIFI_STA);  // Imposta modalità stazione
  WiFi.begin(ssid, password);
  
  // Attendi connessione con timeout
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000) { // Ridotto da 10s a 5s
    delay(100); // Ridotto da 500ms a 100ms
    Serial.print(".");
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nImpossibile connettersi al braccio robotico.");
    while(1) {
      for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(200);
        digitalWrite(LED_PIN, LOW);
        delay(200);
      }
      delay(1000);
    }
  }
  
  Serial.println("\nWiFi connesso");
  Serial.print("Indirizzo IP: ");
  Serial.println(WiFi.localIP());
  
  // LED acceso fisso per indicare connessione riuscita
  digitalWrite(LED_PIN, HIGH);
  
  // Imposta il keepalive per evitare disconnessioni TCP
  client.setNoDelay(true);
  
  // Connetti subito al server del braccio robotico
  connectToRobot();
}

bool connectToRobot() {
  if (client.connected()) {
    return true;
  }
  
  Serial.println("Connessione al braccio robotico...");
  digitalWrite(LED_PIN, LOW);
  
  if (client.connect(robotIP, robotPort)) {
    Serial.println("Connesso al braccio robotico");
    digitalWrite(LED_PIN, HIGH);
    isConnected = true;
    return true;
  } else {
    Serial.println("Connessione fallita");
    isConnected = false;
    return false;
  }
}

void loop() {
  // Controlla se siamo ancora connessi al WiFi (controllo veloce)
  if (WiFi.status() != WL_CONNECTED) {
    handleWifiReconnect();
  }
  
  // Verifica se è ora di inviare dati
  if (millis() - lastSentTime >= SEND_INTERVAL) {
    lastSentTime = millis();
    
    // Verifica connessione al server del braccio
    if (!client.connected()) {
      if (!connectToRobot()) {
        return; // Salta il ciclo se non riusciamo a connetterci
      }
    }
    
    // Leggi i dati dall'accelerometro
    sensors_event_t event; 
    accel.getEvent(&event);
    
    // Formatta i dati in modo più compatto (rimuoviamo il contatore per ottimizzare)
    String accelData = "{\"x\":" + String(event.acceleration.x, 2) + 
                       ",\"y\":" + String(event.acceleration.y, 2) + 
                       ",\"z\":" + String(event.acceleration.z, 2) + "}";
    
    // Invia i dati al braccio robotico
    client.println(accelData);
    messageCount++;
    
    // Lampeggia il LED solo ogni 10 invii invece che ogni volta
    if (messageCount % 10 == 0) {
      digitalWrite(LED_PIN, LOW);
      lastLedBlinkTime = millis();
    }
    
    // Stampa i dati solo ogni 20 invii
    if (messageCount % 20 == 0) {
      Serial.print("Dati: ");
      Serial.println(accelData);
    }
    
    // Attendi e leggi la risposta con timeout breve
    unsigned long responseTimeout = millis();
    while (!client.available() && millis() - responseTimeout < READ_TIMEOUT) {
      // Attendi con timeout breve
      yield(); // Permette di eseguire task di sistema
    }
    
    if (client.available()) {
      // Leggi la risposta (minimo necessario)
      while (client.available()) {
        client.read(); // Leggiamo ma non processiamo per velocità
      }
    }
  }
  
  // Riaccendi il LED dopo il lampeggio
  if (lastLedBlinkTime > 0 && millis() - lastLedBlinkTime > 5) {
    digitalWrite(LED_PIN, HIGH);
    lastLedBlinkTime = 0;
  }
}

void handleWifiReconnect() {
  Serial.println("Connessione WiFi persa. Riconnessione...");
  digitalWrite(LED_PIN, LOW);
  isConnected = false;
  
  // Chiudi la connessione esistente
  if (client.connected()) {
    client.stop();
  }
  
  // Tentativo di riconnessione WiFi
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, password);
  
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000) {
    delay(100);
    Serial.print(".");
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi riconnesso");
    digitalWrite(LED_PIN, HIGH);
    
    // Riconnetti al server del braccio
    connectToRobot();
  } else {
    Serial.println("\nRiconnessione fallita. Riprovo...");
  }
}