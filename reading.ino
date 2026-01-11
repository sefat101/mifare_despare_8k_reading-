#include <Wire.h>
#include <Adafruit_PN532.h>

// Use PN532 over I2C
Adafruit_PN532 nfc(PN532_I2C_INTERFACE);

static void printHex(const uint8_t* data, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

static void printAscii(const uint8_t* data, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    char c = (data[i] >= 32 && data[i] <= 126) ? (char)data[i] : '.';
    Serial.print(c);
  }
  Serial.println();
}

// Sends an APDU to the selected card and returns response
static bool sendApdu(const uint8_t* apdu, uint8_t apduLen, uint8_t* resp, uint8_t* respLen) {
  // respLen must be set to max size before calling
  if (!nfc.inDataExchange((uint8_t*)apdu, apduLen, resp, respLen)) {
    return false;
  }
  return true;
}

static bool respIsOK(const uint8_t* resp, uint8_t respLen) {
  // DESFire status bytes usually end with 0x91 0x00 when OK
  return (respLen >= 2 && resp[respLen - 2] == 0x91 && resp[respLen - 1] == 0x00);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("ESP8266 + PN532 -> MIFARE DESFire read bytes 0000..0009");

  // I2C pins for ESP8266 (NodeMCU): SDA=D2(GPIO4), SCL=D1(GPIO5)
  Wire.begin(4, 5);

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("PN532 not found. Check wiring/power.");
    while (1) delay(10);
  }

  Serial.print("Found PN532. Firmware: ");
  Serial.print((versiondata >> 24) & 0xFF, HEX);
  Serial.print(".");
  Serial.println((versiondata >> 16) & 0xFF, HEX);

  nfc.SAMConfig(); // Enable PN532 to read cards
  Serial.println("Tap your DESFire card...");
}

void loop() {
  uint8_t uid[7];
  uint8_t uidLength = 0;

  // Detect ISO14443A card
  if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50)) {
    delay(150);
    return;
  }

  Serial.print("\nCard detected. UID: ");
  printHex(uid, uidLength);

  // ---- 1) Select DESFire Application: AID = 0x52 0x55 0x44 ----
  const uint8_t selectApp[] = {
    0x90, 0x5A, 0x00, 0x00, 0x03,  // CLA INS P1 P2 Lc
    0x52, 0x55, 0x44,              // AID (3 bytes)
    0x00                           // Le
  };

  uint8_t resp[128];
  uint8_t respLen;

  respLen = sizeof(resp);
  if (!sendApdu(selectApp, sizeof(selectApp), resp, &respLen)) {
    Serial.println("SelectApplication: PN532 exchange failed");
    delay(1200);
    return;
  }

  Serial.print("SelectApp response: ");
  printHex(resp, respLen);

  if (!respIsOK(resp, respLen)) {
    Serial.println("SelectApplication failed (wrong AID / access rights).");
    delay(1200);
    return;
  }

  // ---- 2) ReadData: FileNo=0x01, Offset=0x000000, Length=0x00000A (10 bytes) ----
  const uint8_t readData10[] = {
    0x90, 0xBD, 0x00, 0x00, 0x07,  // CLA INS P1 P2 Lc
    0x01,                          // FileNo
    0x00, 0x00, 0x00,              // Offset (3 bytes)
    0x00, 0x00, 0x0A,              // Length (3 bytes) = 10
    0x00                           // Le
  };

  respLen = sizeof(resp);
  if (!sendApdu(readData10, sizeof(readData10), resp, &respLen)) {
    Serial.println("ReadData: PN532 exchange failed");
    delay(1200);
    return;
  }

  Serial.print("ReadData response: ");
  printHex(resp, respLen);

  if (!respIsOK(resp, respLen)) {
    Serial.println("ReadData failed (maybe authentication required or wrong file).");
    delay(1200);
    return;
  }

  // Payload is respLen - 2 (strip 0x91 0x00)
  uint8_t payloadLen = respLen - 2;

  Serial.print("Roll bytes (HEX): ");
  printHex(resp, payloadLen);

  Serial.print("Roll (ASCII): ");
  printAscii(resp, payloadLen);

  // If you want to use it as a String (trim non-printables):
  String roll = "";
  for (uint8_t i = 0; i < payloadLen; i++) {
    if (resp[i] >= 32 && resp[i] <= 126) roll += (char)resp[i];
  }
  Serial.print("Roll (clean): ");
  Serial.println(roll);

  delay(2000);
}
