// Direct I2S Audio Test for M5Cardputer Adv
// Bypasses M5Unified Speaker to diagnose transport issues
// If this is clean: problem is in Speaker/playRaw layer
// If this crackles: problem is hardware/pins/clock

#include <Arduino.h>
#include "driver/i2s.h"
#include <math.h>
#include "esp_heap_caps.h"

// === CONFIGURATION ===
static const int SAMPLE_RATE = 22050;
static const int FRAMES = 512;
static const float FREQ = 440.0f;  // Test tone frequency

// M5Cardputer ADV I2S pins for internal speaker (NS4150B via ES8311)
// These should match your board - verify in M5Unified source if different
static const int I2S_BCLK = 41;
static const int I2S_LRCLK = 43;
static const int I2S_DOUT = 42;

static int16_t* audioLR = nullptr;
static float phase = 0.0f;

static void i2sInit() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 8;       // More DMA buffers = less gaps
  cfg.dma_buf_len = FRAMES;
  cfg.use_apll = true;         // APLL for cleaner clock
  cfg.tx_desc_auto_clear = true;
  cfg.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = I2S_BCLK;
  pins.ws_io_num = I2S_LRCLK;
  pins.data_out_num = I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  esp_err_t e;
  e = i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
  if (e != ESP_OK) {
    Serial.printf("i2s_driver_install err=%d\n", (int)e);
    while(true) delay(1000);
  }

  e = i2s_set_pin(I2S_NUM_0, &pins);
  if (e != ESP_OK) {
    Serial.printf("i2s_set_pin err=%d\n", (int)e);
    while(true) delay(1000);
  }

  e = i2s_zero_dma_buffer(I2S_NUM_0);
  if (e != ESP_OK) {
    Serial.printf("i2s_zero_dma_buffer err=%d\n", (int)e);
    while(true) delay(1000);
  }
  
  Serial.println("I2S initialized OK");
}

static void fillSineBlock() {
  const float phaseInc = 2.0f * (float)M_PI * FREQ / (float)SAMPLE_RATE;
  for (int i = 0; i < FRAMES; i++) {
    float s = sinf(phase);
    phase += phaseInc;
    if (phase >= 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;

    // Moderate amplitude to avoid amp clipping
    int16_t v = (int16_t)(s * 8000.0f);
    audioLR[i * 2 + 0] = v; // L
    audioLR[i * 2 + 1] = v; // R
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n=== Direct I2S Audio Test ===");
  Serial.printf("Sample Rate: %d Hz\n", SAMPLE_RATE);
  Serial.printf("Buffer: %d frames (stereo)\n", FRAMES);
  Serial.printf("Frequency: %.0f Hz\n", FREQ);

  // Allocate in INTERNAL RAM (not PSRAM!)
  audioLR = (int16_t*)heap_caps_malloc(FRAMES * 2 * sizeof(int16_t), 
                                        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!audioLR) {
    Serial.println("FATAL: audio buffer alloc failed");
    while(true) delay(1000);
  }
  Serial.println("Audio buffer allocated in DRAM");

  // Disable interference sources
  setCpuFrequencyMhz(240);
  // WiFi.mode(WIFI_OFF);  // Uncomment if WiFi.h included
  // btStop();             // Uncomment if BT was initialized

  i2sInit();
  
  Serial.println("Starting audio output...");
  Serial.println("Listen for clean sine wave. Press any key to print stats.");
}

void loop() {
  fillSineBlock();
  
  size_t bytesWritten = 0;
  esp_err_t e = i2s_write(I2S_NUM_0, (const char*)audioLR, 
                          FRAMES * 2 * sizeof(int16_t),
                          &bytesWritten, portMAX_DELAY);
  
  if (e != ESP_OK || bytesWritten != FRAMES * 2 * sizeof(int16_t)) {
    Serial.printf("i2s_write err=%d bytes=%u (expected %u)\n", 
                  (int)e, (unsigned)bytesWritten, 
                  (unsigned)(FRAMES * 2 * sizeof(int16_t)));
  }
  
  // Print stats on serial input
  if (Serial.available()) {
    Serial.read();
    Serial.printf("Phase: %.4f, Last sample: %d\n", phase, audioLR[0]);
  }
}
