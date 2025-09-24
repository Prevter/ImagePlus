#include <Geode/modify/CCImage.hpp>
#include "../StateManager.hpp"

using namespace geode::prelude;
using namespace imgp;

#define SCALAR_PREMUL_PIXEL(r, g, b, a) \
    (((uint32_t)((r) * (a) / 255) << 0) | \
     ((uint32_t)((g) * (a) / 255) << 8) | \
     ((uint32_t)((b) * (a) / 255) << 16) | \
     (uint32_t)(a << 24))

class ImagePlusImage : public CCImage {
public:
    ~ImagePlusImage() override {
        if (StateManager::get().onImageRemoval(this)) {
            m_pData = nullptr; // clear the data to avoid double-free
        }
    }

    static void** getVTable() {
        static void** vtable = []() {
            ImagePlusImage img{};
            return *reinterpret_cast<void***>(&img);
        }();
        return vtable;
    }

    static std::shared_ptr<DecodedAnimation>& hook(CCImage* self) {
        *reinterpret_cast<void***>(self) = getVTable();
        return StateManager::get().getImageStorage(self);
    }
};

class $modify(ImagePlusImageHook, CCImage) {
    // static void onModify(auto& self) {
    //     (void)self.setHookPriority("cocos2d::CCImage::initWithImageData", -1000);
    // }

    bool initFromDecodeResult(DecodedImage&& result) {
        if (!result) return false;

        m_nWidth = result.width;
        m_nHeight = result.height;
        m_bHasAlpha = result.hasAlpha;
        m_nBitsPerComponent = result.bit_depth;
        m_bPreMulti = result.hasAlpha;
        m_pData = result.data.release(); // take ownership of the data

        // premultiply alpha if needed
        if (m_bPreMulti) {
            size_t imageSize = m_nWidth * m_nHeight * 4;
            size_t iters = imageSize / sizeof(uint32_t);

            for (size_t i = 0; i < iters; i++) {
                uint8_t const* pixel = &static_cast<uint8_t const*>(m_pData)[4 * i];
                uint32_t* outpixel = &reinterpret_cast<uint32_t*>(m_pData)[i];

                *outpixel = SCALAR_PREMUL_PIXEL(pixel[0], pixel[1], pixel[2], pixel[3]);
            }
        }

        return true;
    }

    bool initFromDecodeResult(DecodedResult&& result) {
        if (std::holds_alternative<DecodedImage>(result)) {
            return initFromDecodeResult(std::move(std::get<DecodedImage>(result)));
        }

        auto& anim = std::get<DecodedAnimation>(result);
        if (anim.frames.empty()) {
            return false;
        }

        m_nWidth = anim.width;
        m_nHeight = anim.height;
        m_pData = anim.frames[0].data.get();
        m_nBitsPerComponent = 8; // assuming 8 bits per channel for animations
        m_bHasAlpha = anim.hasAlpha;
        m_bPreMulti = false;

        ImagePlusImage::hook(this) = std::make_shared<DecodedAnimation>(std::move(anim));

        return true;
    }

    #define TRY_FROM_DECODE_RESULT(decodeFunc) { \
            auto result = decodeFunc(data, size); \
            if (result.isOk()) { \
                return initFromDecodeResult(std::move(result).unwrap()); \
            } else { log::warn("{}", result.unwrapErr()); } \
            break; \
        }

    bool initWithImageFile(char const* path, EImageFormat fmt) {
        unsigned long size = 0;
        std::string fullPath = CCFileUtils::get()->fullPathForFilename(path, false);
        std::unique_ptr<uint8_t[]> data(
            CCFileUtils::get()->getFileData(fullPath.c_str(), "rb", &size)
        );

        if (!data || size == 0) {
            return false;
        }

        return initWithImageData(data.get(), size, fmt, 0, 0, 8, 0);
    }

    bool initWithImageData(void* data, int size, EImageFormat fmt, int width, int height, int bpc, int whoKnows) {
        static bool alwaysGuess = (
            listenForSettingChanges<bool>("force-autodetect", [](bool val) { alwaysGuess = val; }),
            getMod()->getSettingValue<bool>("force-autodetect")
        );

        // hi texture workshop <3
        if (!data || size <= 0) {
            return false;
        }

        if (fmt != kFmtRawData && (fmt == kFmtUnKnown || alwaysGuess)) {
            fmt = +guessFormat(data, size);
        }

        switch (static_cast<ImageFormat>(fmt)) {
            case ImageFormat::Png: {
                static bool disablePng = (
                    listenForSettingChanges<bool>("disable-png", [](bool val) { disablePng = val; }),
                    getMod()->getSettingValue<bool>("disable-png")
                );

                if (disablePng) break;

                TRY_FROM_DECODE_RESULT(decode::png);
            }
            case ImageFormat::Qoi: TRY_FROM_DECODE_RESULT(decode::qoi);
            case ImageFormat::Webp: TRY_FROM_DECODE_RESULT(decode::webp);
            case ImageFormat::JpegXL: TRY_FROM_DECODE_RESULT(decode::jpegxl);
            case ImageFormat::Gif: TRY_FROM_DECODE_RESULT(decode::gif);
            // case ImageFormat::Avif: return initFromDecodeResult(decode::avif(data, size));
            // case ImageFormat::Heif: return initFromDecodeResult(decode::heif(data, size));
            // case ImageFormat::APng: return initFromDecodeResult(decode::apng(data, size));
            // case ImageFormat::Tiff: return initFromDecodeResult(decode::tiff(data, size));
            // case ImageFormat::Jpg: return initFromDecodeResult(decode::jpeg(data, size));
            default: break;
        }

        return CCImage::initWithImageData(data, size, fmt, width, height, bpc, whoKnows);
    }
};
