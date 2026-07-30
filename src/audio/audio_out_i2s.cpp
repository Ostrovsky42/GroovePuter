#include "audio_out_i2s.h"
#include "../platform/cardputer_adv_hardware.h"
#include "esp_heap_caps.h"
#include <Arduino.h>

// New ESP-IDF I2S Driver (v5.x API)
// Cardputer ADV uses ES8311 on the canonical hardware profile below.

namespace {
constexpr TickType_t kWriteTimeout = pdMS_TO_TICKS(30);
constexpr bool kEnableHardwareMclk = false;
}  // namespace

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

  // Allocate stereo buffer in INTERNAL RAM (critical for DMA).
  stereoBuffer_ = static_cast<int16_t*>(heap_caps_malloc(
      bufferFrames * 2 * sizeof(int16_t),
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

  if (stereoBuffer_) {
    memset(stereoBuffer_, 0, bufferFrames * 2 * sizeof(int16_t));
  }

  if (!stereoBuffer_) {
    Serial.println("[AudioOutI2S] Failed to allocate stereo buffer");
    return false;
  }

  i2s_chan_config_t channelConfig =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  channelConfig.dma_desc_num = 8;
  channelConfig.dma_frame_num = 512;
  channelConfig.auto_clear = true;

  esp_err_t error = i2s_new_channel(&channelConfig, &tx_handle_, nullptr);
  if (error != ESP_OK) {
    Serial.printf("[AudioOutI2S] i2s_new_channel failed: %d\n",
                  static_cast<int>(error));
    heap_caps_free(stereoBuffer_);
    stereoBuffer_ = nullptr;
    return false;
  }

  i2s_std_config_t standardConfig = {};
  standardConfig.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate);
  standardConfig.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
  if (kEnableHardwareMclk) {
    standardConfig.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  }

  standardConfig.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  standardConfig.gpio_cfg.mclk = kEnableHardwareMclk
      ? static_cast<gpio_num_t>(GroovePuterHardware::kI2sMasterClockPin)
      : I2S_GPIO_UNUSED;
  standardConfig.gpio_cfg.bclk =
      static_cast<gpio_num_t>(GroovePuterHardware::kI2sBitClockPin);
  standardConfig.gpio_cfg.ws =
      static_cast<gpio_num_t>(GroovePuterHardware::kI2sWordSelectPin);
  standardConfig.gpio_cfg.dout =
      static_cast<gpio_num_t>(GroovePuterHardware::kI2sDataOutPin);
  standardConfig.gpio_cfg.din = I2S_GPIO_UNUSED;
  standardConfig.gpio_cfg.invert_flags.mclk_inv = false;
  standardConfig.gpio_cfg.invert_flags.bclk_inv = false;
  standardConfig.gpio_cfg.invert_flags.ws_inv = false;

  error = i2s_channel_init_std_mode(tx_handle_, &standardConfig);
  if (error != ESP_OK) {
    Serial.printf("[AudioOutI2S] i2s_channel_init_std_mode failed: %d\n",
                  static_cast<int>(error));
    i2s_del_channel(tx_handle_);
    tx_handle_ = nullptr;
    heap_caps_free(stereoBuffer_);
    stereoBuffer_ = nullptr;
    return false;
  }

  error = i2s_channel_enable(tx_handle_);
  if (error != ESP_OK) {
    Serial.printf("[AudioOutI2S] i2s_channel_enable failed: %d\n",
                  static_cast<int>(error));
    i2s_del_channel(tx_handle_);
    tx_handle_ = nullptr;
    heap_caps_free(stereoBuffer_);
    stereoBuffer_ = nullptr;
    return false;
  }

  Serial.printf(
      "[AudioOutI2S] ADV I2S0: %u Hz, %u frames, BCLK=%d, WS=%d, DOUT=%d, MCLK=%s\n",
      sampleRate,
      static_cast<unsigned>(bufferFrames),
      GroovePuterHardware::kI2sBitClockPin,
      GroovePuterHardware::kI2sWordSelectPin,
      GroovePuterHardware::kI2sDataOutPin,
      kEnableHardwareMclk ? "ON" : "OFF");
  return true;
}

bool AudioOutI2S::writeMono16(const int16_t* monoBuffer, size_t frames) {
  if (!tx_handle_ || !stereoBuffer_ || !monoBuffer) return false;

  if (frames > bufferFrames_) frames = bufferFrames_;

  for (size_t i = 0; i < frames; ++i) {
    const int16_t sample = monoBuffer[i];
    stereoBuffer_[i * 2] = sample;
    stereoBuffer_[i * 2 + 1] = sample;
  }

  size_t bytesWritten = 0;
  const size_t expectedBytes = frames * 2 * sizeof(int16_t);
  const esp_err_t error = i2s_channel_write(
      tx_handle_, stereoBuffer_, expectedBytes, &bytesWritten, kWriteTimeout);

  return error == ESP_OK && bytesWritten == expectedBytes;
}

void AudioOutI2S::end() {
  if (!tx_handle_) return;

  i2s_channel_disable(tx_handle_);
  i2s_del_channel(tx_handle_);
  tx_handle_ = nullptr;

  if (stereoBuffer_) {
    heap_caps_free(stereoBuffer_);
    stereoBuffer_ = nullptr;
  }

  Serial.println("[AudioOutI2S] Stopped");
}
