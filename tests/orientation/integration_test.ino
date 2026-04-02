/*
  Student Tetrahedron — Orientation + NeoPixel Feedback
  Hardware : XIAO MINI ESP32S + MPU6050 + 1 x WS2812B NeoPixel
  changes color of NeoPixel based on orientation:
    0 = I'm good       (GREEN)
    1 = I need help    (RED)
    2 = I need a break (ORANGE)
    3 = I'm ready      (BLUE)
*/

#include <Adafruit_MPU6050.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

// ── Configuration ────────────────────────────────────────────────────────────

#define LED_PIN D6
#define NUM_LEDS 1
#define SAMPLES 10
#define DEBOUNCE_COUNT 5

// Magnitude safety gates
#define MIN_MAG 5.0f
#define MAX_MAG 15.0f

Adafruit_MPU6050 mpu;
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

struct Vec3 {
  float x, y, z;
};

// ── Calibrated Face Vectors ──────────────────────────────────────────────────
const Vec3 FACE_VECTORS[4] = {
    {0.494f, 0.040f, -12.610f},  // Face 0: Good (GREEN)
    {-7.802f, -4.533f, 0.600f},  // Face 1: Help (RED)
    {0.720f, 9.341f, 0.913f},    // Face 2: Break (ORANGE)
    {8.239f, -5.002f, 0.883f}    // Face 3: Ready (BLUE)
};

// ── State Variables ──────────────────────────────────────────────────────────
int currentFace = -2;
int candidateFace = -1;
int debounceCount = 0;

// ── Helpers ──────────────────────────────────────────────────────────────────

float magnitude(Vec3 v) { return sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

float angleDeg(Vec3 a, Vec3 b) {
  float magA = magnitude(a);
  float magB = magnitude(b);
  if (magA < 0.001f || magB < 0.001f) return 180.0f;
  float dot = (a.x * b.x + a.y * b.y + a.z * b.z) / (magA * magB);
  return degrees(acos(constrain(dot, -1.0f, 1.0f)));
}

// Function to update the LED color based on the face index
void setTetraColor(int face) {
  switch (face) {
    case 0:                                            // Good
      strip.setPixelColor(0, strip.Color(0, 255, 0));  // Green
      break;
    case 1:                                            // Help
      strip.setPixelColor(0, strip.Color(255, 0, 0));  // Red
      break;
    case 2:                                              // Break
      strip.setPixelColor(0, strip.Color(255, 100, 0));  // Orange
      break;
    case 3:                                            // Ready
      strip.setPixelColor(0, strip.Color(0, 0, 255));  // Blue
      break;
    case -1:                                            // Rotating / Unstable
      strip.setPixelColor(0, strip.Color(50, 50, 50));  // Dim White/Grey
      break;
    default:
      strip.setPixelColor(0, 0);  // Off
      break;
  }
  strip.show();
}

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
  if (mag < MIN_MAG || mag > MAX_MAG) return -1;

  int bestFace = -1;
  float minAngle = 180.0f;

  for (int i = 0; i < 4; i++) {
    float angle = angleDeg(reading, FACE_VECTORS[i]);
    if (angle < minAngle) {
      minAngle = angle;
      bestFace = i;
    }
  }
  if (minAngle > 50.0f) return -1;
  return bestFace;
}

// ── Setup ────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.setBrightness(50);  // Set to ~20% to save battery/eyes
  strip.show();

  if (!mpu.begin()) {
    Serial.println("MPU6050 error!");
    while (1);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Initial color flash to show it's working
  strip.setPixelColor(0, strip.Color(255, 255, 255));
  strip.show();
  delay(500);
}

// ── Loop ─────────────────────────────────────────────────────────────────────

void loop() {
  Vec3 reading = readSmoothed();
  int detected = detectFace(reading);

  if (detected == candidateFace) {
    debounceCount++;
  } else {
    candidateFace = detected;
    debounceCount = 0;
  }

  // Only update if the orientation has been stable for X samples
  if (debounceCount >= DEBOUNCE_COUNT && detected != currentFace) {
    currentFace = detected;
    Serial.print("New State: ");
    Serial.println(currentFace);
    setTetraColor(currentFace);
  }
}