# Verifying with a 2-channel PicoScope

Optional but useful. It proves the electrical signals are healthy before you trust any logged data. Do the stages in order. Each one validates the layer below it.

## Ground rules

- A USB-powered PicoScope's ground is tied to your PC's ground. Clip the probe ground **only to the ESP32 / logger GND**, never to CAN-H, CAN-L, or any signal.
- Use DC coupling on both channels.
- Run the laptop on battery, not the charger, when the scope and the bike are both connected. This avoids ground-loop surprises.

## Stage 1 - Power rails (no bike connected)

1. Channel A: ESP32 `5V/VIN`. Channel B: ESP32 `3V3`. Range 5V or 10V.
2. Timebase 10 ms/div, then 1 ms/div. Add peak-to-peak measurements on both channels.
3. Pass: steady rails, ripple under about 100 mV. Wiggle wires; any dropout is a bad connection.

## Stage 2 - SPI bus (no bike connected)

Flash a bench test first (`sd_card_test` or `mcp2515_spi_test`) so there is SPI traffic.

1. Channel A: SCK (GPIO18). Channel B: the CS of the device under test (GPIO5 or GPIO15).
2. Range 5V. Timebase 10-50 us/div.
3. Trigger: source B (CS), falling edge, about 1.5V.
4. Pass: clock bursts appear only while CS is low; clean edges, full 0-3.3V swing on the ESP32 side. Rounded edges usually mean long or messy wires.
5. Repeat with Channel A on MOSI (GPIO23) if you want to check the data line.

## Stage 3 - CAN bus (logger connected to the bike, ignition ON)

1. Channel A: CAN-H (connector pin 2). Channel B: CAN-L (pin 5). Range 5V.
2. Timebase: at 500 kbps one bit is 2 us. Start at 20 us/div to see whole frames, then 1-2 us/div for individual bits.
3. Trigger: source B (CAN-L), falling edge, about 2V.
4. Math channel A - B for the differential signal.
5. Expected (typical high-speed CAN):
   - Idle (recessive): both about 2.5V, differential about 0V.
   - Dominant bit: CAN-H rises toward about 3.5V, CAN-L falls toward about 1.5V, mirror images. Differential about 2V.
6. Bad signs: only one line moves, both move the same direction (swapped or damaged), heavy ringing (termination problem: check the MCP2515 termination jumper is removed).
7. Read the bit width on the scope. It should match the bitrate the firmware reports (500 kbps = 2 us, 250 kbps = 4 us, 125 kbps = 8 us).

## Stage 4 - Correlate with the log

With CAN-H/CAN-L on the scope and the logger running, use single-shot trigger and press a marker while performing an action. Confirm the frames you see on the scope correspond in time and rhythm to the frames in the CSV around that marker. This shows your software decodes the real signal and not corrupted data.
