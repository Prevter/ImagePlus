#pragma once
#ifndef IMAGE_PLUS_EVENTS_HPP
#define IMAGE_PLUS_EVENTS_HPP
#define IMAGE_PLUS_USE_EVENTS

#include <cstddef>
#include <Geode/Result.hpp>
#include <Geode/loader/Event.hpp>

#include "types.hpp"

namespace imgp {
    namespace __detail {
        /// @brief Table used for dynamic function resolution
        struct FunctionTable {
            using GuessFormat = ImageFormat (*)(void const*, size_t);
            using CheckFunc = bool (*)(void const*, size_t);
            using DecodeFunc1 = geode::Result<DecodedImage> (*)(void const*, size_t);
            using DecodeFunc2 = geode::Result<DecodedResult> (*)(void const*, size_t);
            using DecodeFunc3 = geode::Result<DecodedResult> (*)(void const*, size_t, ImageFormat);
            using EncodeFunc1 = geode::Result<geode::ByteVector> (*)(void const*, uint16_t, uint16_t, bool);
            using EncodeFunc2 = geode::Result<geode::ByteVector> (*)(void const*, uint16_t, uint16_t, bool, float);
            using EncodeFunc3 = geode::Result<geode::ByteVector> (*)(DecodedAnimation const&, float);
            using EncodeFunc4 = geode::Result<geode::ByteVector> (*)(DecodedAnimation const&);
            using AnimatedSpriteBoolRet = bool (cocos2d::CCSprite::*)();
            using AnimatedSpriteVoidRet = void (cocos2d::CCSprite::*)();
            using AnimatedSpriteSetPlaybackSpeed = void (cocos2d::CCSprite::*)(float);
            using AnimatedSpriteGetPlaybackSpeed = float (cocos2d::CCSprite::*)();
            using AnimatedSpriteSetForceLoop = void (cocos2d::CCSprite::*)(std::optional<bool>);
            using AnimatedSpriteGetForceLoop = std::optional<bool> (cocos2d::CCSprite::*)();
            using AnimatedSpriteGetCurrentFrame = uint32_t (cocos2d::CCSprite::*)();
            using AnimatedSpriteSetCurrentFrame = void (cocos2d::CCSprite::*)(uint32_t);
            using AnimatedSpriteGetFrameCount = size_t (cocos2d::CCSprite::*)();

            // For adding new functions and checking version compatibility
            size_t version = 1;

            // == Guessing Format == //
            GuessFormat guessFormat = nullptr;
            DecodeFunc3 tryDecode = nullptr;

            // == Type Detection == //
            CheckFunc isJpeg = nullptr;
            CheckFunc isAPng = nullptr;
            CheckFunc isPng = nullptr;
            CheckFunc isGif = nullptr;
            CheckFunc isWebp = nullptr;
            CheckFunc isTiff = nullptr;
            CheckFunc isQoi = nullptr;
            CheckFunc isJpegXL = nullptr;

            // == Static Image Decoding == //
            DecodeFunc1 decodePng = nullptr;
            DecodeFunc1 decodeQoi = nullptr;

            // == Animated Image Decoding == //
            DecodeFunc2 decodeJpegXL = nullptr;
            DecodeFunc2 decodeWebp = nullptr;
            DecodeFunc2 decodeGif = nullptr;

            // == Static Image Encoding == //
            EncodeFunc1 encodePng = nullptr;
            EncodeFunc1 encodeQoi = nullptr;
            EncodeFunc2 encodeWebp = nullptr;
            EncodeFunc2 encodeJpegXL = nullptr;

            // == Animated Image Encoding == //
            EncodeFunc3 encodeWebpAnim = nullptr;
            EncodeFunc3 encodeJpegXLAnim = nullptr;

