#include "audio_out_i2s.h"
#include "esp_heap_caps.h"
#include <Arduino.h>

// New ESP-IDF I2S Driver (avoids conflict with M5Unified/M5GFX)
// M5Cardputer ADV I2S pins for ES8311 codec
static const int I2S_BCLK = 41;
static const int I2S_LRCLK = 43;
static const int I2S_DOUT = 42;

AudioOutI2S::AudioOutI2S() 
  : sampleRate_(0)
  , bufferFrames_(0)
  , stereoBuffer_(nullptr)
  , tx_handle_(nullptr)
{
}

AudioOutI2S::~AudioOutI2S() {
  end();
}

bool AudioOutI2S::begin(uint32_t sampleRate, size_t bufferFrames) {
  if (tx_handle_) {
    Serial.println("[AudioOutI2S] Already initialized");
    return false;
  }
  
  sampleRate_ = sampleRate;
  bufferFrames_ = bufferFrames;
  
  // Allocate stereo buffer in INTERNAL RAM (critical for DMA!)
  stereoBuffer_ = (int16_t*)heap_caps_malloc(
    bufferFrames * 2 * sizeof(int16_t),
    MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
  );
  
  if (!stereoBuffer_) {
    Serial.println("[AudioOutI2S] Failed to allocate stereo buffer");
    return false;
  }
  
  // 1. Create I2S Channel (Standard Mode)
  // Use I2S_NUM_1 to minimize conflict chance with M5Unified (which might grab 0)
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num = 8;         // Number of DMA buffers
  chan_cfg.dma_frame_num = 256;      // Fixed smaller frame size for better latency
  chan_cfg.auto_clear = true;        // Silence on underrun
  
  esp_err_t err = i2s_new_channel(&chan_cfg, &tx_handle_, NULL);
  if (err != ESP_OK) {
    Serial.printf("[AudioOutI2S] i2s_new_channel failed: %d\n", (int)err);
    heap_caps_free(stereoBuffer_);
    stereoBuffer_ = nullptr;
    return false;
  }
  
  // 2. Configure Channel (Slot & Clock)
  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate),
      // --- MODE SELECTION ---
      // Option A: MSB Mode (Usually correct for ES8311 in M5Stack)
      //Option A: MSB Mode (Usually correct for ES8311 in M5Stack)
      //.slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
      
      // Option B: Philips Mode (Standard I2S, try if sound is distorted/quiet)
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
      // ----------------------
      .gpio_cfg = {
          .mclk = I2S_GPIO_UNUSED,
          .bclk = (gpio_num_t)I2S_BCLK,
          .ws = (gpio_num_t)I2S_LRCLK,
          .dout = (gpio_num_t)I2S_DOUT,
          .din = I2S_GPIO_UNUSED,
          .invert_flags = {
              .mclk_inv = false,
              .bclk_inv = false,
              .ws_inv = false,
          },
      },
  };

  // Important: M5Unified typically uses I2S_STD_SLOT_DEFAULT_CONFIG (Philips)
  // But some codecs prefer MSB. If sound is missing/distorted, try I2S_STD_SLOT_DEFAULT_CONFIG
  
  err = i2s_channel_init_std_mode(tx_handle_, &std_cfg);
  if (err != ESP_OK) {
    Serial.printf("[AudioOutI2S] i2s_channel_init_std_mode failed: %d\n", (int)err);
    i2s_del_channel(tx_handle_);
    tx_handle_ = nullptr;
    heap_caps_free(stereoBuffer_);
    stereoBuffer_ = nullptr;
    return false;
  }
  
  // 3. Enable Channel
  err = i2s_channel_enable(tx_handle_);
  if (err != ESP_OK) {
    Serial.printf("[AudioOutI2S] i2s_channel_enable failed: %d\n", (int)err);
    i2s_del_channel(tx_handle_);
    tx_handle_ = nullptr;
    heap_caps_free(stereoBuffer_);
    stereoBuffer_ = nullptr;
    return false;
  }
  
  Serial.printf("[AudioOutI2S] Initialized i2s_std: %u Hz, %u frames, 8 DMA bufs\n", 
                sampleRate, (unsigned)bufferFrames);
  return true;
}

bool AudioOutI2S::writeMono16(const int16_t* monoBuffer, size_t frames) {
  if (!tx_handle_ || !stereoBuffer_ || !monoBuffer) {
    return false;
  }
  
  if (frames > bufferFrames_) {
    frames = bufferFrames_;  // Clamp to allocated size
  }
  
  // Convert mono to stereo (L=R duplication)
  for (size_t i = 0; i < frames; i++) {
    int16_t sample = monoBuffer[i];
    stereoBuffer_[i * 2 + 0] = sample;  // L
    stereoBuffer_[i * 2 + 1] = sample;  // R
  }
  
  size_t bytesWritten = 0;
  esp_err_t err = i2s_channel_write(
    tx_handle_,
    stereoBuffer_,
    frames * 2 * sizeof(int16_t),
    &bytesWritten,
    pdMS_TO_TICKS(100) // Block with timeout (avoid indefinite hang)
  );
  
  if (err != ESP_OK || bytesWritten != frames * 2 * sizeof(int16_t)) {
    Serial.printf("[AudioOutI2S] i2s_channel_write error: %d, bytes: %u/%u\n",
                  (int)err, (unsigned)bytesWritten, 
                  (unsigned)(frames * 2 * sizeof(int16_t)));
    return false;
  }
  
  return true;
}

void AudioOutI2S::end() {
  if (!tx_handle_) {
    return;
  }
  
  i2s_channel_disable(tx_handle_);
  i2s_del_channel(tx_handle_);
  tx_handle_ = nullptr;
  
  if (stereoBuffer_) {
    heap_caps_free(stereoBuffer_);
    stereoBuffer_ = nullptr;
  }
  
  Serial.println("[AudioOutI2S] Stopped");
}
