// Single NeoPixel Test
// XIAO mini ESP32-S3 + NeoPixel (WS2812B) 1 LED
//
// 🔌 WIRING
//
// 📡 DATA LINE:
//  Pin 6 ──[330Ω]──> DIN (NeoPixel)
// • Resistor Protects the first LED from signal spikes

#include <Adafruit_NeoPixel.h>

const int ledPin = D6;
const int N_LEDs = 1;

Adafruit_NeoPixel strip =
    Adafruit_NeoPixel(N_LEDs, ledPin, NEO_GRB + NEO_KHZ800);

void setup() { strip.begin(); }

void loop() {
  chase(strip.Color(255, 0, 0));  // Red
  chase(strip.Color(0, 255, 0));  // Green
  chase(strip.Color(0, 0, 255));  // Blue
}

static void chase(uint32_t c) {
  for (uint16_t i = 0; i < strip.numPixels() + 4; i++) {
    strip.setPixelColor(i, c);
    strip.setPixelColor(i - 4, 0);
    strip.show();
    delay(25);
  }
}