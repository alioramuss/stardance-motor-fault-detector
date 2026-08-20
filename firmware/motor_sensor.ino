// Motor fault detector — sensor node
// Streams ax,ay,az at 200 Hz and lights the onboard LED when the
// dashboard sends 'F' (fault) back down the serial line. 'H' clears it.

#include <Wire.h>

const uint8_t  MPU         = 0x68;
const uint8_t  LED_PIN     = LED_BUILTIN;   // pin 13, already on the board
const unsigned long INTERVAL_US = 5000;     // 200 samples/sec

unsigned long next_us;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);

  Wire.beginTransmission(MPU);
  Wire.write(0x6B);      // power management
  Wire.write(0);         // wake the sensor
  Wire.endTransmission(true);

  Serial.println("timestamp,ax,ay,az");
  next_us = micros();
}

void loop() {
  // --- keep a steady 200 Hz -------------------------------------------
  next_us += INTERVAL_US;
  while ((long)(micros() - next_us) < 0) { }

  // --- listen for a verdict from the dashboard ------------------------
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'F') digitalWrite(LED_PIN, HIGH);
    else if (c == 'H') digitalWrite(LED_PIN, LOW);
  }

  // --- read the accelerometer -----------------------------------------
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);      // first accelerometer register
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU, (uint8_t)6, (uint8_t)true);

  uint8_t xh = Wire.read(), xl = Wire.read();
  uint8_t yh = Wire.read(), yl = Wire.read();
  uint8_t zh = Wire.read(), zl = Wire.read();

  int16_t ax = (int16_t)((xh << 8) | xl);
  int16_t ay = (int16_t)((yh << 8) | yl);
  int16_t az = (int16_t)((zh << 8) | zl);

  Serial.print(millis());  Serial.print(',');
  Serial.print(ax);        Serial.print(',');
  Serial.print(ay);        Serial.print(',');
  Serial.println(az);
}
