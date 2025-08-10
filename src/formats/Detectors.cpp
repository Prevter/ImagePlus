#include <api.hpp>

IMAGE_PLUS_BEGIN_NAMESPACE
    namespace formats {
        template <size_t N>
        constexpr bool matchMagic(void const* data, size_t size, std::array<uint8_t, N> const& magic, size_t offset = 0) {
            return size >= offset + N && std::memcmp(static_cast<uint8_t const*>(data) + offset, magic.data(), N) == 0;
        }

        bool isJpeg(void const* data, size_t size) {
            constexpr std::array<uint8_t, 3> sig = {0xFF, 0xD8, 0xFF};
            return matchMagic(data, size, sig);
        }

        static bool checkAPngChunk(void const* data, size_t size) {
            // APNG uses PNG signature, but has a specific chunk with type "acTL"
            // so we can look for all chunks before we hit "IDAT" chunk
            constexpr std::array<uint8_t, 4> acTL = {'a', 'c', 'T', 'L'};
            constexpr std::array<uint8_t, 4> idat = {'I', 'D', 'A', 'T'};
            size_t offset = 8;
            while (offset + 8 <= size) {
                // Read chunk length
                uint32_t length = 0;
                std::memcpy(&length, static_cast<const uint8_t*>(data) + offset, 4);
                length = ntohl(length);

                // Check for acTL chunk
                if (matchMagic(static_cast<const uint8_t*>(data) + offset + 4, size - offset - 4, acTL)) {
                    return true;
                }

                // Check for IDAT chunk
                if (matchMagic(static_cast<const uint8_t*>(data) + offset + 4, size - offset - 4, idat)) {
                    return false;
                }

                // Move to the next chunk
                offset += 8 + length;
            }
            return false;
        }

        bool isAPng(void const* data, size_t size) {
            if (!isPng(data, size)) return false;
            return checkAPngChunk(data, size);
        }

        bool isPng(void const* data, size_t size) {
            constexpr std::array<uint8_t, 8> sig = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
            return matchMagic(data, size, sig);
        }

        bool isGif(void const* data, size_t size) {
            constexpr std::array<uint8_t, 4> sig = {'G', 'I', 'F', '8'};
            return matchMagic(data, size, sig);
        }

        bool isWebp(void const* data, size_t size) {
            constexpr std::array<uint8_t, 4> riffSig = {'R', 'I', 'F', 'F'};
            constexpr std::array<uint8_t, 4> webpSig = {'W', 'E', 'B', 'P'};
            return matchMagic(data, size, riffSig, 0) &&
                   matchMagic(data, size, webpSig, 8);
        }

        bool isTiff(void const* data, size_t size) {
            constexpr std::array<uint8_t, 4> le = {'I', 'I', 0x2A, 0x00};
            constexpr std::array<uint8_t, 4> be = {'M', 'M', 0x00, 0x2A};
            return matchMagic(data, size, le) || matchMagic(data, size, be);
        }

        bool isQoi(void const* data, size_t size) {
            constexpr std::array<uint8_t, 4> sig = {'q', 'o', 'i', 'f'};
            return matchMagic(data, size, sig);
        }

        bool isJpegXL(void const* data, size_t size) {
            constexpr std::array<uint8_t, 12> sig = {
                0x00, 0x00, 0x00, 0x0C, 'J', 'X', 'L', ' ',
                0x0D, 0x0A, 0x87, 0x0A
            };
            constexpr std::array<uint8_t, 2> sig2 = {0xFF, 0x0A};
            return matchMagic(data, size, sig) || matchMagic(data, size, sig2);
        }
    }

    ImageFormat guessFormat(void const* data, size_t size) {
        using namespace formats;
        if (isPng(data, size)) return ImageFormat::Png;
        if (isJpeg(data, size)) return ImageFormat::Jpg;
        if (isWebp(data, size)) return ImageFormat::Webp;
        if (isGif(data, size)) return ImageFormat::Gif;
        if (isQoi(data, size)) return ImageFormat::Qoi;
        if (isJpegXL(data, size)) return ImageFormat::JpegXL;
        if (isTiff(data, size)) return ImageFormat::Tiff;

        return ImageFormat::Unknown;
    }

    geode::Result<DecodedResult> tryDecode(void const* data, size_t size, ImageFormat format) {
        using namespace formats;

        if (format == ImageFormat::Unknown) {
            format = guessFormat(data, size);
        }

        #define FLATTEN_DECODED_IMAGE_RESULT(func) { \
            GEODE_UNWRAP_INTO(auto img, func(data, size)); \
            return geode::Ok(DecodedResult{std::move(img)}); \
        }

        switch (format) {
            case ImageFormat::Png: FLATTEN_DECODED_IMAGE_RESULT(decode::png);
            case ImageFormat::Qoi: FLATTEN_DECODED_IMAGE_RESULT(decode::qoi);
            case ImageFormat::Webp: return decode::webp(data, size);
            case ImageFormat::JpegXL: return decode::jpegxl(data, size);
            case ImageFormat::Gif: return decode::gif(data, size);
            default:
                return geode::Err("Unsupported image format");
        }
    }

IMAGE_PLUS_END_NAMESPACE