            // == AnimatedSprite == //
            AnimatedSpriteBoolRet AnimatedSprite_isAnimated = nullptr;
            AnimatedSpriteVoidRet AnimatedSprite_stop = nullptr;
            AnimatedSpriteVoidRet AnimatedSprite_pause = nullptr;
            AnimatedSpriteVoidRet AnimatedSprite_play = nullptr;
            AnimatedSpriteBoolRet AnimatedSprite_isPaused = nullptr;
            AnimatedSpriteSetPlaybackSpeed AnimatedSprite_setPlaybackSpeed = nullptr;
            AnimatedSpriteGetPlaybackSpeed AnimatedSprite_getPlaybackSpeed = nullptr;
            AnimatedSpriteSetForceLoop AnimatedSprite_setForceLoop = nullptr;
            AnimatedSpriteGetForceLoop AnimatedSprite_getForceLoop = nullptr;
            AnimatedSpriteGetCurrentFrame AnimatedSprite_getCurrentFrame = nullptr;
            AnimatedSpriteSetCurrentFrame AnimatedSprite_setCurrentFrame = nullptr;
            AnimatedSpriteGetFrameCount AnimatedSprite_getFrameCount = nullptr;
        };

        struct FetchTableEvent final : geode::Event {
            FetchTableEvent() = default;
            FunctionTable const* table = nullptr;
        };

        inline FunctionTable const* getFunctionTable() {
            static FunctionTable const* table = nullptr;
            if (!table) {
                FetchTableEvent evt{};
                evt.post();
                table = evt.table;
            }
            return table;
        }
    }

    /// @brief Checks whether ImagePlus functions are available.
    /// Make sure to check this at least once, as the functions will return default values/errors otherwise.
    inline bool isAvailable() {
        return __detail::getFunctionTable() != nullptr;
    }

    /// @brief Attempts to guess the image format based on the header data
    /// @param data Pointer to the image data
    /// @param size Size of the image data
    /// @return The guessed image format, or ImageFormat::Unknown if it cannot be determined
    inline ImageFormat guessFormat(void const* data, size_t size) {
        auto table = __detail::getFunctionTable();
        if (!table || !table->guessFormat)
            return ImageFormat::Unknown;
        return table->guessFormat(data, size);
    }

    /// @brief Decodes an image from raw data with provided format
    /// @param data Pointer to the image data
    /// @param size Size of the image data
    /// @param format The format of the image data, defaults to ImageFormat::Unknown (auto-detect)
    /// @return Result containing the decoded image or an error message
    inline geode::Result<DecodedResult> tryDecode(void const* data, size_t size, ImageFormat format = ImageFormat::Unknown) {
        auto table = __detail::getFunctionTable();
        if (!table || !table->tryDecode)
            return geode::Err("ImagePlus is not available");
        return table->tryDecode(data, size, format);
    }

    #define IMAGE_PLUS_GEN_CHECK_FUNC(name) \
        inline bool name(void const* data, size_t size) { \
            auto table = __detail::getFunctionTable(); \
            if (!table || !table->name) \
                return false; \
            return table->name(data, size); \
        }

    #define IMAGE_PLUS_GEN_DECODE_FUNC1(name, func) \
        inline geode::Result<DecodedImage> name(void const* data, size_t size) { \
            auto table = __detail::getFunctionTable(); \
            if (!table || !table->func) \
                return geode::Err("ImagePlus is not available"); \
            return table->func(data, size); \
        }

    #define IMAGE_PLUS_GEN_DECODE_FUNC2(name, func) \
        inline geode::Result<DecodedResult> name(void const* data, size_t size) { \
            auto table = __detail::getFunctionTable(); \
            if (!table || !table->func) \
                return geode::Err("ImagePlus is not available"); \
            return table->func(data, size); \
        }

    #define IMAGE_PLUS_GEN_ENCODE_FUNC1(name, func) \
        inline geode::Result<geode::ByteVector> name(void const* image, uint16_t width, uint16_t height, bool hasAlpha = true) { \
            auto table = __detail::getFunctionTable(); \
            if (!table || !table->func) \
                return geode::Err("ImagePlus is not available"); \
            return table->func(image, width, height, hasAlpha); \
        }

