/*
 * FINAL FIRMWARE: labeled data logger + real-time activity classifier.
 * ESP32 + MPU6050 (I2C: SDA = D21, SCL = D22), start/stop touch-wire on D4.
 *
 * Streams one CSV row per sample (~20 Hz) while recording:
 *   time_ms,label,accX,accY,accZ,gyroX,gyroY,gyroZ,detected
 * `label` is the ground truth typed over serial, `detected` is the rule-based
 * prediction over a 40-sample (~2 s) window ("..." until the first window fills).
 *
 * Libraries: Adafruit MPU6050, Adafruit Unified Sensor. Board: ESP32 Dev Module.
 * This is the sketch that produced data/raw/accuracy_run.csv.
 */
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

Adafruit_MPU6050 mpu;

const int buttonPin = 4; // D4 connected to button, other side to GND (using internal pull-up)

String activityLabel = "idle"; // default label before you set one
bool recording = false;
unsigned long startTime;

// Debounce for button
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// ---- Sliding window for real-time detection ----
const int WINDOW_SIZE = 40; // ~2 seconds at 20 Hz
float accY_buf[WINDOW_SIZE];
float accZ_buf[WINDOW_SIZE];
float gyroY_buf[WINDOW_SIZE];
int bufIndex = 0;
bool bufFull = false;

// ---- Thresholds derived from collected data ----
const float RUNNING_ACCY_STD_THRESHOLD = 4.0;
const float DOWNSTAIRS_ACCZ_STD_MAX    = 0.7;
const float UPSTAIRS_ACCZ_STD_MAX      = 1.1;
const float DOWNSTAIRS_GYROY_PTP_MAX   = 1.8;

float computeMean(float *buf, int n) {
  float sum = 0;
  for (int i = 0; i < n; i++) sum += buf[i];
  return sum / n;
}

float computeStd(float *buf, int n, float mean) {
  float sumSq = 0;
  for (int i = 0; i < n; i++) {
    float diff = buf[i] - mean;
    sumSq += diff * diff;
  }
  return sqrt(sumSq / n);
}

float computePeakToPeak(float *buf, int n) {
  float maxVal = buf[0];
  float minVal = buf[0];
  for (int i = 1; i < n; i++) {
    if (buf[i] > maxVal) maxVal = buf[i];
    if (buf[i] < minVal) minVal = buf[i];
  }
  return maxVal - minVal;
}

String classifyActivity(float accY_std, float accZ_std, float gyroY_ptp) {
  if (accY_std > RUNNING_ACCY_STD_THRESHOLD) {
    return "Running";
  } else if (accZ_std < DOWNSTAIRS_ACCZ_STD_MAX && gyroY_ptp < DOWNSTAIRS_GYROY_PTP_MAX) {
    return "Down-stairs";
  } else if (accZ_std < UPSTAIRS_ACCZ_STD_MAX) {
    return "Up-stairs";
  } else {
    return "Walking";
  }
}

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
        bufIndex = 0;
        bufFull = false;
        Serial.println("time_ms,label,accX,accY,accZ,gyroX,gyroY,gyroZ,detected");
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

    // --- Update sliding window ---
    accY_buf[bufIndex] = a.acceleration.y;
    accZ_buf[bufIndex] = a.acceleration.z;
    gyroY_buf[bufIndex] = g.gyro.y;

    bufIndex++;

    if (bufIndex >= WINDOW_SIZE) {
      bufIndex = 0;
      bufFull = true;
    }

    // --- Classify using the window (once full) ---
    String detected = "...";

    if (bufFull) {
      float accY_mean = computeMean(accY_buf, WINDOW_SIZE);
      float accY_std  = computeStd(accY_buf, WINDOW_SIZE, accY_mean);

      float accZ_mean = computeMean(accZ_buf, WINDOW_SIZE);
      float accZ_std  = computeStd(accZ_buf, WINDOW_SIZE, accZ_mean);

      float gyroY_ptp = computePeakToPeak(gyroY_buf, WINDOW_SIZE);

      detected = classifyActivity(accY_std, accZ_std, gyroY_ptp);
    }

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
    Serial.print(g.gyro.z, 4);
    Serial.print(",");
    Serial.println(detected);

    delay(50); // ~20 Hz sampling rate
  }
}
