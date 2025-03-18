void setup() {
  Serial.begin(115200);  // Match STM32 baud rate
}

void loop() {
  Serial.println("Hello");  // Send "Hello" with newline
  delay(1000);  // Wait 1 second
}