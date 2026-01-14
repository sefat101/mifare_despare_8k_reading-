#include <SPI.h>
#include <PN532_SPI.h>
#include <PN532.h>

// ESP32 VSPI pins
static const int PN532_SCK  = 18;
static const int PN532_MISO = 19;
static const int PN532_MOSI = 23;
static const int PN532_SS   = 5;   // CS/SSEL

SPIClass spi(VSPI);
PN532_SPI pn532spi(spi, PN532_SS);
PN532 nfc(pn532spi);

static void printHex(const uint8_t* data, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

static bool desfireOK(const uint8_t* resp, uint8_t respLen) {
  // DESFire success ends with 0x91 0x00
  return (respLen >= 2 && resp[respLen - 2] == 0x91 && resp[respLen - 1] == 0x00);
}

// Correct wrapper for YOUR PN532 library signature
static bool sendApdu(const uint8_t* apdu, uint8_t apduLen, uint8_t* resp, uint8_t* respLen) {
  // respLen must contain max buffer size before call in some libs.
  // We'll assume resp is big enough and just let library fill respLen.
  return nfc.inDataExchange((uint8_t*)apdu, apduLen, resp, respLen);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nESP32 + PN532(SPI) DESFire Roll Reader");

  spi.begin(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("PN532 not found. Check wiring + ensure PN532 is in SPI mode.");
    while (1) delay(10);
  }

  Serial.print("Found PN532. Firmware: ");
  Serial.print((versiondata >> 24) & 0xFF, HEX);
  Serial.print(".");
  Serial.println((versiondata >> 16) & 0xFF, HEX);

  nfc.SAMConfig();
  Serial.println("Tap your DESFire card...");
}

void loop() {
  uint8_t uid[10];
  uint8_t uidLen = 0;

  if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 50)) {
    delay(100);
    return;
  }

  Serial.print("\nCard UID: ");
  printHex(uid, uidLen);

  // 1) SelectApplication AID = 52 55 44
  const uint8_t selectApp[] = { 0x90, 0x5A, 0x00, 0x00, 0x03, 0x52, 0x55, 0x44, 0x00 };

  uint8_t resp[255];
  uint8_t respLen = sizeof(resp); // max len allowed to write (some versions ignore this)

  if (!sendApdu(selectApp, sizeof(selectApp), resp, &respLen)) {
    Serial.println("SelectApplication: failed (no response).");
    delay(1000);
    return;
  }

  Serial.print("SelectApp resp: ");
  printHex(resp, respLen);

  if (!desfireOK(resp, respLen)) {
    Serial.println("SelectApplication NOT OK (wrong AID / access rights).");
    delay(1000);
    return;
  }

  // 2) ReadData: File 0x01, offset 0x000000, length 10 bytes (0x00000A)
  const uint8_t readRoll10[] = {
    0x90, 0xBD, 0x00, 0x00, 0x07,
    0x01,
    0x00, 0x00, 0x00,  // offset
    0x00, 0x00, 0x0A,  // length = 10
    0x00
  };

  respLen = sizeof(resp);

  if (!sendApdu(readRoll10, sizeof(readRoll10), resp, &respLen)) {
    Serial.println("ReadData: failed (no response).");
    delay(1000);
    return;
  }

  Serial.print("ReadData resp: ");
  printHex(resp, respLen);

  if (!desfireOK(resp, respLen)) {
    Serial.println("ReadData NOT OK (needs auth? wrong file?).");
    delay(1000);
    return;
  }

  // Payload = everything except last 2 bytes (91 00)
  uint8_t payloadLen = respLen - 2;

  Serial.print("Roll (ASCII): ");
  for (uint8_t i = 0; i < payloadLen; i++) Serial.print((char)resp[i]);
  Serial.println();

  // digits only
  String roll = "";
  for (uint8_t i = 0; i < payloadLen; i++) {
    if (resp[i] >= '0' && resp[i] <= '9') roll += (char)resp[i];
  }
  Serial.print("Roll (digits): ");
  Serial.println(roll);

  delay(2000);
}
