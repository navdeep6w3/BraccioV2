#include <Arduino.h>
#include <PS4Controller.h>
#include <AccelStepper.h>
#include <Stepper.h>

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

// ===== MAC PS4 =====
#define PS4_MAC "b0:52:16:f4:a3:54"

// ===== SETTINGS MOTORI PRINCIPALI =====
#define DEADZONE 40
#define MAX_SPEED 20000  // Velocità massima aumentata
#define SPEED_STEP 2000  // Incremento/decremento velocità con le frecce

// ===== SETTINGS PINZA =====
#define STEPS_PER_REV 2048  // Passi per rotazione completa del 28BYJ-48
#define PINZA_MAX_SPEED 12  // RPM massimi per la pinza
#define TRIGGER_THRESHOLD 25 // Soglia per i trigger R2/L2

// ===== VARIABILI GLOBALI =====
int speedMultiplier = MAX_SPEED;  // Moltiplicatore di velocità corrente

// ===== OGGETTI STEPPER =====
AccelStepper stepperX(AccelStepper::DRIVER, X_STEP_PIN, X_DIR_PIN);
AccelStepper stepperY(AccelStepper::DRIVER, Y_STEP_PIN, Y_DIR_PIN);
AccelStepper stepperZ(AccelStepper::DRIVER, Z_STEP_PIN, Z_DIR_PIN);
Stepper pinzaStepper(STEPS_PER_REV, PINZA_IN1, PINZA_IN3, PINZA_IN2, PINZA_IN4);

// ===== VARIABILI PINZA =====
bool pinzaEnabled = false;
bool pinzaMoving = false;

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

  // Connessione PS4
  PS4.begin(PS4_MAC);
  Serial.println("In attesa di connessione PS4...");
  Serial.println("Controlli:");
  Serial.println("- Stick analogici: movimento X/Y/Z");
  Serial.println("- Frecce SU/GIU: velocità generale");
  Serial.println("- R2: chiusura pinza (continua finché premuto)");
  Serial.println("- L2: apertura pinza (continua finché premuto)");
  Serial.println("- Triangle: abilita pinza");
  Serial.println("- Circle: disabilita pinza");
  Serial.println("- Options: abilita motori principali");
  Serial.println("- Share: disabilita motori principali");
}

int applyDeadzone(int value) {
  if (abs(value) < DEADZONE) {
    return 0;
  }
  // Mappa il valore usando il moltiplicatore di velocità corrente
  if (value > 0) {
    return map(value, DEADZONE, 127, 0, speedMultiplier);
  } else {
    return map(value, -DEADZONE, -128, 0, -speedMultiplier);
  }
}

void controlPinza() {
  if (!pinzaEnabled) return;

  // Utilizzo dei trigger R2 e L2 per aprire e chiudere la pinza
  int r2Value = PS4.R2Value();
  int l2Value = PS4.L2Value();

  // Chiudi pinza (R2) - movimento continuo
  if (r2Value > TRIGGER_THRESHOLD) {
    // Calcola il numero di passi basato sull'intensità del trigger
    int stepsToMove = map(r2Value, TRIGGER_THRESHOLD, 255, 1, 10);
    pinzaStepper.step(-stepsToMove);  // Direzione chiusura
    pinzaMoving = true;
  }
  // Apri pinza (L2) - movimento continuo
  else if (l2Value > TRIGGER_THRESHOLD) {
    // Calcola il numero di passi basato sull'intensità del trigger
    int stepsToMove = map(l2Value, TRIGGER_THRESHOLD, 255, 1, 10);
    pinzaStepper.step(stepsToMove);   // Direzione apertura
    pinzaMoving = true;
  } else {
    // Nessun movimento attivo
    if (pinzaMoving) {
      pinzaMoving = false;
    }
  }
}

void disablePinzaStepper() {
  // Disattiva le bobine del motore delle pinze per non poterle usare più
  digitalWrite(PINZA_IN1, LOW);
  digitalWrite(PINZA_IN2, LOW);
  digitalWrite(PINZA_IN3, LOW);
  digitalWrite(PINZA_IN4, LOW);
}

