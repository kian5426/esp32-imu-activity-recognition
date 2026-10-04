/*
 * STAGE 2: labeled data logger (no on-board classification).
 * Streams: time_ms,label,accX,accY,accZ,gyroX,gyroY,gyroZ
 * Type a label over serial, touch the D4 wire to start/stop recording.
 * Used with scripts/serial_logger.py to record the training trials.
 */
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

Adafruit_MPU6050 mpu;

const int buttonPin = 4; // D4 connected to button, other side to GND (using internal pull-up)

String activityLabel = "idle"; // default label before you set one
bool recording = false;
unsigned long startTime;

// Debounce for button
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  pinMode(buttonPin, INPUT_PULLUP);

  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  delay(100);

  Serial.println("Ready.");
  Serial.println("Type a label (e.g. walking, running, upstairs, downstairs) and press Enter to set it.");
  Serial.println("Press the button to start/stop recording.");
}

void loop() {
  // --- Handle label input from Serial Monitor ---
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      activityLabel = input;
      Serial.print("Label set to: ");
      Serial.println(activityLabel);
    }
  }

  // --- Handle button press (toggle recording) with debounce ---
  bool reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    static bool buttonHandled = false;
    if (reading == LOW && !buttonHandled) {
      recording = !recording;
      buttonHandled = true;
      if (recording) {
        startTime = millis();
        Serial.println("time_ms,label,accX,accY,accZ,gyroX,gyroY,gyroZ");
      } else {
        Serial.println("-- Recording stopped --");
      }
    } else if (reading == HIGH) {
      buttonHandled = false;
    }
  }
  lastButtonState = reading;

  // --- Record data if active ---
  if (recording) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    unsigned long t = millis() - startTime;

    Serial.print(t);
    Serial.print(",");
    Serial.print(activityLabel);
    Serial.print(",");
    Serial.print(a.acceleration.x, 4);
    Serial.print(",");
    Serial.print(a.acceleration.y, 4);
    Serial.print(",");
    Serial.print(a.acceleration.z, 4);
    Serial.print(",");
    Serial.print(g.gyro.x, 4);
    Serial.print(",");
    Serial.print(g.gyro.y, 4);
    Serial.print(",");
    Serial.println(g.gyro.z, 4);

    delay(50); // ~20 Hz sampling rate
  }
}