    #define IMAGE_PLUS_GEN_ENCODE_FUNC2(name, func) \
        inline geode::Result<geode::ByteVector> name(void const* image, uint16_t width, uint16_t height, bool hasAlpha = true, float quality = 75.f) { \
            auto table = __detail::getFunctionTable(); \
            if (!table || !table->func) \
                return geode::Err("ImagePlus is not available"); \
            return table->func(image, width, height, hasAlpha, quality); \
        }

    #define IMAGE_PLUS_GEN_ENCODE_FUNC3(name, func) \
        inline geode::Result<geode::ByteVector> name(DecodedAnimation const& anim, float quality = 75.f) { \
            auto table = __detail::getFunctionTable(); \
            if (!table || !table->func) \
                return geode::Err("ImagePlus is not available"); \
            return table->func(anim, quality); \
        }

    namespace formats {
        /// @brief Checks whether the data buffer contains valid JPEG magic
        /// @param data Pointer to the buffer
        /// @param size Size of the buffer to avoid out-of-bounds reads
        IMAGE_PLUS_GEN_CHECK_FUNC(isJpeg)

        /// @brief Checks whether the data buffer contains valid APNG magic
        /// @param data Pointer to the buffer
        /// @param size Size of the buffer to avoid out-of-bounds reads
        IMAGE_PLUS_GEN_CHECK_FUNC(isAPng)

        /// @brief Checks whether the data buffer contains valid PNG magic
        /// @param data Pointer to the buffer
        /// @param size Size of the buffer to avoid out-of-bounds reads
        IMAGE_PLUS_GEN_CHECK_FUNC(isPng)

        /// @brief Checks whether the data buffer contains valid GIF magic
        /// @param data Pointer to the buffer
        /// @param size Size of the buffer to avoid out-of-bounds reads
        IMAGE_PLUS_GEN_CHECK_FUNC(isGif)

        /// @brief Checks whether the data buffer contains valid WEBP magic
        /// @param data Pointer to the buffer
        /// @param size Size of the buffer to avoid out-of-bounds reads
        IMAGE_PLUS_GEN_CHECK_FUNC(isWebp)

        /// @brief Checks whether the data buffer contains valid TIFF magic
        /// @param data Pointer to the buffer
        /// @param size Size of the buffer to avoid out-of-bounds reads
        IMAGE_PLUS_GEN_CHECK_FUNC(isTiff)

        /// @brief Checks whether the data buffer contains valid QOI magic
        /// @param data Pointer to the buffer
        /// @param size Size of the buffer to avoid out-of-bounds reads
        IMAGE_PLUS_GEN_CHECK_FUNC(isQoi)

        /// @brief Checks whether the data buffer contains valid JXL magic
        /// @param data Pointer to the buffer
        /// @param size Size of the buffer to avoid out-of-bounds reads
        IMAGE_PLUS_GEN_CHECK_FUNC(isJpegXL)
    }

    namespace decode {
        // == Static Images == //

        /// @brief Decodes a PNG image and returns the decoded image data
        /// @note User is responsible for freeing the image data
        /// @param data Pointer to the image data
        /// @param size Size of the image data
        /// @return Result containing the decoded image or an error message
        IMAGE_PLUS_GEN_DECODE_FUNC1(png, decodePng)

        /// @brief Decodes a QOI image and returns the decoded image data
        /// @note User is responsible for freeing the image data
        /// @param data Pointer to the image data
        /// @param size Size of the image data
        /// @return Result containing the decoded image or an error message
        IMAGE_PLUS_GEN_DECODE_FUNC1(qoi, decodeQoi)

        // == Animated Images == //

        /// @brief Decodes a JPEG XL image and returns either a single frame or an animation
        /// @note User is responsible for freeing the image data (if single frame)
        /// @param data Pointer to the image data
        /// @param size Size of the image data
        /// @return Result containing the decoded image or an error message
        IMAGE_PLUS_GEN_DECODE_FUNC2(jpegxl, decodeJpegXL)

