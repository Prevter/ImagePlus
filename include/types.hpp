#pragma once
#ifndef IMAGE_PLUS_TYPES_HPP
#define IMAGE_PLUS_TYPES_HPP

#include <Geode/cocos/platform/CCImage.h>

#include <cstdint>
#include <memory>
#include <string_view>
#include <variant>
#include <vector>

namespace imgp {
    /// @brief Enum representing various image formats supported by ImagePlus.
    /// It's an extension of cocos2d::CCImage::EImageFormat
    enum class ImageFormat {
        Jpg     = 0,
        Png     = 1,
        Tiff    = 2,
        Webp    = 3,
        RawData = 4,
        Unknown = 5,

        // extended
        Gif    = 6,
        Qoi    = 7,
        JpegXL = 8,
        CgBI   = 9, ///< @note Only available on iOS
    };

    /// @brief Helper converter to turn imgp::ImageFormat into cocos2d enum
    constexpr cocos2d::CCImage::EImageFormat operator+(ImageFormat val) {
        switch (val) {
            case ImageFormat::Jpg: return cocos2d::CCImage::kFmtJpg;
            case ImageFormat::Png: return cocos2d::CCImage::kFmtPng;
            case ImageFormat::Tiff: return cocos2d::CCImage::kFmtTiff;
            case ImageFormat::Webp: return cocos2d::CCImage::kFmtWebp;
            case ImageFormat::RawData: return cocos2d::CCImage::kFmtRawData;
            case ImageFormat::CgBI: return cocos2d::CCImage::kFmtPng;
            default: return cocos2d::CCImage::kFmtUnKnown;
        }
    }

    /// @brief Container for decoded image data
    struct DecodedImage {
        std::unique_ptr<uint8_t[]> data = nullptr;
        uint16_t width = 0;
        uint16_t height = 0;
        uint8_t bit_depth = 8;
        bool hasAlpha = false;
        bool isPreMultiplied = false; // used to skip manual premultiplication during load (e.g. for CgBI)

        operator bool() const { return data.get(); }
    };

    /// @brief Represents a single frame in an animation
    struct AnimationFrame {
        std::unique_ptr<uint8_t[]> data = nullptr;
        uint32_t delay = 0; // in milliseconds
    };

    /// @brief Container for decoded animation data
    struct DecodedAnimation {
        std::vector<AnimationFrame> frames;
        uint16_t loopCount = 0; // 0 means infinite
        uint16_t width = 0;
        uint16_t height = 0;
        bool hasAlpha = false;
    };

    /// @brief Result type that can hold either a decoded image or a decoded animation
    using DecodedResult = std::variant<DecodedImage, DecodedAnimation>;

    inline std::string_view format_as(ImageFormat fmt) {
        switch (fmt) {
            case ImageFormat::Jpg:     return "jpg";
            case ImageFormat::Png:     return "png";
            case ImageFormat::Tiff:    return "tiff";
            case ImageFormat::Webp:    return "webp";
            case ImageFormat::Gif:     return "gif";
            case ImageFormat::Qoi:     return "qoi";
            case ImageFormat::JpegXL:  return "jxl";
            case ImageFormat::CgBI:    return "cgbi";
            default:                   return "unknown";
        }
    }
}

#endif // IMAGE_PLUS_TYPES_HPP
