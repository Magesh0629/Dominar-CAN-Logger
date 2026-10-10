/*
  Dominar CAN logger - ALL IN ONE (flash once, never reflash)
  ------------------------------------------------------------
  On every power-up it will:
    1. Scan 125/250/500/1000 kbps in LISTEN-ONLY mode (never transmits)
    2. Pick the speed that gives the most clean frames
       (if the bus is silent, it keeps retrying until you turn ignition ON)
    3. Create a NEW log file on the SD card (/canlog_001.csv, _002, ...)
    4. Log every frame + your event markers

  Event markers (so you can correlate bytes to actions later):
    - Type a single character in Serial Monitor (115200 baud) + Enter
        r = throttle/RPM blip   b = front brake     c = rear brake
        t = turn signal on      T = turn signal off  g = gear change
        h = high beam           s = speed change
    - OR press the BOOT button on the ESP32 (logs marker '*')
      (never hold BOOT while powering on / resetting - it enters flash mode)
    - Type '!' to force a re-scan of the bitrate and start a new log file

  Library needed: "arduino-mcp2515" by autowp (Library Manager). SD is built in.

  Arduino IDE note: save this sketch in a folder named
  "dominar_can_logger" so the folder name matches the .ino file name.
*/

#include <SPI.h>
#include <mcp2515.h>
#include <SD.h>

// ---------- Pins ----------
#define CAN_CS_PIN  5
#define SD_CS_PIN   15
#define BUTTON_PIN  0     // onboard BOOT button (active LOW)
#define LED_PIN     2     // onboard LED on most ESP32 devkits

// ---------- Set to match the crystal on YOUR MCP2515 board ----------
#define CRYSTAL MCP_8MHZ  // or MCP_16MHZ

// ---------- Scan settings ----------
struct Candidate {
  CAN_SPEED speed;
  const char* label;
};

Candidate candidates[] = {
  { CAN_125KBPS,  "125 kbps"  },
  { CAN_250KBPS,  "250 kbps"  },
  { CAN_500KBPS,  "500 kbps"  },
  { CAN_1000KBPS, "1000 kbps" }
};
const int NUM_CANDIDATES = sizeof(candidates) / sizeof(candidates[0]);

const unsigned long SCAN_WINDOW_MS    = 2500;  // listen time per speed
const unsigned long RESCAN_DELAY_MS   = 2000;  // wait between failed scans
const unsigned long MIN_CLEAN_FRAMES  = 5;     // minimum to accept a speed
const unsigned long FLUSH_INTERVAL_MS = 200;

// ---------- Globals ----------
MCP2515 mcp2515(CAN_CS_PIN);
struct can_frame canMsg;
File logFile;

unsigned long lastFlush = 0;
unsigned long lastStats = 0;
unsigned long totalFrames = 0;
unsigned long framesThisSecond = 0;
unsigned long lastFrameTime = 0;

// forward declarations
void logFrame(const struct can_frame &f);
void logEvent(char marker);

bool lastButton = HIGH;
unsigned long lastButtonTime = 0;

// =====================================================================
//  Scan all candidate speeds, return index of best one (or -1)
// =====================================================================
int scanBitrates() {
  int bestIdx = -1;
  unsigned long bestClean = 0;

  for (int i = 0; i < NUM_CANDIDATES; i++) {
    mcp2515.reset();
    mcp2515.setBitrate(candidates[i].speed, CRYSTAL);
    mcp2515.setListenOnlyMode();

    unsigned long start = millis();
    unsigned long clean = 0;
    unsigned long errors = 0;
    struct can_frame f;

    while (millis() - start < SCAN_WINDOW_MS) {
      if (mcp2515.readMessage(&f) == MCP2515::ERROR_OK) {
        clean++;
      }
      if (mcp2515.getErrorFlags() != 0) {
        errors++;
        mcp2515.clearRXnOVRFlags();
      }
      digitalWrite(LED_PIN, (millis() / 100) % 2); // fast blink = scanning
    }

    Serial.print("  ");
    Serial.print(candidates[i].label);
    Serial.print(" -> clean frames: ");
    Serial.print(clean);
    Serial.print(", error hits: ");
    Serial.println(errors);

    if (clean >= MIN_CLEAN_FRAMES && clean > bestClean) {
      bestClean = clean;
      bestIdx = i;
    }
  }
  return bestIdx;
}