void loop() {
  if (PS4.isConnected()) {
    // ===== CONTROLLO VELOCITÀ GENERALE CON FRECCE =====
    static bool upPressed = false;
    static bool downPressed = false;
    
    // Freccia SU - Aumenta velocità generale
    if (PS4.Up() && !upPressed) {
      speedMultiplier += SPEED_STEP;
      if (speedMultiplier > MAX_SPEED) {
        speedMultiplier = MAX_SPEED;
      }
      upPressed = true;
      Serial.print("Velocità aumentata a: ");
      Serial.println(speedMultiplier);
    } else if (!PS4.Up()) {
      upPressed = false;
    }
    
    // Freccia GIÙ - Diminuisce velocità generale
    if (PS4.Down() && !downPressed) {
      speedMultiplier -= SPEED_STEP;
      if (speedMultiplier < SPEED_STEP) {
        speedMultiplier = SPEED_STEP;
      }
      downPressed = true;
      Serial.print("Velocità diminuita a: ");
      Serial.println(speedMultiplier);
    } else if (!PS4.Down()) {
      downPressed = false;
    }

    // ===== CONTROLLO CON STICK ANALOGICI =====
    // Leggi input con dead zone applicata
    int xSpeed = applyDeadzone(PS4.RStickX());
    
    // Inverto l'asse Y dell'analogico sinistro (moltiplicando per -1)
    int ySpeed = -applyDeadzone(PS4.LStickY());
    
    int zSpeed = applyDeadzone(PS4.RStickY());

    // Controlla motori principali
    stepperX.setSpeed(xSpeed);
    stepperY.setSpeed(ySpeed);
    stepperZ.setSpeed(zSpeed);

    // ===== CONTROLLO PINZA =====
    controlPinza();

    // ===== GESTIONE ABILITAZIONE MOTORI =====
    // Abilita/disabilita motori principali
    if (PS4.Options()) {
      digitalWrite(X_ENABLE_PIN, LOW);
      digitalWrite(Y_ENABLE_PIN, LOW);
      digitalWrite(Z_ENABLE_PIN, LOW);
      Serial.println("Motori principali abilitati");
    }
    if (PS4.Share()) {
      digitalWrite(X_ENABLE_PIN, HIGH);
      digitalWrite(Y_ENABLE_PIN, HIGH);
      digitalWrite(Z_ENABLE_PIN, HIGH);
      Serial.println("Motori principali disabilitati");
    }

    // ===== GESTIONE ABILITAZIONE PINZA =====
    static bool trianglePressed = false;
    static bool circlePressed = false;
    
    // Abilita pinza
    if (PS4.Triangle() && !trianglePressed && !pinzaEnabled) {
      pinzaEnabled = true;
      trianglePressed = true;
      Serial.println("Pinza abilitata");
    } else if (!PS4.Triangle()) {
      trianglePressed = false;
    }
    
    // Disabilita pinza
    if (PS4.Circle() && !circlePressed && pinzaEnabled) {
      pinzaEnabled = false;
      circlePressed = true;
      disablePinzaStepper();
      Serial.println("Pinza disabilitata");
    } else if (!PS4.Circle()) {
      circlePressed = false;
    }

    // ===== VISUALIZZA STATO PINZA =====
    static bool crossPressed = false;
    if (PS4.Cross() && !crossPressed) {
      crossPressed = true;
      Serial.print("Pinza - Stato: ");
      Serial.print(pinzaEnabled ? "Abilitata" : "Disabilitata");
      Serial.print(" - Movimento: ");
      Serial.println(pinzaMoving ? "Attivo" : "Fermo");
    } else if (!PS4.Cross()) {
      crossPressed = false;
    }
  }

  // Esegui movimenti motori principali
  stepperX.runSpeed();
  stepperY.runSpeed();
  stepperZ.runSpeed();
  
}
