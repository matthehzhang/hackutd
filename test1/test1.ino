#define out1 9

void setup() {
  Serial.begin(115200);
  pinMode(out1, OUTPUT);
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'L') {
      digitalWrite(out1, HIGH);
    } else if (c == 'l') {
      digitalWrite(out1, LOW);
    }
  }

  Serial.println("Hello from ESP32-S3!");
  delay(10);
}
