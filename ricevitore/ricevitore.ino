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
#define STEPS_PER_REV 2048   // Passi per rotazione completa del 28BYJ-48 (32 step * 64:1 rapporto di riduzione)
#define MAX_SPEED 12          // RPM massimi (il 28BYJ-48 è lento, 12-15 RPM è tipico)

// ===== SETTINGS CONTROLLER =====
#define DEADZONE_THRESHOLD 15
#define TRIGGER_THRESHOLD 25  // Soglia per i trigger R2/L2

// Inizializzazione stepper
Stepper pinzaStepper(STEPS_PER_REV, PINZA_IN1, PINZA_IN3, PINZA_IN2, PINZA_IN4);

// Variabili di stato
bool pinzaEnabled = false;
int pinzaPosition = 0;          // Tiene traccia della posizione della pinza
int pinzaMaxPosition = 2000;     // Posizione massima (corsa totale)
bool pinzaMoving = false;

void setup() {
  Serial.begin(115200);
  
  // Imposta velocità stepper
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
  }
  
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
    stepsToMove = map(r2Value, TRIGGER_THRESHOLD, 255, 1, 5);
    if (pinzaPosition + stepsToMove > pinzaMaxPosition) {
      stepsToMove = pinzaMaxPosition - pinzaPosition;
    }
    if (stepsToMove > 0) {
      pinzaStepper.step(stepsToMove);
      pinzaPosition += stepsToMove;
      pinzaMoving = true;
    }
  }
  // Chiudi pinza (L2)
  else if (l2Value > TRIGGER_THRESHOLD && pinzaPosition > 0) {
    stepsToMove = map(l2Value, TRIGGER_THRESHOLD, 255, 1, 5);
    if (pinzaPosition - stepsToMove < 0) {
      stepsToMove = pinzaPosition;
    }
    if (stepsToMove > 0) {
      pinzaStepper.step(-stepsToMove);
      pinzaPosition -= stepsToMove;
      pinzaMoving = true;
    }
  } else {
    // Nessun movimento attivo
    if (pinzaMoving) {
      pinzaMoving = false;
    }
  }
  
  // Bottoni per apertura/chiusura rapida
  if (PS4.R1()) {
    // Apri completamente
    int stepsRemaining = pinzaMaxPosition - pinzaPosition;
    if (stepsRemaining > 0) {
      pinzaStepper.step(stepsRemaining);
      pinzaPosition = pinzaMaxPosition;
    }
  }
  
  if (PS4.L1()) {
    // Chiudi completamente
    if (pinzaPosition > 0) {
      pinzaStepper.step(-pinzaPosition);
      pinzaPosition = 0;
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