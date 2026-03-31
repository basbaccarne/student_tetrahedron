/*
  Student Tetrahedron — Orientation Detection
  Hardware : XOAO MINI ESP32S + MPU6050 via I2C
  Libraries: Adafruit_MPU6050, Adafruit_Sensor, Wire

  Note: MPU6050 via Adafruit library returns acceleration in m/s²
        so gravity = ~9.81 instead of ~1.0

  BLE face values:
    0 = I'm good       (GREEN)
    1 = I need help    (RED)
    2 = I need a break (ORANGE)
    3 = I'm ready      (BLUE)
  255 = rotating / unstable
*/

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

Adafruit_MPU6050 mpu;

// ── Configuration
// ─────────────────────────────────────────────────────────────

#define CALIBRATE false  // set false once vectors are filled in

#define SAMPLES 8
#define MATCH_THRESHOLD_DEG 15.0f
#define DEBOUNCE_COUNT 4

// Gravity in m/s² — accept readings between 8.5 and 11.0 as valid
#define MIN_MAG 8.5f
#define MAX_MAG 11.0f

// ── Calibrated face vectors
// ─────────────────────────────────────────────────── Run with CALIBRATE true,
// place each face flat, note the stable readings. Paste them here, then set
// CALIBRATE false. Values are in m/s² (gravity ≈ 9.81)

struct Vec3 {
  float x, y, z;
};

const Vec3 FACE_VECTORS[4] = {
    {0.660f, 0.087f,
     7.796f},  // Face 0 → I'm good       — REPLACE with calibrated value
    {9.24f, 0.00f,
     -3.27f},  // Face 1 → I need help    — REPLACE with calibrated value
    {-4.62f, 8.00f,
     -3.27f},  // Face 2 → I need a break — REPLACE with calibrated value
    {-4.62f, -8.00f,
     -3.27f},  // Face 3 → I'm ready      — REPLACE with calibrated value
};

const char* FACE_NAMES[4] = {
    "I'm good       (GREEN)  → 0",
    "I need help    (RED)    → 1",
    "I need a break (ORANGE) → 2",
    "I'm ready      (BLUE)   → 3",
};

// ── Helpers
// ───────────────────────────────────────────────────────────────────

float magnitude(Vec3 v) { return sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

Vec3 normalize(Vec3 v) {
  float m = magnitude(v);
  return {v.x / m, v.y / m, v.z / m};
}

float angleDeg(Vec3 a, Vec3 b) {
  float dot = constrain(a.x * b.x + a.y * b.y + a.z * b.z, -1.0f, 1.0f);
  return degrees(acos(dot));
}

Vec3 readSmoothed() {
  float sx = 0, sy = 0, sz = 0;
  for (int i = 0; i < SAMPLES; i++) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    sx += a.acceleration.x;
    sy += a.acceleration.y;
    sz += a.acceleration.z;
    delay(10);
  }
  return {sx / SAMPLES, sy / SAMPLES, sz / SAMPLES};
}

int detectFace(Vec3 reading) {
  float mag = magnitude(reading);
  if (mag < MIN_MAG || mag > MAX_MAG) return -1;
  Vec3 norm = normalize(reading);
  int best = -1;
  float bestAngle = MATCH_THRESHOLD_DEG;
  for (int i = 0; i < 4; i++) {
    float angle = angleDeg(norm, normalize(FACE_VECTORS[i]));
    if (angle < bestAngle) {
      bestAngle = angle;
      best = i;
    }
  }
  return best;
}

// ── State
// ─────────────────────────────────────────────────────────────────────

int currentFace = -2;
int candidateFace = -1;
int debounceCount = 0;

// ── Setup
// ─────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  if (!mpu.begin()) {
    Serial.println("ERROR: MPU6050 not found");
    while (true) delay(10);
  }

  mpu.setAccelerometerRange(
      MPU6050_RANGE_2_G);  // ±2g is plenty for orientation
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("Student Tetrahedron — MPU6050");
  if (CALIBRATE) {
    Serial.println(
        "CALIBRATION MODE — place each face flat, wait for stable reading");
    Serial.println("---");
  }
}

// ── Loop
// ──────────────────────────────────────────────────────────────────────

void loop() {
  Vec3 reading = readSmoothed();

  // ── Calibration mode ──────────────────────────────────────────────────────
  if (CALIBRATE) {
    static float sx = 0, sy = 0, sz = 0;
    static int n = 0;
    sx += reading.x;
    sy += reading.y;
    sz += reading.z;
    n++;
    if (n >= 10) {
      float mag = magnitude({sx / n, sy / n, sz / n});
      Serial.print("ax=");
      Serial.print(sx / n, 3);
      Serial.print(" ay=");
      Serial.print(sy / n, 3);
      Serial.print(" az=");
      Serial.print(sz / n, 3);
      Serial.print("  |mag=");
      Serial.println(mag, 3);
      sx = 0;
      sy = 0;
      sz = 0;
      n = 0;
    }
    return;
  }

  // ── Detection mode ────────────────────────────────────────────────────────
  int detected = detectFace(reading);

  if (detected == candidateFace) {
    debounceCount++;
  } else {
    candidateFace = detected;
    debounceCount = 1;
  }

  if (debounceCount >= DEBOUNCE_COUNT && detected != currentFace) {
    currentFace = detected;
    Serial.print("Face: ");
    if (currentFace == -1)
      Serial.println("(rotating / unstable)");
    else
      Serial.println(FACE_NAMES[currentFace]);
  }
}
