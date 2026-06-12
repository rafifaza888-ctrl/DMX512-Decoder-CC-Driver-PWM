#include <Arduino.h>

/* * NODE 1 - DMX to 5-Channel LED Controller
 * Handle DMX Channels: 1, 2, 3, 4, 5
 */

// Konfigurasi Pin RS485 (ESP32)
#define RXD2 16
#define TXD2 17
#define DMX_BAUD 243000 // Sesuai pembacaan osiloskop pada FT232RL

// --- CONFIGURATION NODE 1 ---
int startAddress = 6;         // Channel 1 di QLC+
const int ledPins[] = {13, 12, 27, 26, 25}; 
const int numPins = 5;

// Parameter LEDC 16-bit
#define LEDC_FREQ 1000        // Frekuensi aman untuk resolusi 16-bit
#define LEDC_RES 12           // Resolusi 0-65535

// Buffer & Smoothing
uint8_t dmxBuffer[513];
float smoothedValues[numPins]; 
float filterFactor = 0.15;     // Menghilangkan langkah (stepping) fader

int channelIndex = 0;
unsigned long lastByteTime = 0;
void updateLamps() {
  for (int i = 0; i < numPins; i++) {
    // Menghitung index channel (Node 1: dmxBuffer[1] s/d dmxBuffer[5])
    int targetChannel = startAddress + i;
    
    if (targetChannel < 513) {
      uint8_t raw8bit = dmxBuffer[targetChannel];

      // Konversi 8-bit (0-255) ke 16-bit (0-65535) yang sempurna
      uint32_t target12bit = (raw8bit << 4) | (raw8bit >> 4);

      // Filter EMA agar transisi sangat mulus (anti lompat)
      smoothedValues[i] = (filterFactor * target12bit) + ((1.0 - filterFactor) * smoothedValues[i]);

      // Output ke PWM LEDC
      ledcWrite(i, (uint32_t)smoothedValues[i]);
    }
  }
}

void updateLamps();

void setup() {
  // Serial Monitor untuk Debug
  Serial.begin(115200);
  
  // Serial2 untuk menerima data dari MAX3485
  Serial2.begin(DMX_BAUD, SERIAL_8N2, RXD2, TXD2);

  // Inisialisasi LEDC (API v2.x)
  for (int i = 0; i < numPins; i++) {
    ledcSetup(i, LEDC_FREQ, LEDC_RES); 
    ledcAttachPin(ledPins[i], i); 
    smoothedValues[i] = 0;
  }

  Serial.println(">>> NODE 1 INITIALIZED <<<");
  Serial.printf("Start Address: %d\n", startAddress);
  Serial.printf("Channels: %d - %d\n", startAddress, startAddress + numPins - 1);
}

void loop() {
  while (Serial2.available()) {
    uint8_t incomingByte = Serial2.read();
    unsigned long currentTime = micros();

    // Reset paket jika ada jeda (Sync) > 2ms
    if (currentTime - lastByteTime > 2000) { 
      channelIndex = 0; 
    }
    lastByteTime = currentTime;

    // Isi buffer paket DMX (0-512)
    if (channelIndex < 513) {
      dmxBuffer[channelIndex] = incomingByte;
      channelIndex++;
    }

    // Jalankan update jika:
    // 1. Sudah menerima cukup data untuk Node 1 (Start Code + 5 Ch)
    // 2. Start Code (indeks 0) adalah 0x00 (Valid DMX)
    if (channelIndex >= (startAddress + numPins) && dmxBuffer[0] == 0) {
      updateLamps();
    }
  }
}