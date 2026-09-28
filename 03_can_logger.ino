/*
  Step 2: CAN bus logger with event markers
  ------------------------------------------
  Logs every CAN frame to SD as CSV, in listen-only mode (safe, never
  transmits/ACKs onto the bus). While it runs, type a single character
  into the Serial Monitor + Enter to drop a labeled timestamp marker
  into the log — do this right when you rev the throttle, hit the
  brake, flick a turn signal, etc. Those markers are what let you
  correlate raw bytes to real signals in step 3.

  Suggested marker convention (use your own, just be consistent):
    r = throttle/RPM blip       b = front brake
    c = rear brake              t = turn signal on
    T = turn signal off         g = gear change
    h = headlight/high beam     s = speed change note

  Library required: "arduino-mcp2515" by autowp, and the built-in SD library.

  Set CAN_SPEED below to whatever 02_baud_scanner.ino found.
*/

#include <SPI.h>
#include <mcp2515.h>
#include <SD.h>

#define CAN_CS_PIN 5
#define SD_CS_PIN  15

#define CRYSTAL MCP_8MHZ          // match your MCP2515 board's crystal
#define BUS_SPEED CAN_500KBPS     // <-- set from the scanner's result

const char* LOG_FILENAME = "/canlog.csv";
const unsigned long FLUSH_INTERVAL_MS = 200;

MCP2515 mcp2515(CAN_CS_PIN);
struct can_frame canMsg;
File logFile;

unsigned long lastFlush = 0;
unsigned long frameCount = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  SPI.begin(18, 19, 23, CAN_CS_PIN); // SCK, MISO, MOSI, default SS

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD init FAILED. Check wiring/card format (FAT32).");
    while (1) delay(1000);
  }
  Serial.println("SD OK.");

  mcp2515.reset();
  mcp2515.setBitrate(BUS_SPEED, CRYSTAL);
  mcp2515.setListenOnlyMode(); // never transmits onto the bus

  bool isNewFile = !SD.exists(LOG_FILENAME);
  logFile = SD.open(LOG_FILENAME, FILE_WRITE);
  if (!logFile) {
    Serial.println("Could not open log file.");
    while (1) delay(1000);
  }
  if (isNewFile) {
    logFile.println("timestamp_ms,id,dlc,d0,d1,d2,d3,d4,d5,d6,d7");
    logFile.flush();
  }

  Serial.println("Logging started. Type a marker character + Enter");
  Serial.println("in this Serial Monitor when you perform a test action.");
}

void loop() {
  // 1. Log any incoming CAN frame
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    logFrame(canMsg);
    frameCount++;
  }

  // 2. Check for a manual event marker typed into Serial
  if (Serial.available()) {
    char marker = Serial.read();
    if (marker != '\n' && marker != '\r') {
      logEvent(marker);
      Serial.print("Marker logged: ");
      Serial.println(marker);
    }
  }

  // 3. Periodic flush so data survives a power loss
  if (millis() - lastFlush > FLUSH_INTERVAL_MS) {
    logFile.flush();
    lastFlush = millis();
  }
}

void logFrame(const struct can_frame &f) {
  logFile.print(millis());
  logFile.print(",");
  logFile.print(f.can_id, HEX);
  logFile.print(",");
  logFile.print(f.can_dlc);
  for (int i = 0; i < 8; i++) {
    logFile.print(",");
    if (i < f.can_dlc) {
      logFile.print(f.data[i], HEX);
    }
  }
  logFile.println();
}

void logEvent(char marker) {
  logFile.print(millis());
  logFile.print(",EVENT,");
  logFile.print((int)marker);
  logFile.println(",,,,,,,,"); // pad to same column count as frame rows
}
