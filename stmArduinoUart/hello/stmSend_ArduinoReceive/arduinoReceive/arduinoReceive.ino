void setup() {
  Serial.begin(115200);  // Match STM32 baud rate
}

void loop() {
  if (Serial.available() > 0) {  // If data is received
    String message = Serial.readStringUntil('\n');  // Read until newline
    Serial.println(message);  // Print to Serial Monitor
  }
}