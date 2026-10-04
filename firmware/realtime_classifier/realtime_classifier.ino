/*
 * STAGE 3: real-time classifier only (no logging, no button).
 * Prints accY_std, accZ_std, gyroY_ptp and the detected activity every sample
 * once the 40-sample window is full. Merged with the logger into activity_detector.
 */
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

Adafruit_MPU6050 mpu;

// ---- Sliding window settings ----
const int WINDOW_SIZE = 40;   // ~2 seconds of data at 20 Hz sampling rate
float accY_buf[WINDOW_SIZE];
float accZ_buf[WINDOW_SIZE];
float gyroY_buf[WINDOW_SIZE];
int bufIndex = 0;
bool bufFull = false;

// ---- Thresholds derived from collected data (adjust if needed after more testing) ----
const float RUNNING_ACCY_STD_THRESHOLD   = 4.0;   // running has much higher accY_std than others
const float DOWNSTAIRS_ACCZ_STD_MAX      = 0.7;   // downstairs has the lowest accZ_std
const float UPSTAIRS_ACCZ_STD_MAX        = 1.1;   // upstairs is between downstairs and walking
const float DOWNSTAIRS_GYROY_PTP_MAX     = 1.8;   // downstairs also has the lowest gyroY peak-to-peak

unsigned long lastSampleTime = 0;
const unsigned long SAMPLE_INTERVAL_MS = 50; // 20 Hz

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
  Serial.println("Real-time activity classifier (with gyro) started.");
  Serial.println("Collecting initial window...");
}

void loop() {
  unsigned long now = millis();
  if (now - lastSampleTime < SAMPLE_INTERVAL_MS) return;
  lastSampleTime = now;

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Store into circular buffer
  accY_buf[bufIndex] = a.acceleration.y;
  accZ_buf[bufIndex] = a.acceleration.z;
  gyroY_buf[bufIndex] = g.gyro.y;
  bufIndex++;

  if (bufIndex >= WINDOW_SIZE) {
    bufIndex = 0;
    bufFull = true;
  }

  // Only classify once we have a full window of data
  if (bufFull) {
    float accY_mean = computeMean(accY_buf, WINDOW_SIZE);
    float accY_std  = computeStd(accY_buf, WINDOW_SIZE, accY_mean);

    float accZ_mean = computeMean(accZ_buf, WINDOW_SIZE);
    float accZ_std  = computeStd(accZ_buf, WINDOW_SIZE, accZ_mean);

    float gyroY_ptp = computePeakToPeak(gyroY_buf, WINDOW_SIZE);

    String activity = classifyActivity(accY_std, accZ_std, gyroY_ptp);

    Serial.print("accY_std=");
    Serial.print(accY_std, 3);
    Serial.print("  accZ_std=");
    Serial.print(accZ_std, 3);
    Serial.print("  gyroY_ptp=");
    Serial.print(gyroY_ptp, 3);
    Serial.print("  --> Detected activity: ");
    Serial.println(activity);
  }
}
