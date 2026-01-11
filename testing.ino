#include <Wire.h>
#include <Adafruit_PN532.h>

#define PN532_IRQ   D3
#define PN532_RESET D4   // set to -1 if not connected

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

void setup() {
  Serial.begin(115200);
  Wire.begin(D2, D1); // SDA=D2(GPIO4), SCL=D1(GPIO5) for ESP8266

  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Didn't find PN532. Check wiring, power, IRQ/RESET.");
    while (1) delay(10);
  }

  Serial.print("Found PN532. Firmware: ");
  Serial.print((versiondata >> 24) & 0xFF, HEX);
  Serial.print(".");
  Serial.println((versiondata >> 16) & 0xFF, HEX);

  nfc.SAMConfig();
  Serial.println("PN532 ready.");
}

void loop() {}
