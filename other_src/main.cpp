#include <Arduino.h>
#include <M5Core2.h>

void setup() {
  // Initialize the device so it is responsive
  M5.begin();

  // Do some initial stuff
  Serial.println("Hello from M5Stack Core 2!");
  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.fillScreen(CYAN);
  M5.Lcd.println("Hello World!");
}

void loop() {
  // Put your main code here, to run repeatedly:
  Serial.println("Hello again!");
  delay(1000);
}