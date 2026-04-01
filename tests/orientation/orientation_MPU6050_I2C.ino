/*
  Student Tetrahedron — Orientation Detection (Robust Version)
  Hardware : XOAO MINI ESP32S + MPU6050 via I2C
*/

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

Adafruit_MPU6050 mpu;

// ── Configuration ────────────────────────────────────────────────────────────

#define CALIBRATE false   // Set true to see raw values in Serial Monitor
#define SAMPLES 10        // Number of readings to average
#define DEBOUNCE_COUNT 5  // How many stable readings before switching faces

// Safety gates: Only ignore readings if they are wildly outside gravity
// (e.g., when being shaken or thrown).
// Your 12.62 reading fits comfortably inside 5.0 - 15.0.
#define MIN_MAG 5.0f
#define MAX_MAG 15.0f

struct Vec3 {
  float x, y, z;
};

// ── Calibrated Face Vectors ──────────────────────────────────────────────────
// These are your specific readings. Magnitude doesn't matter anymore,
// just the "direction" they point.
const Vec3 FACE_VECTORS[4] = {
    {0.494f, 0.040f, -12.610f},  // Face 0: Green
    {-7.802f, -4.533f, 0.600f},  // Face 1: Red
    {0.720f, 9.341f, 0.913f},    // Face 2: Orange
    {8.239f, -5.002f, 0.883f}    // Face 3: Blue
};

const char* FACE_NAMES[4] = {
    "I'm good       (GREEN)  -> 0", "I need help    (RED)    -> 1",
    "I need a break (ORANGE) -> 2", "I'm ready      (BLUE)   -> 3"};

// ── Math Helpers ─────────────────────────────────────────────────────────────

float magnitude(Vec3 v) { return sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

Vec3 normalize(Vec3 v) {
  float m = magnitude(v);
  if (m < 0.001f) return {0, 0, 0};
  return {v.x / m, v.y / m, v.z / m};
}

float angleDeg(Vec3 a, Vec3 b) {
  Vec3 na = normalize(a);
  Vec3 nb = normalize(b);
  float dot = na.x * nb.x + na.y * nb.y + na.z * nb.z;
  return degrees(acos(constrain(dot, -1.0f, 1.0f)));
}

// ── Core Logic ───────────────────────────────────────────────────────────────

Vec3 readSmoothed() {
  float sx = 0, sy = 0, sz = 0;
  for (int i = 0; i < SAMPLES; i++) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    sx += a.acceleration.x;
    sy += a.acceleration.y;
    sz += a.acceleration.z;
    delay(5);
  }
  return {sx / SAMPLES, sy / SAMPLES, sz / SAMPLES};
}

int detectFace(Vec3 reading) {
  float mag = magnitude(reading);

  // If the sensor is experiencing heavy motion, return "unstable"
  if (mag < MIN_MAG || mag > MAX_MAG) return -1;

  int bestFace = -1;
  float minAngle = 180.0f;  // Start with max possible angle

  for (int i = 0; i < 4; i++) {
    float angle = angleDeg(reading, FACE_VECTORS[i]);
    if (angle < minAngle) {
      minAngle = angle;
      bestFace = i;
    }
  }

  // Sanity check: If the nearest face is > 50 deg away, it's tilted awkwardly
  if (minAngle > 50.0f) return -1;

  return bestFace;
}

// ── State ────────────────────────────────────────────────────────────────────

int currentFace = -2;
int candidateFace = -1;
int debounceCount = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  if (!mpu.begin()) {
    Serial.println("ERROR: MPU6050 not found!");
    while (1) delay(10);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("Tetrahedron Ready.");
}

void loop() {
  Vec3 reading = readSmoothed();

  if (CALIBRATE) {
    float mag = magnitude(reading);
    Serial.printf("ax=%.3f ay=%.3f az=%.3f | mag=%.3f\n", reading.x, reading.y,
                  reading.z, mag);
    delay(200);
    return;
  }

  int detected = detectFace(reading);

  // Debounce Logic
  if (detected == candidateFace) {
    debounceCount++;
  } else {
    candidateFace = detected;
    debounceCount = 0;
  }

  if (debounceCount >= DEBOUNCE_COUNT && detected != currentFace) {
    currentFace = detected;
    if (currentFace == -1) {
      Serial.println("STATE: Moving / Unstable (255)");
    } else {
      Serial.print("STATE: ");
      Serial.println(FACE_NAMES[currentFace]);
    }
  }
}