#include <Wire.h>
#include <Adafruit_PN532.h>

// NodeMCU wiring:
// SDA = D2 (GPIO4)
// SCL = D1 (GPIO5)
// IRQ = D3 (GPIO0)
// RST = D4 (GPIO2)

#define PN532_IRQ   D3
#define PN532_RESET D4   // If you did not wire RST, change this to -1

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

static void printHex(const uint8_t* data, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("PN532 I2C Test (ESP8266)");

  // ESP8266 I2C pins (SDA, SCL)
  Wire.begin(D2, D1);

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("PN532 not found!");
    Serial.println("Check: I2C mode switch, 3.3V power, SDA/SCL, and IRQ/RST wiring.");
    while (1) delay(10);
  }

  Serial.print("Found PN532. Firmware: ");
  Serial.print((versiondata >> 24) & 0xFF, HEX);
  Serial.print(".");
  Serial.println((versiondata >> 16) & 0xFF, HEX);

  nfc.SAMConfig();
  Serial.println("PN532 ready. Tap a card...");
}

void loop() {
  uint8_t uid[7];
  uint8_t uidLength;

  // Wait for card
  bool success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100);

  if (success) {
    Serial.print("Card detected! UID: ");
    printHex(uid, uidLength);
    delay(1000);
  }
}