// =====================================================================
//  Open a new numbered log file + a small info file with the bitrate
// =====================================================================
void openNewLogFile(int bitrateIdx) {
  char name[32];
  int n;
  for (n = 1; n < 1000; n++) {
    snprintf(name, sizeof(name), "/canlog_%03d.csv", n);
    if (!SD.exists(name)) break;
  }

  logFile = SD.open(name, FILE_WRITE);
  if (!logFile) {
    Serial.println("ERROR: could not open log file on SD.");
    while (1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(100);
    }
  }
  logFile.println("timestamp_ms,id,dlc,d0,d1,d2,d3,d4,d5,d6,d7");
  logFile.flush();

  // info file: same name + ".txt"
  char infoName[40];
  snprintf(infoName, sizeof(infoName), "/canlog_%03d_info.txt", n);
  File info = SD.open(infoName, FILE_WRITE);
  if (info) {
    info.print("Bitrate: ");
    info.println(candidates[bitrateIdx].label);
    info.close();
  }

  Serial.print("Logging to ");
  Serial.println(name);
}

// =====================================================================
//  Full session start: scan (retry until bus found) -> open log
// =====================================================================
void startSession() {
  if (logFile) {
    logFile.flush();
    logFile.close();
  }

  int idx = -1;
  while (idx < 0) {
    Serial.println("Scanning CAN bitrate (listen-only)...");
    idx = scanBitrates();
    if (idx < 0) {
      Serial.println("No bus activity found. Turn bike ignition ON and check CAN wiring. Retrying...");
      delay(RESCAN_DELAY_MS);
    }
  }

  Serial.print("Detected bus speed: ");
  Serial.println(candidates[idx].label);

  mcp2515.reset();
  mcp2515.setBitrate(candidates[idx].speed, CRYSTAL);
  mcp2515.setListenOnlyMode();

  openNewLogFile(idx);
  totalFrames = 0;
  framesThisSecond = 0;
  Serial.println("Ready. Type a marker char + Enter, press BOOT button, or '!' to rescan.");
}

// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  SPI.begin(18, 19, 23, CAN_CS_PIN); // SCK, MISO, MOSI, SS

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD init FAILED. Check wiring and FAT32 format.");
    while (1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(100);
    }
  }
  Serial.println("SD OK.");

  startSession();
}

void loop() {
  // 1. Log incoming CAN frames
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    logFrame(canMsg);
    totalFrames++;
    framesThisSecond++;
    lastFrameTime = millis();
  }

  // 2. Serial commands / markers
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') continue;
    if (c == '!') {
      Serial.println("Re-scan requested...");
      startSession();
    } else {
      logEvent(c);
      Serial.print("Marker logged: ");
      Serial.println(c);
    }
  }

  // 3. BOOT button marker (debounced)
  bool btn = digitalRead(BUTTON_PIN);
  if (btn == LOW && lastButton == HIGH && millis() - lastButtonTime > 250) {
    logEvent('*');
    Serial.println("Marker logged: * (button)");
    lastButtonTime = millis();
  }
  lastButton = btn;

  // 4. LED on = frames seen in the last 500 ms
  digitalWrite(LED_PIN, (millis() - lastFrameTime < 500) ? HIGH : LOW);

  // 5. Periodic flush so data survives power loss
  if (millis() - lastFlush > FLUSH_INTERVAL_MS) {
    logFile.flush();
    lastFlush = millis();
  }

  // 6. Once-per-second status line
  if (millis() - lastStats > 1000) {
    Serial.print("Frames/sec: ");
    Serial.print(framesThisSecond);
    Serial.print("   Total: ");
    Serial.println(totalFrames);
    framesThisSecond = 0;
    lastStats = millis();
  }
}

// =====================================================================
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
  logFile.println(",,,,,,,,");
}
