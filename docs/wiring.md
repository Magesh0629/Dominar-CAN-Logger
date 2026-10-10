# Wiring

## Bike diagnostic connector (6-pin)

| Pin | Function | Connect to |
|---|---|---|
| 1 | Ground | Common ground (ESP32 / MCP2515 / SD GND) |
| 2 | CAN-H | MCP2515 `CANH` |
| 3 | OEM-specific diagnostic line | **Leave unconnected** |
| 4 | K-Line | **Leave unconnected** (different protocol, not CAN) |
| 5 | CAN-L | MCP2515 `CANL` |
| 6 | Battery +12V | Only for the permanent install (fuse, diode, buck converter). **Not used while reverse-engineering.** |

Confirm on your own bike with a multimeter before trusting this table (README, step 6).

## Voltage levels: read this before wiring the MCP2515

The common blue/red **MCP2515 + TJA1050** module needs **5V** for its CAN transceiver, and its `SO` (MISO) output is then **5V logic**. ESP32 GPIOs are **not 5V tolerant**.

Choose one:

| Option | How | Notes |
|---|---|---|
| **A (recommended)** | 5V MCP2515 module, powered from the ESP32 `5V/VIN` pin, with a **4-channel bidirectional level shifter** on SCK, MOSI (SI), MISO (SO), CS (and INT if used) | Correct by the datasheet. ESP32 side of the shifter = 3V3, module side = 5V |
| **B (minimum)** | 5V module, with a resistor divider (e.g. 1k over 2k) on `SO` to the ESP32 MISO pin | Many people run SCK/SI/CS at 3.3V directly and it works, but it is marginal (MCP2515 needs about 3.5V for a logic high at 5V supply). If it is flaky, go to option A |
| **C** | A 3.3V-native module (MCP2515 with an SN65HVD230 3.3V transceiver) powered from `3V3` | No level shifting needed |

Because MISO is shared with the SD card, put the shifted/divided MISO on the ESP32 side of the node so both devices are safe.

**SD card module:** most blue modules have an onboard regulator and level shifter, so power them from `5V`. Bare breakouts without those need `3V3`. Check what is printed on yours.

## Pin map

### Power (USB or bench phase)

| From | To |
|---|---|
| USB | ESP32 |
| ESP32 `5V/VIN` | MCP2515 `VCC` (option A/B) and SD module `VCC` (if regulated module) |
| ESP32 `3V3` | MCP2515 `VCC` (option C only), level shifter LV side |
| ESP32 `GND` | MCP2515 `GND`, SD `GND`, connector pin 1 |

### SPI bus (shared)

| ESP32 | MCP2515 | SD module |
|---|---|---|
| GPIO18 | SCK | SCK |
| GPIO19 | SO | MISO |
| GPIO23 | SI | MOSI |
| GPIO5 | CS | - |
| GPIO15 | - | CS |
| GPIO4 | INT (optional, unused by current firmware) | - |

Each SPI device gets its own CS pin. That is the only thing that differs on the shared bus.

GPIO5 and GPIO15 are ESP32 boot-strapping pins. They work fine as chip-selects here, but if the ESP32 ever refuses to boot, disconnect the CS wires and retry.

### CAN bus

| MCP2515 | Bike connector |
|---|---|
| `CANH` | pin 2 |
| `CANL` | pin 5 |
| `GND` | pin 1 |

## Remove the MCP2515's termination resistor

Most MCP2515 modules have a **120 ohm termination jumper** (often labelled `J1`) across CANH/CANL. The bike's bus is already terminated at its own two ends. Adding a third terminator loads the bus and distorts the signal. **Remove or open that jumper** when sniffing the bike. (Leave it as is for the bench loopback test, which does not touch a bus.)

This also means the "about 60 ohm" resistance check must be done with the logger **disconnected**.

## Permanent install power chain (only after everything works on USB)

```
Connector pin 6 (+12V) -> 1A fuse -> diode (1N5400, band toward buck) -> Buck IN+
Connector pin 1 (GND)  ------------------------------------------------> Buck IN-
Buck OUT+ (5V, set and check with a multimeter BEFORE connecting) -> ESP32 5V/VIN
Buck OUT-                                                         -> ESP32 GND
```

- Use a buck converter rated for at least 6-40V input, 5V output, 1A or more.
- Pin 6 may be always-on battery or ignition-switched. Measure it with the ignition off and on. Always-on will slowly drain the battery if you forget the logger.
- Never run USB (laptop) and the buck 5V into the ESP32 at the same time without a diode-OR arrangement. Use one power source at a time.
