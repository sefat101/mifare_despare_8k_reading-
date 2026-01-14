#include <SPI.h>
#include <PN532_SPI.h>
#include <PN532.h>

// --- SPI pins for ESP32 (VSPI) ---
static const int PN532_SCK  = 18;
static const int PN532_MISO = 19;
static const int PN532_MOSI = 23;
static const int PN532_SS   = 5;   // Chip Select (CS)

// Create SPI bus instance (VSPI)
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

static bool inDataExchange(const uint8_t* apdu, uint8_t apduLen, uint8_t* resp, uint8_t* respLen) {
  int16_t len = nfc.inDataExchange((uint8_t*)apdu, apduLen, resp, 255);
  if (len <= 0) return false;
  *respLen = (uint8_t)len;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nESP32 + PN532 (SPI) -> DESFire Roll Reader");

  // Start SPI with explicit pins
  spi.begin(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("PN532 not found. Check SPI wiring + set PN532 to SPI mode.");
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
  uint8_t respLen = 0;

  if (!inDataExchange(selectApp, sizeof(selectApp), resp, &respLen)) {
    Serial.println("SelectApplication: no response");
    delay(1000);
    return;
  }

  Serial.print("SelectApp resp: ");
  printHex(resp, respLen);

  if (!desfireOK(resp, respLen)) {
    Serial.println("SelectApplication failed (wrong AID or access rights).");
    delay(1000);
    return;
  }

  // 2) ReadData file=01 offset=000000 length=00000A (10 bytes)
  const uint8_t readRoll10[] = {
    0x90, 0xBD, 0x00, 0x00, 0x07,
    0x01,
    0x00, 0x00, 0x00,   // offset
    0x00, 0x00, 0x0A,   // length 10
    0x00
  };

  if (!inDataExchange(readRoll10, sizeof(readRoll10), resp, &respLen)) {
    Serial.println("ReadData: no response");
    delay(1000);
    return;
  }

  Serial.print("ReadData resp: ");
  printHex(resp, respLen);

  if (!desfireOK(resp, respLen)) {
    Serial.println("ReadData failed (needs auth? wrong file?).");
    delay(1000);
    return;
  }

  // payload = respLen - 2 (strip 91 00)
  uint8_t payloadLen = respLen - 2;

  Serial.print("Roll (HEX): ");
  printHex(resp, payloadLen);

  Serial.print("Roll (ASCII): ");
  for (uint8_t i = 0; i < payloadLen; i++) Serial.print((char)resp[i]);
  Serial.println();

  // clean digits only
  String roll = "";
  for (uint8_t i = 0; i < payloadLen; i++) {
    if (resp[i] >= '0' && resp[i] <= '9') roll += (char)resp[i];
  }
  Serial.print("Roll (digits): ");
  Serial.println(roll);

  delay(2000);
}