        /// @brief Decodes a WEBP image and returns either a single frame or an animation
        /// @note User is responsible for freeing the image data (if single frame)
        /// @param data Pointer to the image data
        /// @param size Size of the image data
        /// @return Result containing the decoded image or an error message
        IMAGE_PLUS_GEN_DECODE_FUNC2(webp, decodeWebp)

        /// @brief Decodes a GIF image and returns either a single frame or an animation
        /// @note User is responsible for freeing the image data (if single frame)
        /// @param data Pointer to the image data
        /// @param size Size of the image data
        /// @return Result containing the decoded image or an error message
        IMAGE_PLUS_GEN_DECODE_FUNC2(gif, decodeGif)
    }

    namespace encode {
        /// @brief Encodes a PNG image from raw pixel data
        /// @param image Pointer to the raw pixel data
        /// @param width Width of the image
        /// @param height Height of the image
        /// @param hasAlpha Whether the image has an alpha channel (default is true)
        /// @return Result containing the encoded image data or an error message
        IMAGE_PLUS_GEN_ENCODE_FUNC1(png, encodePng)

        /// @brief Encodes a QOI image from raw pixel data
        /// @param image Pointer to the raw pixel data
        /// @param width Width of the image
        /// @param height Height of the image
        /// @param hasAlpha Whether the image has an alpha channel (default is true)
        /// @return Result containing the encoded image data or an error message
        IMAGE_PLUS_GEN_ENCODE_FUNC1(qoi, encodeQoi)

        /// @brief Encodes a WEBP image from raw pixel data
        /// @param image Pointer to the raw pixel data
        /// @param width Width of the image
        /// @param height Height of the image
        /// @param hasAlpha Whether the image has an alpha channel (default is true)
        /// @param quality Quality of the encoding (default is 75.f)
        /// @return Result containing the encoded image data or an error message
        IMAGE_PLUS_GEN_ENCODE_FUNC2(webp, encodeWebp)

        /// @brief Encodes a JPEG XL image from raw pixel data
        /// @param image Pointer to the raw pixel data
        /// @param width Width of the image
        /// @param height Height of the image
        /// @param hasAlpha Whether the image has an alpha channel (default is true)
        /// @param quality Quality of the encoding (default is 75.f)
        /// @return Result containing the encoded image data or an error message
        IMAGE_PLUS_GEN_ENCODE_FUNC2(jpegxl, encodeJpegXL)

        /// @brief Encodes a WEBP animation from a DecodedAnimation
        /// @param anim The animation to encode
        /// @param quality The quality of the encoding (default is 75.f)
        /// @return Result containing the encoded animation data or an error message
        IMAGE_PLUS_GEN_ENCODE_FUNC3(webp, encodeWebpAnim)

        /// @brief Encodes a JPEG XL animation from a DecodedAnimation
        /// @param anim The animation to encode
        /// @param quality The quality of the encoding (default is 75.f)
        /// @return Result containing the encoded animation data or an error message
        IMAGE_PLUS_GEN_ENCODE_FUNC3(jpegxl, encodeJpegXLAnim)
    }

    #undef IMAGE_PLUS_GEN_CHECK_FUNC
    #undef IMAGE_PLUS_GEN_DECODE_FUNC1
    #undef IMAGE_PLUS_GEN_DECODE_FUNC2
    #undef IMAGE_PLUS_GEN_ENCODE_FUNC1
    #undef IMAGE_PLUS_GEN_ENCODE_FUNC2
    #undef IMAGE_PLUS_GEN_ENCODE_FUNC3

    class AnimatedSprite : public cocos2d::CCSprite {
    public:
        /// @brief Creates a new CCSprite and casts it to an AnimatedSprite
        static GEODE_INLINE AnimatedSprite* create(char const* path) {
            return static_cast<AnimatedSprite*>(CCSprite::create(path));
        }

