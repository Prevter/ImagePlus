#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

#include "../NV12.hpp"

/// @brief Opaque platform-specific video state handle
class VideoHandle {
private:
    struct Impl;
    Impl* m_impl;

    VideoHandle(Impl* impl);

public:
    VideoHandle() : m_impl(nullptr) {}
    ~VideoHandle() { if (m_impl) destroyImpl(m_impl); }

    VideoHandle(VideoHandle const&) = delete;
    VideoHandle(VideoHandle&& other) noexcept {
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }

    VideoHandle& operator=(VideoHandle const&) = delete;
    VideoHandle& operator=(VideoHandle&& other) noexcept {
        if (this != &other) {
            if (m_impl) {
                destroyImpl(m_impl);
            }
            m_impl = other.m_impl;
            other.m_impl = nullptr;
        }
        return *this;
    }

    static geode::Result<VideoHandle> createFromMemory(std::vector<uint8_t>&& data);
    static geode::Result<VideoHandle> createFromFile(std::filesystem::path const& path);

    operator bool() const { return m_impl != nullptr; }

    bool readNextFrame(std::vector<uint8_t>& frameData);

    bool seek(double timeInSeconds);
    bool seekToFrame(uint64_t frameNumber);

    /// == Video == ///

    ColorSpace getColorSpace() const;
    ColorRange getColorRange() const;

    uint32_t getWidth() const;
    uint32_t getHeight() const;
    uint32_t getBufferWidth() const;
    uint32_t getBufferHeight() const;

    double getFps() const;
    double getFrameTime() const;
    double getDurationInSeconds() const;

    /// == Audio == ///

    bool hasAudio() const;
    uint32_t getAudioSampleRate() const;
    uint32_t getAudioChannels() const;
    uint32_t getAudioBitsPerSample() const;

    uint8_t const* getAudioData() const; // raw PCM data
    size_t getAudioDataSize() const; // in samples
    FMOD::Sound* getSound() const;
    void assignChannel(FMOD::Channel* channel);

private:
    static void destroyImpl(Impl* impl);
};
