# ESP32 + MCP2515 + SD Card — Dominar CAN Logger: Wiring

## Parts
- ESP32 dev board (any WROOM/WROVER)
- MCP2515 CAN module **with onboard transceiver** (MCP2551/TJA1050) — check the crystal on the board, it's usually 8MHz or 16MHz, printed on the silver can
- SPI SD card module (3.3V logic, or one with an onboard regulator)
- MicroSD card, FAT32, 32GB or smaller
- Jumper wires, USB power bank (do NOT power from the bike battery directly)

## Shared SPI bus (MCP2515 + SD card on same bus, separate CS)

| Signal | ESP32 GPIO | MCP2515 pin | SD module pin |
|---|---|---|---|
| SCK  | 18 | SCK | SCK |
| MISO | 19 | SO  | MISO |
| MOSI | 23 | SI  | MOSI |
| CS (CAN) | 5  | CS  | — |
| CS (SD)  | 15 | —   | CS |
| INT (CAN) | 4 | INT | — (not used yet, reserved for interrupt-driven version) |
| 3.3V | 3V3 | VCC | VCC |
| GND  | GND | GND | GND |

Each SPI device needs its **own CS pin** — that's the only thing that changes per device on a shared bus.

## MCP2515 → Bike CAN bus

- MCP2515 module's `CANH`/`CANL` screw terminals → tap into the bike's CAN_H / CAN_L wires (commonly at the dash connector or ECU harness — check a wiring diagram or probe with a multimeter for a twisted pair).
- Share ground between your logger and the bike (single point, avoid ground loops).
- Do **not** connect the bus in normal/active mode until you've confirmed correct wiring — the code below uses **listen-only mode**, which never transmits or ACKs, so it can't disrupt the bike's bus even if something's wrong.

## Safety notes
- Do this with the bike parked, not while riding.
- Don't power your ESP32 from the bike's 12V rail without a proper buck converter — use USB power for bench work.
- Listen-only mode is intentional and important: it protects the live bus from a misconfigured logger.
