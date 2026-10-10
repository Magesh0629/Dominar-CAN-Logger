/*
  Bench test 2: SD card module wiring + card format.
  Wiring needed: SD module only  (SCK=18, MISO=19, MOSI=23, CS=15, power, GND).
  PASS: prints "PASS: wrote and read back test file" and the card size.
  FAIL hints: check CS pin, that the card is FAT32 (<=32GB), and the module's VCC
              (5V for modules with an onboard regulator, 3V3 for bare breakouts).
*/
#include <SPI.h>
#include <SD.h>

#define SD_CS_PIN 15

void setup() {
  Serial.begin(115200);
  delay(1000);
  SPI.begin(18, 19, 23, SD_CS_PIN);

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("FAIL: SD.begin() failed (wiring, power, or card format)");
    return;
  }
  if (SD.cardType() == CARD_NONE) {
    Serial.println("FAIL: no SD card detected");
    return;
  }
  Serial.print("Card size: ");
  Serial.print((uint32_t)(SD.cardSize() / (1024 * 1024)));
  Serial.println(" MB");

  SD.remove("/test.txt");
  File f = SD.open("/test.txt", FILE_WRITE);
  if (!f) {
    Serial.println("FAIL: could not create /test.txt");
    return;
  }
  f.println("dominar can logger sd test");
  f.close();

  f = SD.open("/test.txt");
  String line = f.readStringUntil('\n');
  f.close();
  line.trim();

  if (line == "dominar can logger sd test") {
    Serial.println("PASS: wrote and read back test file");
  } else {
    Serial.println("FAIL: read-back mismatch");
  }
}

void loop() {}
