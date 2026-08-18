#include <Wire.h>

// ---- MPU-6050 registers (from the datasheet) ----
const uint8_t MPU_ADDR         = 0x68;
const uint8_t REG_PWR_MGMT_1   = 0x6B;  // sleep control
const uint8_t REG_CONFIG       = 0x1A;  // low-pass filter
const uint8_t REG_ACCEL_CONFIG = 0x1C;  // measurement range
const uint8_t REG_ACCEL_XOUT_H = 0x3B;  // first accel data byte

const float LSB_PER_G = 8192.0;              // counts per 1g at +/-4g
const unsigned long SAMPLE_INTERVAL_MS = 5;  // 5ms = 200 samples/sec

unsigned long lastSample = 0;

void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);

  writeRegister(REG_PWR_MGMT_1, 0x00);   // wake the sensor
  writeRegister(REG_CONFIG, 0x00);       // filter off
  writeRegister(REG_ACCEL_CONFIG, 0x08); // +/-4g range

  Serial.println("t_ms,ax_g,ay_g,az_g");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSample < SAMPLE_INTERVAL_MS) return;
  lastSample = now;

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_ACCEL_XOUT_H);
  Wire.endTransmission(false);

  Wire.requestFrom(MPU_ADDR, (uint8_t)6, (uint8_t)true);
  if (Wire.available() < 6) return;

  uint8_t xHigh = Wire.read(), xLow = Wire.read();
  uint8_t yHigh = Wire.read(), yLow = Wire.read();
  uint8_t zHigh = Wire.read(), zLow = Wire.read();

  int16_t rawX = (int16_t)((xHigh << 8) | xLow);
  int16_t rawY = (int16_t)((yHigh << 8) | yLow);
  int16_t rawZ = (int16_t)((zHigh << 8) | zLow);

  Serial.print(now);                 Serial.print(",");
  Serial.print(rawX / LSB_PER_G, 4); Serial.print(",");
  Serial.print(rawY / LSB_PER_G, 4); Serial.print(",");
  Serial.println(rawZ / LSB_PER_G, 4);
}
