/*
  Student Tetrahedron — Orientation test
  Hardware : Arduino Nano 33 IoT
             Onboard LSM6DS3 IMU
             Onboard NINA-W102 BLE
  Libraries: Arduino_LSM6DS3, ArduinoBLE

  BLE face values:
    0x00 = I'm good       (GREEN)
    0x01 = I need help    (RED)
    0x02 = I need a break (ORANGE)
    0x03 = I'm ready      (BLUE)
    0xFF = rotating / unstable
*/

#include <ArduinoBLE.h>
#include <Arduino_LSM6DS3.h>

// ── Configuration
// ─────────────────────────────────────────────────────────────

// Set CALIBRATE on true to get the values of the postion
#define CALIBRATE false

#define SAMPLES 8
#define MIN_MAG 0.85f
#define MAX_MAG 1.15f
#define MATCH_THRESHOLD_DEG 15.0f
#define DEBOUNCE_COUNT 4

// ── BLE
// ───────────────────────────────────────────────────────────────────────

BLEService tetraService("95ff7bf8-aa6f-4671-82d9-22a8931c5387");
BLEByteCharacteristic faceCharacteristic("95ff7bf9-aa6f-4671-82d9-22a8931c5387",
                                         BLERead | BLENotify);

// ── Calibrated face vectors
// ───────────────────────────────────────────────────

struct Vec3 {
  float x, y, z;
};

const Vec3 FACE_VECTORS[4] = {
    {0.000f, 0.000f, 1.000f},     // Face 0 → I'm good       (GREEN)
    {0.943f, 0.000f, -0.333f},    // Face 1 → I need help    (RED)
    {-0.471f, -0.816f, -0.333f},  // Face 2 → I need a break (ORANGE)
    {-0.471f, 0.816f, -0.333f},   // Face 3 → I'm ready      (BLUE)
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
  int count = 0;
  while (count < SAMPLES) {
    BLE.poll();  // keep BLE alive during sampling
    float x, y, z;
    if (IMU.accelerationAvailable()) {
      IMU.readAcceleration(x, y, z);
      sx += x;
      sy += y;
      sz += z;
      count++;
    }
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

  if (!IMU.begin()) {
    Serial.println("ERROR: LSM6DS3 not found");
  }

  BLE.begin();
  BLE.setLocalName("Tetrahedron");
  BLE.setAdvertisedService(tetraService);
  tetraService.addCharacteristic(faceCharacteristic);
  BLE.addService(tetraService);
  faceCharacteristic.writeValue(255);
  BLE.advertise();

  Serial.println("Student Tetrahedron ready");
  Serial.println("BLE advertising as: Tetrahedron");
}

// ── Loop
// ──────────────────────────────────────────────────────────────────────

void loop() {
  Vec3 reading = readSmoothed();  // BLE.poll() called inside here

  // ── Calibration mode ──────────────────────────────────────────────────────
  if (CALIBRATE) {
    static float sx = 0, sy = 0, sz = 0;
    static int n = 0;
    sx += reading.x;
    sy += reading.y;
    sz += reading.z;
    n++;
    if (n >= 20) {
      Serial.print("ax=");
      Serial.print(sx / n, 3);
      Serial.print(" ay=");
      Serial.print(sy / n, 3);
      Serial.print(" az=");
      Serial.print(sz / n, 3);
      Serial.println();
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

    uint8_t bleValue = (currentFace == -1) ? 255 : (uint8_t)currentFace;
    faceCharacteristic.writeValue(bleValue);

    Serial.print("Face: ");
    switch (currentFace) {
      case 0:
        Serial.println("I'm good       (GREEN)  → 0x00");
        break;
      case 1:
        Serial.println("I need help    (RED)    → 0x01");
        break;
      case 2:
        Serial.println("I need a break (ORANGE) → 0x02");
        break;
      case 3:
        Serial.println("I'm ready      (BLUE)   → 0x03");
        break;
      default:
        Serial.println("rotating / unstable     → 0xFF");
        break;
    }
  }
}
