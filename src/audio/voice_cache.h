#pragma once

#include <Arduino.h>
#include <SD.h>
#include <FS.h>
#include <string>
#include <cstring>

/**
 * VoiceCache - SD Card Voice Caching System
 * 
 * Stores pre-synthesized voice phrases as RAW audio files on SD card.
 * Provides instant playback for cached phrases without CPU-intensive synthesis.
 * 
 * Storage format: /scenes/voices/<hash>.raw (16-bit signed, 22050 Hz, mono)
 */
class VoiceCache {
public:
    static constexpr const char* kVoiceDir = "/scenes/voices";
    static constexpr uint32_t kSampleRate = 22050;
    static constexpr size_t kMaxPhraseLength = 5 * kSampleRate; // 5 seconds max
    static constexpr size_t kStreamBufferSize = 512; // Samples per read
    
    VoiceCache() = default;
    
    /**
     * Initialize the cache directory on SD card
     */
    bool init() {
        if (!SD.exists(kVoiceDir)) {
            if (!SD.mkdir(kVoiceDir)) {
                Serial.println("[VoiceCache] Failed to create voices directory");
                return false;
            }
            Serial.println("[VoiceCache] Created voices directory");
        }
        initialized_ = true;
        return true;
    }
    
    /**
     * Check if a phrase is cached on SD
     */
    bool isCached(const char* text) const {
        if (!initialized_) return false;
        std::string path = pathForPhrase(text);
        return SD.exists(path.c_str());
    }
    
    /**
     * Cache a phrase by writing samples to SD
     * @param text The phrase text (used for filename hash)
     * @param samples Audio samples (16-bit signed)
     * @param numSamples Number of samples
     * @return true if cached successfully
     */
    bool cachePhrase(const char* text, const int16_t* samples, size_t numSamples) {
        if (!initialized_ || !samples || numSamples == 0) return false;
        
        std::string path = pathForPhrase(text);
        
        // Remove existing file
        if (SD.exists(path.c_str())) {
            SD.remove(path.c_str());
        }
        
        File file = SD.open(path.c_str(), FILE_WRITE);
        if (!file) {
            Serial.printf("[VoiceCache] Failed to open %s for writing\n", path.c_str());
            return false;
        }
        
        // Write raw samples
        size_t written = file.write((const uint8_t*)samples, numSamples * sizeof(int16_t));
        file.close();
        
        if (written == numSamples * sizeof(int16_t)) {
            Serial.printf("[VoiceCache] Cached '%s' -> %s (%d samples)\n", 
                         text, path.c_str(), numSamples);
            return true;
        }
        
        Serial.printf("[VoiceCache] Write failed: %d/%d bytes\n", 
                     written, numSamples * sizeof(int16_t));
        SD.remove(path.c_str());
        return false;
    }
    
    /**
     * Start streaming a cached phrase
     * @param text The phrase to play
     * @return true if file opened successfully
     */
    bool startPlayback(const char* text) {
        stopPlayback();
        
        if (!initialized_) return false;
        
        std::string path = pathForPhrase(text);
        playbackFile_ = SD.open(path.c_str(), FILE_READ);
        
        if (!playbackFile_) {
            return false;
        }
        
        playbackActive_ = true;
        totalSamples_ = playbackFile_.size() / sizeof(int16_t);
        samplesRead_ = 0;
        
        Serial.printf("[VoiceCache] Playing %s (%d samples)\n", path.c_str(), totalSamples_);
        return true;
    }
    
    /**
     * Read next batch of samples for playback
     * @param buffer Output buffer
     * @param maxSamples Maximum samples to read
     * @return Number of samples read (0 = end of file)
     */
    size_t readSamples(int16_t* buffer, size_t maxSamples) {
        if (!playbackActive_ || !playbackFile_) return 0;
        
        size_t bytesToRead = maxSamples * sizeof(int16_t);
        size_t bytesRead = playbackFile_.read((uint8_t*)buffer, bytesToRead);
        size_t samplesRead = bytesRead / sizeof(int16_t);
        
        samplesRead_ += samplesRead;
        
        if (samplesRead == 0 || samplesRead_ >= totalSamples_) {
            stopPlayback();
        }
        
        return samplesRead;
    }
    
    /**
     * Stop current playback
     */
    void stopPlayback() {
        if (playbackFile_) {
            playbackFile_.close();
        }
        playbackActive_ = false;
        samplesRead_ = 0;
        totalSamples_ = 0;
    }
    
    /**
     * Check if currently playing
     */
    bool isPlaying() const { return playbackActive_; }
    
    /**
     * Get playback progress (0.0 - 1.0)
     */
    float getProgress() const {
        if (totalSamples_ == 0) return 0.0f;
        return (float)samplesRead_ / (float)totalSamples_;
    }
    
    /**
     * Delete a cached phrase
     */
    bool removePhrase(const char* text) {
        if (!initialized_) return false;
        std::string path = pathForPhrase(text);
        if (SD.exists(path.c_str())) {
            return SD.remove(path.c_str());
        }
        return true;
    }
    
    /**
     * Clear all cached voices
     */
    void clearAll() {
        if (!initialized_) return;
        
        stopPlayback();
        
        File dir = SD.open(kVoiceDir);
        if (!dir || !dir.isDirectory()) return;
        
        File entry;
        while ((entry = dir.openNextFile())) {
            if (!entry.isDirectory()) {
                std::string path = kVoiceDir;
                path += "/";
                path += entry.name();
                SD.remove(path.c_str());
            }
            entry.close();
        }
        dir.close();
        
        Serial.println("[VoiceCache] Cleared all cached voices");
    }
    
    /**
     * Get count of cached phrases
     */
    int getCacheCount() const {
        if (!initialized_) return 0;
        
        int count = 0;
        File dir = SD.open(kVoiceDir);
        if (!dir || !dir.isDirectory()) return 0;
        
        File entry;
        while ((entry = dir.openNextFile())) {
            if (!entry.isDirectory()) {
                count++;
            }
            entry.close();
        }
        dir.close();
        return count;
    }
    
    bool isInitialized() const { return initialized_; }

private:
    /**
     * Generate file path for a phrase (using hash)
     */
    std::string pathForPhrase(const char* text) const {
        uint32_t hash = hashString(text);
        char filename[32];
        snprintf(filename, sizeof(filename), "/%08X.raw", hash);
        
        std::string path = kVoiceDir;
        path += filename;
        return path;
    }
    
    /**
     * Simple DJB2 hash for phrase text
     */
    static uint32_t hashString(const char* str) {
        uint32_t hash = 5381;
        int c;
        while ((c = *str++)) {
            hash = ((hash << 5) + hash) + c;
        }
        return hash;
    }
    
    bool initialized_ = false;
    File playbackFile_;
    volatile bool playbackActive_ = false;
    size_t totalSamples_ = 0;
    size_t samplesRead_ = 0;
};
