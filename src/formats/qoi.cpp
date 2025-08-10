#include <api.hpp>
#include "../FakeVector.hpp"

#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO
#include <qoi.h>

using namespace geode;

IMAGE_PLUS_BEGIN_NAMESPACE
namespace decode {
    Result<DecodedImage> qoi(void const* data, size_t size) {
        qoi_desc desc;
        auto* output = static_cast<uint8_t*>(qoi_decode(data, size, &desc, 0));
        if (!output) {
            return Err("Failed to decode QOI image");
        }

        return Ok(DecodedImage{
            .data = std::unique_ptr<uint8_t[]>(output),
            .width = static_cast<uint16_t>(desc.width),
            .height = static_cast<uint16_t>(desc.height),
            .bit_depth = 8,
            .hasAlpha = desc.channels == 4,
        });
    }
}

namespace encode {
    Result<ByteVector> qoi(void const* image, uint16_t width, uint16_t height, bool hasAlpha) {
        if (!image) {
            return Err("Invalid image data");
        }

        qoi_desc desc = {};
        desc.width = width;
        desc.height = height;
        desc.channels = hasAlpha ? 4 : 3;
        desc.colorspace = QOI_SRGB;

        int size;
        auto* encoded = qoi_encode(image, &desc, &size);
        if (!encoded) {
            return Err("Failed to encode QOI image");
        }

        return Ok(FakeVector(encoded, size));
    }
}
IMAGE_PLUS_END_NAMESPACE