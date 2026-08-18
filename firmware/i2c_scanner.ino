#include <Wire.h>

void setup() {
  Serial.begin(9600);
  while (!Serial) {}
  Wire.begin();
  Serial.println("Scanning I2C bus...");
}

void loop() {
  int found = 0;

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("Device found at 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      found++;
    }
  }

  if (found == 0) {
    Serial.println("No devices found");
  }

  Serial.println("---");
  delay(3000);
}
