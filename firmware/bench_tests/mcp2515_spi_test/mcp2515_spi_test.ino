/*
  Bench test 3: MCP2515 SPI wiring, using LOOPBACK mode.
  Wiring needed: MCP2515 only (SCK=18, MISO=19, MOSI=23, CS=5, power, GND).
  NO bike / CAN bus connection needed - the chip sends a frame to itself.

  PASS: prints "PASS: loopback frame sent and received correctly".
  FAIL: check SPI wires, CS pin, power (see README about 5V vs 3.3V modules).

  Note: loopback does NOT test the CRYSTAL setting or the CAN transceiver/bus
  wiring - those are verified later when the logger detects real bus traffic.
*/
#include <SPI.h>
#include <mcp2515.h>

#define CAN_CS_PIN 5
#define CRYSTAL MCP_8MHZ   // or MCP_16MHZ - match your board's crystal

MCP2515 mcp2515(CAN_CS_PIN);

void setup() {
  Serial.begin(115200);
  delay(1000);
  SPI.begin(18, 19, 23, CAN_CS_PIN);

  if (mcp2515.reset() != MCP2515::ERROR_OK) {
    Serial.println("FAIL: reset() failed");
    return;
  }
  if (mcp2515.setBitrate(CAN_500KBPS, CRYSTAL) != MCP2515::ERROR_OK) {
    Serial.println("FAIL: setBitrate() failed - chip not responding over SPI");
    return;
  }
  if (mcp2515.setLoopbackMode() != MCP2515::ERROR_OK) {
    Serial.println("FAIL: could not enter loopback mode");
    return;
  }

  struct can_frame tx;
  tx.can_id = 0x123;
  tx.can_dlc = 8;
  for (int i = 0; i < 8; i++) tx.data[i] = 0xA0 + i;

  if (mcp2515.sendMessage(&tx) != MCP2515::ERROR_OK) {
    Serial.println("FAIL: sendMessage() failed");
    return;
  }
  delay(20);

  struct can_frame rx;
  if (mcp2515.readMessage(&rx) != MCP2515::ERROR_OK) {
    Serial.println("FAIL: nothing received in loopback");
    return;
  }

  bool ok = (rx.can_id == 0x123) && (rx.can_dlc == 8);
  for (int i = 0; i < 8 && ok; i++) ok = (rx.data[i] == 0xA0 + i);

  Serial.println(ok ? "PASS: loopback frame sent and received correctly"
                    : "FAIL: received frame does not match");
}

void loop() {}
