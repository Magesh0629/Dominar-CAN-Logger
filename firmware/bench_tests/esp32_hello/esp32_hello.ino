/*
  Bench test 1: is the ESP32 alive and can I upload to it?
  Wiring needed: NONE (USB cable only).
  PASS: onboard LED blinks and Serial Monitor (115200) prints a line every second.
*/
#define LED_PIN 2

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("ESP32 alive - LED on");
  delay(500);
  digitalWrite(LED_PIN, LOW);
  delay(500);
}
