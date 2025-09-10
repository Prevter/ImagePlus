#pragma once
#ifndef IMAGE_PLUS_API_HPP
#define IMAGE_PLUS_API_HPP

#include <Geode/Result.hpp>
#include <Geode/platform/cplatform.h>
#include "types.hpp"

#include <cstddef>

#ifdef GEODE_IS_WINDOWS
    #ifdef IMAGE_PLUS_EXPORTING
        #define IMAGE_PLUS_DLL __declspec(dllexport)
    #else
        #define IMAGE_PLUS_DLL __declspec(dllimport)
    #endif
#else
    #define IMAGE_PLUS_DLL __attribute__((visibility("default")))
#endif

// trick to avoid circular includes
#ifdef IMAGE_PLUS_USE_EVENTS
    #define IMAGE_PLUS_BEGIN_NAMESPACE namespace imgp { namespace v1 {
#else
    #define IMAGE_PLUS_BEGIN_NAMESPACE namespace imgp { inline namespace v1 {
#endif
#define IMAGE_PLUS_END_NAMESPACE }}

IMAGE_PLUS_BEGIN_NAMESPACE
    namespace formats {
        /// @brief Checks whether the data buffer contains valid JPEG magic
        /// @param data Pointer to the buffer
        /// @param size Size of the buffer to avoid out-of-bounds reads
        bool IMAGE_PLUS_DLL isJpeg(void const* data, size_t size);

        /// @brief Checks whether the data buffer contains valid APNG magic
        /// @param data Pointer to the buffer
        /// @param size Size of the buffer to avoid out-of-bounds reads
        bool IMAGE_PLUS_DLL isAPng(void const* data, size_t size);

        /// @brief Checks whether the data buffer contains valid PNG magic
        /// @param data Pointer to the buffer
        /// @param size Size of the buffer to avoid out-of-bounds reads
        bool IMAGE_PLUS_DLL isPng(void const* data, size_t size);

        /// @brief Checks whether the data buffer contains valid GIF magic
        /// @param data Pointer to the buffer
        /// @param size Size of the buffer to avoid out-of-bounds reads
        bool IMAGE_PLUS_DLL isGif(void const* data, size_t size);

        /// @brief Checks whether the data buffer contains valid WebP magic
        /// @param data Pointer to the buffer
        /// @param size Size of the buffer to avoid out-of-bounds reads
        bool IMAGE_PLUS_DLL isWebp(void const* data, size_t size);

        /// @brief Checks whether the data buffer contains valid TIFF magic
        /// @param data Pointer to the buffer
        /// @param size Size of the buffer to avoid out-of-bounds reads
        bool IMAGE_PLUS_DLL isTiff(void const* data, size_t size);

        /// @brief Checks whether the data buffer contains valid QOI magic
        /// @param data Pointer to the buffer
        /// @param size Size of the buffer to avoid out-of-bounds reads
        bool IMAGE_PLUS_DLL isQoi(void const* data, size_t size);

        /// @brief Checks whether the data buffer contains valid JPEGXL magic
        /// @param data Pointer to the buffer
        /// @param size Size of the buffer to avoid out-of-bounds reads
        bool IMAGE_PLUS_DLL isJpegXL(void const* data, size_t size);
    }

    namespace decode {
        // == Static Images == //

        /// @brief Decodes a PNG image and returns the decoded image data
        /// @note User is responsible for freeing the image data
        /// @param data Pointer to the image data
        /// @param size Size of the image data
        /// @return Result containing the decoded image or an error message
        geode::Result<DecodedImage> IMAGE_PLUS_DLL png(void const* data, size_t size);

        /// @brief Decodes a QOI image and returns the decoded image data
        /// @note User is responsible for freeing the image data
        /// @param data Pointer to the image data
        /// @param size Size of the image data
        /// @return Result containing the decoded image or an error message
        geode::Result<DecodedImage> IMAGE_PLUS_DLL qoi(void const* data, size_t size);

        // == Animated Images == //

        /// @brief Decodes a JPEG XL image and returns either a single frame or an animation
        /// @note User is responsible for freeing the image data (if single frame)
        /// @param data Pointer to the image data
        /// @param size Size of the image data
        /// @return Result containing the decoded image or an error message
        geode::Result<DecodedResult> IMAGE_PLUS_DLL jpegxl(void const* data, size_t size);

        /// @brief Decodes a WEBP image and returns either a single frame or an animation
        /// @note User is responsible for freeing the image data (if single frame)
        /// @param data Pointer to the image data
        /// @param size Size of the image data
        /// @return Result containing the decoded image or an error message
        geode::Result<DecodedResult> IMAGE_PLUS_DLL webp(void const* data, size_t size);

        /// @brief Decodes a GIF image and returns either a single frame or an animation
        /// @note User is responsible for freeing the image data (if single frame)
        /// @param data Pointer to the image data
        /// @param size Size of the image data
        /// @return Result containing the decoded image or an error message
        geode::Result<DecodedResult> IMAGE_PLUS_DLL gif(void const* data, size_t size);
    }

    namespace encode {
        /// @brief Encodes a PNG image from raw pixel data
        /// @param image Pointer to the raw pixel data
        /// @param width Width of the image
        /// @param height Height of the image
        /// @param hasAlpha Whether the image has an alpha channel (default is true)
        /// @return Result containing the encoded image data or an error message
        geode::Result<geode::ByteVector> IMAGE_PLUS_DLL png(
            void const* image, uint16_t width, uint16_t height, bool hasAlpha = true
        );

        /// @brief Encodes a QOI image from raw pixel data
        /// @param image Pointer to the raw pixel data
        /// @param width Width of the image
        /// @param height Height of the image
        /// @param hasAlpha Whether the image has an alpha channel (default is true)
        /// @return Result containing the encoded image data or an error message
        geode::Result<geode::ByteVector> IMAGE_PLUS_DLL qoi(
            void const* image, uint16_t width, uint16_t height, bool hasAlpha = true
        );

        /// @brief Encodes a WEBP image from raw pixel data
        /// @param image Pointer to the raw pixel data
        /// @param width Width of the image
        /// @param height Height of the image
        /// @param hasAlpha Whether the image has an alpha channel (default is true)
        /// @param quality Quality of the encoding (default is 75.f)
        /// @return Result containing the encoded image data or an error message
        geode::Result<geode::ByteVector> IMAGE_PLUS_DLL webp(
            void const* image, uint16_t width, uint16_t height, bool hasAlpha = true, float quality = 75.f
        );

        /// @brief Encodes a JPEG XL image from raw pixel data
        /// @param image Pointer to the raw pixel data
        /// @param width Width of the image
        /// @param height Height of the image
        /// @param hasAlpha Whether the image has an alpha channel (default is true)
        /// @param quality Quality of the encoding (default is 75.f)
        /// @return Result containing the encoded image data or an error message
        geode::Result<geode::ByteVector> IMAGE_PLUS_DLL jpegxl(
            void const* image, uint16_t width, uint16_t height, bool hasAlpha = true, float quality = 75.f
        );

        /// @brief Encodes a WEBP animation from a DecodedAnimation
        /// @param anim The animation to encode
        /// @param quality The quality of the encoding (default is 75.f)
        /// @return Result containing the encoded animation data or an error message
        geode::Result<geode::ByteVector> IMAGE_PLUS_DLL webp(DecodedAnimation const& anim, float quality = 75.f);

        /// @brief Encodes a JPEG XL animation from a DecodedAnimation
        /// @param anim The animation to encode
        /// @param quality The quality of the encoding (default is 75.f)
        /// @return Result containing the encoded animation data or an error message
        geode::Result<geode::ByteVector> IMAGE_PLUS_DLL jpegxl(DecodedAnimation const& anim, float quality = 75.f);
    }

    /// @brief Attempts to guess the image format based on the header data
    /// @param data Pointer to the image data
    /// @param size Size of the image data
    /// @return The guessed image format, or ImageFormat::Unknown if it cannot be determined
    ImageFormat IMAGE_PLUS_DLL guessFormat(void const* data, size_t size);

    /// @brief Decodes an image from raw data with provided format
    /// @param data Pointer to the image data
    /// @param size Size of the image data
    /// @param format The format of the image data, defaults to ImageFormat::Unknown (auto-detect)
    /// @return Result containing the decoded image or an error message
    geode::Result<DecodedResult> IMAGE_PLUS_DLL tryDecode(
        void const* data, size_t size, ImageFormat format = ImageFormat::Unknown
    );

    /// @brief Thin wrapper for calling extension functions on animated sprites
    /// @note AnimatedSprite is not actually used, so typeinfo_cast will never show it.
    /// To check if a CCSprite supports animations, use `isAnimated()` method.
    class IMAGE_PLUS_DLL AnimatedSprite : public cocos2d::CCSprite {
    public:
        /// @brief Creates a new CCSprite and casts it to an AnimatedSprite
        inline static GEODE_INLINE AnimatedSprite* create(char const* path) {
            return static_cast<AnimatedSprite*>(CCSprite::create(path));
        }

        /// @brief Casts a generic CCSprite to an AnimatedSprite
        inline static GEODE_INLINE AnimatedSprite* from(CCSprite* sprite) {
            return static_cast<AnimatedSprite*>(sprite);
        }

        /// @brief Check if the sprite holds an animated texture
        bool isAnimated();

        /// @brief Stops the animation and resets to the first frame
        void stop();

        /// @brief Pauses the animation
        void pause();

        /// @brief Resumes the animation
        void play();

        /// @brief Checks if the animation is currently paused
        bool isPaused();

        /// @brief Sets the playback speed of the animation
        /// @param speed The speed multiplier (1.0 is default, 0.5 is half speed, 2.0 is double speed)
        /// @note Supports negative values for reverse playback
        void setPlaybackSpeed(float speed);

        /// @brief Gets the current playback speed of the animation
        float getPlaybackSpeed();

        /// @brief Sets whether the animation should loop
        /// @param forceLoop If true, the animation will always loop, if false it will stop after the last frame.
        /// Pass std::nullopt to use the default behavior
        void setForceLoop(std::optional<bool> forceLoop);

        /// @brief Gets whether the animation is set to loop
        std::optional<bool> getForceLoop();

        /// @brief Gets the current frame index of the animation
        uint32_t getCurrentFrame();

        /// @brief Sets the current frame index of the animation
        void setCurrentFrame(uint32_t frame);

        /// @brief Gets the total number of frames in the animation
        size_t getFrameCount();
    };

IMAGE_PLUS_END_NAMESPACE

#endif // IMAGE_PLUS_API_HPP