        /// @brief Casts a generic CCSprite to an AnimatedSprite
        static GEODE_INLINE AnimatedSprite* from(CCSprite* sprite) {
            return static_cast<AnimatedSprite*>(sprite);
        }

        /// @brief Check if the sprite holds an animated texture
        bool isAnimated() {
            auto table = __detail::getFunctionTable();
            if (!table || !table->AnimatedSprite_isAnimated)
                return false;
            return (this->*table->AnimatedSprite_isAnimated)();
        }

        /// @brief Stops the animation
        void stop() {
            auto table = __detail::getFunctionTable();
            if (!table || !table->AnimatedSprite_stop)
                return;
            (this->*table->AnimatedSprite_stop)();
        }

        /// @brief Pauses the animation
        void pause() {
            auto table = __detail::getFunctionTable();
            if (!table || !table->AnimatedSprite_pause)
                return;
            (this->*table->AnimatedSprite_pause)();
        }

        /// @brief Plays the animation
        void play() {
            auto table = __detail::getFunctionTable();
            if (!table || !table->AnimatedSprite_play)
                return;
            (this->*table->AnimatedSprite_play)();
        }

        /// @brief Checks if the animation is paused
        bool isPaused() {
            auto table = __detail::getFunctionTable();
            if (!table || !table->AnimatedSprite_isPaused)
                return false;
            return (this->*table->AnimatedSprite_isPaused)();
        }

        /// @brief Sets the playback speed of the animation
        /// @param speed The speed multiplier (1.0 is default, 0.5 is half speed, 2.0 is double speed)
        /// @note Supports negative values for reverse playback
        void setPlaybackSpeed(float speed) {
            auto table = __detail::getFunctionTable();
            if (!table || !table->AnimatedSprite_setPlaybackSpeed)
                return;
            (this->*table->AnimatedSprite_setPlaybackSpeed)(speed);
        }

        /// @brief Gets the current playback speed of the animation
        float getPlaybackSpeed() {
            auto table = __detail::getFunctionTable();
            if (!table || !table->AnimatedSprite_getPlaybackSpeed)
                return 1.0f;
            return (this->*table->AnimatedSprite_getPlaybackSpeed)();
        }

        /// @brief Sets whether the animation should loop
        /// @param forceLoop If true, the animation will always loop, if false it will stop after the last frame.
        /// Pass std::nullopt to use the default behavior
        void setForceLoop(std::optional<bool> forceLoop) {
            auto table = __detail::getFunctionTable();
            if (!table || !table->AnimatedSprite_setForceLoop)
                return;
            (this->*table->AnimatedSprite_setForceLoop)(forceLoop);
        }

        /// @brief Gets whether the animation is set to loop
        std::optional<bool> getForceLoop() {
            auto table = __detail::getFunctionTable();
            if (!table || !table->AnimatedSprite_getForceLoop)
                return std::nullopt;
            return (this->*table->AnimatedSprite_getForceLoop)();
        }

        /// @brief Gets the current frame index of the animation
        uint32_t getCurrentFrame() {
            auto table = __detail::getFunctionTable();
            if (!table || !table->AnimatedSprite_getCurrentFrame)
                return 0;
            return (this->*table->AnimatedSprite_getCurrentFrame)();
        }

        /// @brief Sets the current frame index of the animation
        void setCurrentFrame(uint32_t frame) {
            auto table = __detail::getFunctionTable();
            if (!table || !table->AnimatedSprite_setCurrentFrame)
                return;
            (this->*table->AnimatedSprite_setCurrentFrame)(frame);
        }

        /// @brief Gets the total number of frames in the animation
        size_t getFrameCount() {
            auto table = __detail::getFunctionTable();
            if (!table || !table->AnimatedSprite_getFrameCount)
                return 0;
            return (this->*table->AnimatedSprite_getFrameCount)();
        }
    };
}

#endif // IMAGE_PLUS_EVENTS_HPP
