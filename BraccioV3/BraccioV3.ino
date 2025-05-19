#include <Arduino.h>
#include <PS4Controller.h>
#include <Stepper.h>

// ===== MAC PS4 =====
#define PS4_MAC "b0:52:16:f4:a3:54"

// ===== PIN STEPPER 28BYJ-48 =====
#define PINZA_IN1 27
#define PINZA_IN2 14
#define PINZA_IN3 12
#define PINZA_IN4 13

// ===== CONFIGURAZIONE STEPPER =====
#define STEPS_PER_REV 2048    // Passi per rotazione completa del 28BYJ-48
#define MAX_SPEED 10          // RPM ridotti per maggiore coppia
#define HOLD_TORQUE_TIME 5000 // Tempo di mantenimento della coppia in millisecondi

// ===== SETTINGS CONTROLLER =====
#define DEADZONE_THRESHOLD 15
#define TRIGGER_THRESHOLD 25

// Inizializzazione stepper
Stepper pinzaStepper(STEPS_PER_REV, PINZA_IN1, PINZA_IN3, PINZA_IN2, PINZA_IN4);

// Variabili di stato
bool pinzaEnabled = false;
int pinzaPosition = 0;
int pinzaMaxPosition = 4000;
bool pinzaMoving = false;
bool highTorqueMode = false;
unsigned long lastMoveTime = 0;
bool holdingPosition = false;

void setup() {
  Serial.begin(115200);
  
  // Configurazione pin come OUTPUT
  pinMode(PINZA_IN1, OUTPUT);
  pinMode(PINZA_IN2, OUTPUT);
  pinMode(PINZA_IN3, OUTPUT);
  pinMode(PINZA_IN4, OUTPUT);
  
  // Imposta velocità stepper (più bassa per maggiore coppia)
  pinzaStepper.setSpeed(MAX_SPEED);
  
  // Connessione PS4
  PS4.begin(PS4_MAC);
  Serial.println("In attesa di connessione PS4...");
}

void loop() {
  if (PS4.isConnected()) {
    // Controllo Pinza
    controlPinza();
    
    // Visualizza stato
    if (PS4.Cross()) {
      Serial.print("Posizione pinza: ");
      Serial.println(pinzaPosition);
      Serial.print("Modalità alta coppia: ");
      Serial.println(highTorqueMode ? "Attiva" : "Disattiva");
    }
    
    // Abilita/disabilita pinza
    if (PS4.Triangle() && !pinzaEnabled) {
      pinzaEnabled = true;
      Serial.println("Pinza abilitata");
    }
    
    if (PS4.Circle() && pinzaEnabled) {
      pinzaEnabled = false;
      disableStepper();
      Serial.println("Pinza disabilitata");
    }
    
    // Modalità alta coppia (Square/Quadrato)
    if (PS4.Square()) {
      highTorqueMode = !highTorqueMode;
      Serial.print("Modalità alta coppia: ");
      Serial.println(highTorqueMode ? "Attiva" : "Disattiva");
      delay(300); // Debounce
    }
  }
  
  // Gestione del mantenimento della posizione
  manageTorqueHolding();
  
  delay(10); // Breve pausa per stabilità
}

void controlPinza() {
  if (!pinzaEnabled) return;
  
  // Utilizzo dei trigger R2 e L2 per aprire e chiudere la pinza
  int r2Value = PS4.R2Value();
  int l2Value = PS4.L2Value();
  
  // Inizializza movimenti di pinza
  int stepsToMove = 0;
  
  // Apri pinza (R2)
  if (r2Value > TRIGGER_THRESHOLD && pinzaPosition < pinzaMaxPosition) {
    // Movimento più lento per aumentare la coppia (1-3 step alla volta)
    stepsToMove = map(r2Value, TRIGGER_THRESHOLD, 255, 1, highTorqueMode ? 2 : 4);
    if (pinzaPosition + stepsToMove > pinzaMaxPosition) {
      stepsToMove = pinzaMaxPosition - pinzaPosition;
    }
    if (stepsToMove > 0) {
      pinzaStepper.step(stepsToMove);
      pinzaPosition += stepsToMove;
      pinzaMoving = true;
      lastMoveTime = millis();
      holdingPosition = true;
    }
  }
  // Chiudi pinza (L2)
  else if (l2Value > TRIGGER_THRESHOLD && pinzaPosition > 0) {
    stepsToMove = map(l2Value, TRIGGER_THRESHOLD, 255, 1, highTorqueMode ? 2 : 4);
    if (pinzaPosition - stepsToMove < 0) {
      stepsToMove = pinzaPosition;
    }
    if (stepsToMove > 0) {
      pinzaStepper.step(-stepsToMove);
      pinzaPosition -= stepsToMove;
      pinzaMoving = true;
      lastMoveTime = millis();
      holdingPosition = true;
    }
  } else {
    // Nessun movimento attivo
    if (pinzaMoving) {
      pinzaMoving = false;
    }
  }
  
  // Bottoni per apertura/chiusura con incrementi minori per più forza
  if (PS4.R1()) {
    // Apri con piccoli incrementi per maggiore coppia
    int increment = highTorqueMode ? 32 : 64;
    int targetPos = min(pinzaPosition + increment, pinzaMaxPosition);
    int stepsToMove = targetPos - pinzaPosition;
    
    if (stepsToMove > 0) {
      pinzaStepper.step(stepsToMove);
      pinzaPosition = targetPos;
      lastMoveTime = millis();
      holdingPosition = true;
    }
  }
  
  if (PS4.L1()) {
    // Chiudi con piccoli incrementi per maggiore coppia
    int increment = highTorqueMode ? 32 : 64;
    int targetPos = max(pinzaPosition - increment, 0);
    int stepsToMove = targetPos - pinzaPosition;
    
    if (stepsToMove < 0) {
      pinzaStepper.step(stepsToMove);
      pinzaPosition = targetPos;
      lastMoveTime = millis();
      holdingPosition = true;
    }
  }
}

