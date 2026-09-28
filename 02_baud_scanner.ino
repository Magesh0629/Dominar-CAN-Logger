/*
  Step 1: CAN bitrate scanner
  ----------------------------
  Cycles through common automotive CAN speeds in listen-only mode and
  reports how many clean frames were received on each, so you can find
  the Dominar's actual bus speed without guessing or risking the bus.

  Library required: "arduino-mcp2515" by autowp (install via Library Manager)

  IMPORTANT: set CRYSTAL below to match your MCP2515 board's crystal
  (check the metal can on the board — usually printed 8.000 or 16.000).
*/

#include <SPI.h>
#include <mcp2515.h>

#define CAN_CS_PIN 5

// --- set this to match your board's crystal ---
#define CRYSTAL MCP_8MHZ
// If your board has a 16MHz crystal instead, use MCP_16MHZ

MCP2515 mcp2515(CAN_CS_PIN);

struct Candidate {
  CAN_SPEED speed;
  const char* label;
};

Candidate candidates[] = {
  { CAN_125KBPS, "125 kbps" },
  { CAN_250KBPS, "250 kbps" },
  { CAN_500KBPS, "500 kbps" },
  { CAN_1000KBPS, "1000 kbps" }
};

const int NUM_CANDIDATES = sizeof(candidates) / sizeof(candidates[0]);
const unsigned long WINDOW_MS = 2500; // listen time per candidate

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  SPI.begin(18, 19, 23, CAN_CS_PIN); // SCK, MISO, MOSI, SS

  Serial.println();
  Serial.println("=== CAN Bitrate Scanner ===");
  Serial.println("Make sure CAN_H/CAN_L are connected before starting.");
  Serial.println();

  for (int i = 0; i < NUM_CANDIDATES; i++) {
    testSpeed(candidates[i]);
  }

  Serial.println();
  Serial.println("Scan complete. The speed with the most clean frames");
  Serial.println("and few/no error flags is almost certainly correct.");
  Serial.println("Use that value in 03_can_logger.ino.");
}

void loop() {
  // nothing — one-shot scan
}

void testSpeed(Candidate c) {
  mcp2515.reset();
  mcp2515.setBitrate(c.speed, CRYSTAL);
  mcp2515.setListenOnlyMode();

  unsigned long start = millis();
  unsigned long cleanFrames = 0;
  unsigned long errorEvents = 0;
  struct can_frame frame;

  while (millis() - start < WINDOW_MS) {
    if (mcp2515.readMessage(&frame) == MCP2515::ERROR_OK) {
      cleanFrames++;
    }
    uint8_t eflg = mcp2515.getErrorFlags();
    if (eflg != 0) {
      errorEvents++;
      mcp2515.clearRXnOVRFlags();
    }
  }

  Serial.print(c.label);
  Serial.print(" -> clean frames: ");
  Serial.print(cleanFrames);
  Serial.print(", error flag hits: ");
  Serial.println(errorEvents);
}
