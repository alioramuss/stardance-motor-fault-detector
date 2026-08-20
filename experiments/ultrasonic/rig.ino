// ============================================================================
//  Motor fault detector — automated test rig
//  Uno + MPU-6050 (vibration) + HC-SR04 (obstruction) + servo (fault injection)
//
//  Two modes:
//    STREAM  (default)  continuous 200 Hz ax,ay,az for the live dashboard
//    COLLECT (send 'G') runs a self-labelling data-collection session
//
//  Serial commands from the PC:
//    G   run an automated collection session
//    K   re-run the ultrasonic calibration
//    F   LED on  (fault alarm from the dashboard)
//    H   LED off
// ============================================================================

#include <Wire.h>
#include <Servo.h>

// ---- pins ------------------------------------------------------------------
const uint8_t MPU       = 0x68;
const uint8_t TRIG_PIN  = 7;
const uint8_t ECHO_PIN  = 2;          // must be 2 or 3 on an Uno — external interrupt
const uint8_t SERVO_PIN = 9;
const uint8_t LED_PIN   = LED_BUILTIN;

// ---- rig geometry — tune these to your setup -------------------------------
const int ANGLE_CLEAR = 20;           // flap swung away from the fan
const int ANGLE_BLOCK = 110;          // flap covering the fan intake

// ---- timing ----------------------------------------------------------------
const unsigned long INTERVAL_US = 5000;   // 200 samples/sec
const unsigned long SETTLE_MS   = 1500;   // let the servo stop and airflow settle
const unsigned long RECORD_MS   = 5000;   // one trial = 5 s = 1000 samples
const unsigned long PING_MS     = 1000;   // how often to range while streaming
const int           TRIALS      = 20;     // 10 clear + 10 blocked

Servo flap;
unsigned long next_us;
long threshold_mm = 0;

// ---- non-blocking ultrasonic ----------------------------------------------
// pulseIn() blocks for up to 30 ms, which would wreck the 200 Hz sampling.
// Instead the echo pin drives an interrupt and we read the result later.
volatile unsigned long echoRise = 0, echoWidth = 0;
volatile bool echoReady = false;
unsigned long lastPing = 0;
long lastDistMM = -1;

void echoISR() {
  if (digitalRead(ECHO_PIN)) {
    echoRise = micros();
  } else if (echoRise) {
    echoWidth = micros() - echoRise;
    echoRise = 0;
    echoReady = true;
  }
}

void firePing() {
  echoReady = false;
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
}

// blocking version, only used during calibration and between trials
long pingBlockingMM() {
  firePing();
  unsigned long t0 = millis();
  while (!echoReady) { if (millis() - t0 > 60) return -1; }
  return (long)(echoWidth * 10UL / 58UL);
}

long medianPingMM() {
  long a = pingBlockingMM(); delay(60);
  long b = pingBlockingMM(); delay(60);
  long c = pingBlockingMM();
  if (a > b) { long t = a; a = b; b = t; }
  if (b > c) { long t = b; b = c; c = t; }
  if (a > b) { long t = a; a = b; b = t; }
  return b;
}

// ---- servo -----------------------------------------------------------------
// Attach only while moving. A servo holding position draws current continuously
// and the resulting supply ripple shows up as noise in the accelerometer.
void moveFlap(int angle) {
  flap.attach(SERVO_PIN);
  flap.write(angle);
  delay(700);
  flap.detach();
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

// ---- calibration -----------------------------------------------------------
// Measure the range in both flap positions and put the threshold halfway
// between. Self-calibrating, so moving the rig doesn't need code changes.
void calibrate() {
  Serial.println(F("#CAL,start"));
  moveFlap(ANGLE_CLEAR);  delay(400);
  long dClear = medianPingMM();
  moveFlap(ANGLE_BLOCK);  delay(400);
  long dBlock = medianPingMM();
  moveFlap(ANGLE_CLEAR);

  if (dClear < 0 || dBlock < 0 || abs(dClear - dBlock) < 30) {
    threshold_mm = 0;
    Serial.print(F("#CAL,fail,clear=")); Serial.print(dClear);
    Serial.print(F(",block="));          Serial.println(dBlock);
    Serial.println(F("#CAL,hint,check the sensor aims at the flap and the two positions differ by >30mm"));
  } else {
    threshold_mm = (dClear + dBlock) / 2;
    Serial.print(F("#CAL,ok,clear="));   Serial.print(dClear);
    Serial.print(F(",block="));          Serial.print(dBlock);
    Serial.print(F(",threshold="));      Serial.println(threshold_mm);
  }
}

// ---- automated collection --------------------------------------------------
void runCollection() {
  Serial.print(F("#RUN,start,trials=")); Serial.print(TRIALS);
  Serial.print(F(",record_ms="));        Serial.println(RECORD_MS);

  for (int t = 0; t < TRIALS; t++) {
    bool wantBlocked = (t % 2 == 1);
    moveFlap(wantBlocked ? ANGLE_BLOCK : ANGLE_CLEAR);
    delay(SETTLE_MS);

    // Label from what the sensor actually measures, not from what we asked for.
    // If the flap jams, the trial is still labelled truthfully.
    long d = medianPingMM();
    const char *label;
    if (threshold_mm <= 0 || d < 0)      label = "unknown";
    else if (d < threshold_mm)           label = "blocked";
    else                                 label = "clear";

    Serial.print(F("#TRIAL,")); Serial.print(t);
    Serial.print(F(","));       Serial.print(label);
    Serial.print(F(",asked="));  Serial.print(wantBlocked ? "blocked" : "clear");
    Serial.print(F(",dist="));   Serial.println(d);

    unsigned long stop = millis() + RECORD_MS;
    next_us = micros();
    while (millis() < stop) {
      next_us += INTERVAL_US;
      while ((long)(micros() - next_us) < 0) { }
      int16_t ax, ay, az;
      readAccel(ax, ay, az);
      Serial.print(millis()); Serial.print(',');
      Serial.print(ax);       Serial.print(',');
      Serial.print(ay);       Serial.print(',');
      Serial.println(az);
    }
    Serial.print(F("#END,")); Serial.println(t);
  }

  moveFlap(ANGLE_CLEAR);
  Serial.println(F("#RUN,done"));
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

  Serial.println(F("#RIG,v1,200hz,send G to collect"));
  calibrate();
  Serial.println(F("timestamp,ax,ay,az"));
  next_us = micros();
}

void loop() {
  next_us += INTERVAL_US;
  while ((long)(micros() - next_us) < 0) { }

  while (Serial.available() > 0) {
    char c = Serial.read();
    if      (c == 'F') digitalWrite(LED_PIN, HIGH);
    else if (c == 'H') digitalWrite(LED_PIN, LOW);
    else if (c == 'K') { calibrate(); next_us = micros(); }
    else if (c == 'G') { runCollection(); }
  }

  int16_t ax, ay, az;
  readAccel(ax, ay, az);

  Serial.print(millis()); Serial.print(',');
  Serial.print(ax);       Serial.print(',');
  Serial.print(ay);       Serial.print(',');
  Serial.println(az);

  // Range once a second. Firing is instant; the echo arrives on an interrupt,
  // so nothing here blocks and the 200 Hz cadence is untouched.
  if (millis() - lastPing > PING_MS) {
    lastPing = millis();
    if (echoReady) {
      lastDistMM = (long)(echoWidth * 10UL / 58UL);
      Serial.print(F("#DIST,")); Serial.println(lastDistMM);
    }
    firePing();
  }
}