void manageTorqueHolding() {
  // Se siamo in modalità alta coppia e stiamo mantenendo la posizione
  if (holdingPosition && highTorqueMode) {
    // Mantieni la coppia per un periodo definito
    if (millis() - lastMoveTime > HOLD_TORQUE_TIME) {
      // Dopo il periodo di mantenimento, rilascia se non in modalità mantenimento continuo
      disableStepper();
      holdingPosition = false;
    }
    // Altrimenti, lascia alimentate le bobine per mantenere la coppia
  } else if (holdingPosition && !highTorqueMode) {
    // In modalità normale, disattiva dopo un breve periodo
    if (millis() - lastMoveTime > 1000) {
      disableStepper();
      holdingPosition = false;
    }
  }
}

void disableStepper() {
  // Disattiva le bobine del motore per risparmiare energia
  digitalWrite(PINZA_IN1, LOW);
  digitalWrite(PINZA_IN2, LOW);
  digitalWrite(PINZA_IN3, LOW);
  digitalWrite(PINZA_IN4, LOW);
}

// Funzione per mantenere la forza continua (utile per la presa)
void enableContinuousHolding() {
  // Implementazione con half-stepping per più coppia
  switch (pinzaPosition % 8) {
    case 0:
      digitalWrite(PINZA_IN1, HIGH);
      digitalWrite(PINZA_IN2, LOW);
      digitalWrite(PINZA_IN3, LOW);
      digitalWrite(PINZA_IN4, LOW);
      break;
    case 1:
      digitalWrite(PINZA_IN1, HIGH);
      digitalWrite(PINZA_IN2, HIGH);
      digitalWrite(PINZA_IN3, LOW);
      digitalWrite(PINZA_IN4, LOW);
      break;
    case 2:
      digitalWrite(PINZA_IN1, LOW);
      digitalWrite(PINZA_IN2, HIGH);
      digitalWrite(PINZA_IN3, LOW);
      digitalWrite(PINZA_IN4, LOW);
      break;
    case 3:
      digitalWrite(PINZA_IN1, LOW);
      digitalWrite(PINZA_IN2, HIGH);
      digitalWrite(PINZA_IN3, HIGH);
      digitalWrite(PINZA_IN4, LOW);
      break;
    case 4:
      digitalWrite(PINZA_IN1, LOW);
      digitalWrite(PINZA_IN2, LOW);
      digitalWrite(PINZA_IN3, HIGH);
      digitalWrite(PINZA_IN4, LOW);
      break;
    case 5:
      digitalWrite(PINZA_IN1, LOW);
      digitalWrite(PINZA_IN2, LOW);
      digitalWrite(PINZA_IN3, HIGH);
      digitalWrite(PINZA_IN4, HIGH);
      break;
    case 6:
      digitalWrite(PINZA_IN1, LOW);
      digitalWrite(PINZA_IN2, LOW);
      digitalWrite(PINZA_IN3, LOW);
      digitalWrite(PINZA_IN4, HIGH);
      break;
    case 7:
      digitalWrite(PINZA_IN1, HIGH);
      digitalWrite(PINZA_IN2, LOW);
      digitalWrite(PINZA_IN3, LOW);
      digitalWrite(PINZA_IN4, HIGH);
      break;
  }
}