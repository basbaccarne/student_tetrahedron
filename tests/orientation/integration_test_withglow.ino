/*
  Student Tetrahedron — Orientation + Soft Glow (Breathing)
  Hardware : XIAO MINI ESP32S + MPU6050 + 1 x WS2812B NeoPixel
  This test builds on the basic orientation detection to add a soft breathing
  glow effect to the NeoPixel. The LED will smoothly pulse in brightness while
  maintaining the color corresponding to the detected face. This makes the
  feedback more visually appealing and easier to notice
*/

#include <Adafruit_MPU6050.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

// ── Configuration ────────────────────────────────────────────────────────────
#define LED_PIN D6
#define NUM_LEDS 1
#define SAMPLES 5          // Reduced samples for faster LED refresh
#define DEBOUNCE_COUNT 10  // Increased to compensate for faster loop

#define MIN_MAG 5.0f
#define MAX_MAG 15.0f

Adafruit_MPU6050 mpu;
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

struct Vec3 {
  float x, y, z;
};

const Vec3 FACE_VECTORS[4] = {
    {0.494f, 0.040f, -12.610f},  // Face 0: Green
    {-7.802f, -4.533f, 0.600f},  // Face 1: Red
    {0.720f, 9.341f, 0.913f},    // Face 2: Orange
    {8.239f, -5.002f, 0.883f}    // Face 3: Blue
};

// ── State Variables ──────────────────────────────────────────────────────────
int currentFace = -1;
int candidateFace = -1;
int debounceCount = 0;

// Colors (R, G, B)
uint8_t targetR = 50, targetG = 50, targetB = 50;

// ── Math & Sensing ───────────────────────────────────────────────────────────

float magnitude(Vec3 v) { return sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

float angleDeg(Vec3 a, Vec3 b) {
  float magA = magnitude(a);
  float magB = magnitude(b);
  if (magA < 0.001f || magB < 0.001f) return 180.0f;
  float dot = (a.x * b.x + a.y * b.y + a.z * b.z) / (magA * magB);
  return degrees(acos(constrain(dot, -1.0f, 1.0f)));
}

// Sets the color target; the loop handles the "glow"
void updateTargetColor(int face) {
  switch (face) {
    case 0:
      targetR = 0;
      targetG = 255;
      targetB = 0;
      break;  // Green
    case 1:
      targetR = 255;
      targetG = 0;
      targetB = 0;
      break;  // Red
    case 2:
      targetR = 255;
      targetG = 80;
      targetB = 0;
      break;  // Orange
    case 3:
      targetR = 0;
      targetG = 0;
      targetB = 255;
      break;  // Blue
    default:
      targetR = 30;
      targetG = 30;
      targetB = 30;
      break;  // Dim White
  }
}

// ── Glow Logic ───────────────────────────────────────────────────────────────

void updateGlow() {
  // Use a sine wave to calculate brightness (0.0 to 1.0)
  // millis() / 1000.0 controls the speed of the breath
  float speed = 2.0;
  float intensity = (sin(millis() * 0.001 * speed) + 1.0) / 2.0;

  // Scale intensity so it never goes completely black (0.2 to 1.0 range)
  intensity = 0.2 + (intensity * 0.8);

  strip.setPixelColor(0, strip.Color((uint8_t)(targetR * intensity),
                                     (uint8_t)(targetG * intensity),
                                     (uint8_t)(targetB * intensity)));
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
  return (minAngle > 45.0f) ? -1 : bestFace;
}

// ── Setup & Loop ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.setBrightness(100);  // We handle dimming via math, so keep global high

  if (!mpu.begin()) {
    while (1);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

void loop() {
  Vec3 reading = readSmoothed();
  int detected = detectFace(reading);

  // Debounce
  if (detected == candidateFace) {
    debounceCount++;
  } else {
    candidateFace = detected;
    debounceCount = 0;
  }

  if (debounceCount >= DEBOUNCE_COUNT && detected != currentFace) {
    currentFace = detected;
    updateTargetColor(currentFace);
  }

  // Always update the glow intensity every frame
  updateGlow();
}