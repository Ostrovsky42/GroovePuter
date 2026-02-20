#include "audio_out_i2s.h"
#include "esp_heap_caps.h"
#include <Arduino.h>

// New ESP-IDF I2S Driver (v5.x API)
// Using I2S_NUM_0 to avoid Port 0 conflicts with M5Unified/Mic/Speaker

// M5Cardputer ADV I2S pins for ES8311 codec
static const int I2S_BCLK = 41;
static const int I2S_LRCLK = 43;
static const int I2S_DOUT = 42;
static const int I2S_MCLK = -1;  // Unused on Cardputer (derived from BCLK)

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
  
  if (stereoBuffer_) {
    memset(stereoBuffer_, 0, bufferFrames * 2 * sizeof(int16_t));
  }
  
  if (!stereoBuffer_) {
    Serial.println("[AudioOutI2S] Failed to allocate stereo buffer");
    return false;
  }
  
  // 1. Create I2S Channel (Standard Mode)
  // Use I2S_NUM_0 to avoid conflicts with M5.Speaker and ESP32-audioI2S on port 0
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num = 8;         // Matches working test config
  chan_cfg.dma_frame_num = 512;      // Matches working test config
  chan_cfg.auto_clear = true;        // Silence on underrun
  
  esp_err_t err = i2s_new_channel(&chan_cfg, &tx_handle_, NULL);
  if (err != ESP_OK) {
    Serial.printf("[AudioOutI2S] i2s_new_channel failed: %d\n", (int)err);
    heap_caps_free(stereoBuffer_);
    stereoBuffer_ = nullptr;
    return false;
  }
  
  // 2. Configure Channel (Slot & Clock)
  i2s_std_config_t std_cfg = {};
  
  // Clock: Use default source (usually PLL or XTAL)
  std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate);
  std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;

  
  // Slot: Standard Philips I2S (Option B from before, matching i2s_test.ino)
  std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  
  // GPIOs
  std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  std_cfg.gpio_cfg.bclk = (gpio_num_t)I2S_BCLK;
  std_cfg.gpio_cfg.ws = (gpio_num_t)I2S_LRCLK;
  std_cfg.gpio_cfg.dout = (gpio_num_t)I2S_DOUT;
  std_cfg.gpio_cfg.din = I2S_GPIO_UNUSED;
  std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
  std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
  std_cfg.gpio_cfg.invert_flags.ws_inv = false;

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
  
  Serial.printf("[AudioOutI2S] Initialized on I2S_NUM_0: %u Hz, %u frames\n", 
                sampleRate, (unsigned)bufferFrames);
  return true;
}

bool AudioOutI2S::writeMono16(const int16_t* monoBuffer, size_t frames) {
  if (!tx_handle_ || !stereoBuffer_ || !monoBuffer) {
    return false;
  }
  
  if (frames > bufferFrames_) {
    frames = bufferFrames_;
  }
  
  // Convert mono to stereo (L=R duplication)
  for (size_t i = 0; i < frames; i++) {
    int16_t sample = monoBuffer[i];
    stereoBuffer_[i * 2 + 0] = sample;
    stereoBuffer_[i * 2 + 1] = sample;
  }
  
  size_t bytesWritten = 0;
  esp_err_t err = i2s_channel_write(
    tx_handle_,
    stereoBuffer_,
    frames * 2 * sizeof(int16_t),
    &bytesWritten,
    pdMS_TO_TICKS(100) // Avoid indefinite hang
  );
  
  if (err != ESP_OK || bytesWritten != frames * 2 * sizeof(int16_t)) {
    // Treat regular timeouts as silent failures (underruns)
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
