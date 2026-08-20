// ============================================================================
//  Motor fault detector — non-contact sensing experiment
//
//  Research question: can an HC-SR04 ultrasonic rangefinder detect fan
//  imbalance that the MPU-6050 detects by contact — and at what angle and
//  standoff does it work best (or fail)?
//
//  The servo carries the HC-SR04 and sweeps it through a set of angles.
//  At each angle both sensors record simultaneously: accelerometer at 200 Hz
//  (the reference truth) and rangefinder at ~25 Hz (the thing under test).
//
//  Serial commands from the PC:
//    U   run a full angle sweep
//    F   LED on      H   LED off
//    S   report the servo angle list and exit
// ============================================================================

#include <Wire.h>
#include <Servo.h>

// ---- pins ------------------------------------------------------------------
const uint8_t MPU       = 0x68;
const uint8_t TRIG_PIN  = 7;
const uint8_t ECHO_PIN  = 2;      // must be D2 or D3 on an Uno — external interrupt
const uint8_t SERVO_PIN = 9;
const uint8_t LED_PIN   = LED_BUILTIN;

// ---- sweep -----------------------------------------------------------------
const int ANGLES[]   = { 60, 75, 90, 105, 120 };
const int N_ANGLES   = sizeof(ANGLES) / sizeof(ANGLES[0]);
const unsigned long SETTLE_MS  = 1200;
const unsigned long RECORD_MS  = 10000;   // 10 s per angle
const unsigned long INTERVAL_US = 5000;   // 200 Hz accelerometer

// HC-SR04 needs time between triggers or the previous echo is still ringing.
// The datasheet says 60 ms; 40 ms is safe at short range. That gives ~25 Hz,
// which is BELOW Nyquist for a 37.5 Hz fan — see the design notes. We are
// measuring the spread of the readings, not trying to reconstruct the waveform.
const unsigned long PING_US = 40000;

Servo arm;
unsigned long next_us, lastPing_us;

// ---- non-blocking ultrasonic ----------------------------------------------
volatile unsigned long echoRise = 0, echoWidth = 0;
volatile bool echoReady = false;

void echoISR() {
  if (digitalRead(ECHO_PIN)) {
    echoRise = micros();
  } else if (echoRise) {
    echoWidth = micros() - echoRise;
    echoRise  = 0;
    echoReady = true;
  }
}

void firePing() {
  echoReady = false;
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
}

// ---- accelerometer ---------------------------------------------------------
void readAccel(int16_t &ax, int16_t &ay, int16_t &az) {
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU, (uint8_t)6, (uint8_t)true);
  uint8_t xh = Wire.read(), xl = Wire.read();
  uint8_t yh = Wire.read(), yl = Wire.read();
  uint8_t zh = Wire.read(), zl = Wire.read();
  ax = (int16_t)((xh << 8) | xl);
  ay = (int16_t)((yh << 8) | yl);
  az = (int16_t)((zh << 8) | zl);
}

// ---- servo -----------------------------------------------------------------
// Attached only while moving. A servo holding position pulses every 20 ms and
// its current draw ripples the 5 V rail, which the accelerometer sees as noise.
// If the HC-SR04 droops when detached, set HOLD to true and power the servo
// from its own supply.
const bool HOLD = false;

void moveArm(int angle) {
  arm.attach(SERVO_PIN);
  arm.write(angle);
  delay(700);
  if (!HOLD) arm.detach();
}

// ---- the sweep -------------------------------------------------------------
void runSweep() {
  Serial.print(F("#SWEEP,start,angles=")); Serial.print(N_ANGLES);
  Serial.print(F(",record_ms="));          Serial.println(RECORD_MS);

  for (int i = 0; i < N_ANGLES; i++) {
    moveArm(ANGLES[i]);
    delay(SETTLE_MS);

    Serial.print(F("#ANGLE,")); Serial.print(i);
    Serial.print(F(","));       Serial.println(ANGLES[i]);

    unsigned long stop = millis() + RECORD_MS;
    next_us     = micros();
    lastPing_us = micros();
    firePing();

    while (millis() < stop) {
      next_us += INTERVAL_US;
      while ((long)(micros() - next_us) < 0) { }

      int16_t ax, ay, az;
      readAccel(ax, ay, az);
      Serial.print(millis()); Serial.print(',');
      Serial.print(ax);       Serial.print(',');
      Serial.print(ay);       Serial.print(',');
      Serial.println(az);

      if (micros() - lastPing_us >= PING_US) {
        lastPing_us = micros();
        long mm = -1;
        if (echoReady) mm = (long)(echoWidth * 10UL / 58UL);   // -1 = no echo
        Serial.print(F("#D,")); Serial.print(millis());
        Serial.print(F(","));   Serial.println(mm);
        firePing();
      }
    }
    Serial.print(F("#ENDANGLE,")); Serial.println(i);
  }

  moveArm(90);
  Serial.println(F("#SWEEP,done"));
  next_us = micros();
}

// ---- setup / loop ----------------------------------------------------------
void setup() {
  pinMode(LED_PIN, OUTPUT);  digitalWrite(LED_PIN, LOW);
  pinMode(TRIG_PIN, OUTPUT); digitalWrite(TRIG_PIN, LOW);
  pinMode(ECHO_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(ECHO_PIN), echoISR, CHANGE);

  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  moveArm(90);
  Serial.println(F("#RIG,v2,200hz,send U to sweep"));
  Serial.println(F("timestamp,ax,ay,az"));
  next_us     = micros();
  lastPing_us = micros();
}

void loop() {
  next_us += INTERVAL_US;
  while ((long)(micros() - next_us) < 0) { }

  while (Serial.available() > 0) {
    char c = Serial.read();
    if      (c == 'F') digitalWrite(LED_PIN, HIGH);
    else if (c == 'H') digitalWrite(LED_PIN, LOW);
    else if (c == 'U') runSweep();
    else if (c == 'S') {
      Serial.print(F("#ANGLES"));
      for (int i = 0; i < N_ANGLES; i++) { Serial.print(','); Serial.print(ANGLES[i]); }
      Serial.println();
      next_us = micros();
    }
  }

  int16_t ax, ay, az;
  readAccel(ax, ay, az);
  Serial.print(millis()); Serial.print(',');
  Serial.print(ax);       Serial.print(',');
  Serial.print(ay);       Serial.print(',');
  Serial.println(az);

  // slow ranging while idle, so the dashboard can show a live distance later
  if (micros() - lastPing_us >= 500000UL) {
    lastPing_us = micros();
    if (echoReady) {
      Serial.print(F("#D,")); Serial.print(millis());
      Serial.print(F(","));   Serial.println((long)(echoWidth * 10UL / 58UL));
    }
    firePing();
  }
}
