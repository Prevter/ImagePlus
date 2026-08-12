#include <api.hpp>

#import <CoreFoundation/CoreFoundation.h>
#import <ImageIO/ImageIO.h>
#import <CoreGraphics/CoreGraphics.h>

#include "../FakeVector.hpp"
#include "../Utils.hpp"

using namespace geode;

IMAGE_PLUS_BEGIN_NAMESPACE
namespace decode {
    static Result<CGImageRef> createCGImageFromBuffer(void const* data, size_t size) {
        if (!data || size == 0) {
            return Err("Invalid buffer data");
        }

        CFDataRef rawData = CFDataCreateWithBytesNoCopy(
            kCFAllocatorDefault,
            static_cast<UInt8 const*>(data),
            size, kCFAllocatorNull
        );
        if (!rawData) {
            return Err("Failed to create CFData wrapper");
        }

        CGImageSourceRef source = CGImageSourceCreateWithData(rawData, nullptr);
        CFRelease(rawData);
        if (!source) {
            return Err("Failed to create CGImageSource from CgBI data");
        }

        CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
        CFRelease(source);
        if (!image) {
            return Err("Failed to extract CGImage from CgBI source");
        }

        return Ok(image);
    }

    Result<DecodedImage> cgbi(void const* data, size_t size) {
        GEODE_UNWRAP_INTO(CGImageRef image, createCGImageFromBuffer(data, size));

        size_t width = CGImageGetWidth(image);
        size_t height = CGImageGetHeight(image);
        size_t totalSize = width * height * 4;

        auto output = util::make_unique(totalSize);
        if (!output) {
            CGImageRelease(image);
            return Err("Failed to allocate memory for decoded CgBI image");
        }

        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

        CGContextRef context = CGBitmapContextCreate(
            output.get(),
            width, height, 8,
            width * 4, colorSpace,
            static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)
        );

        CGColorSpaceRelease(colorSpace);
        if (!context) {
            CGImageRelease(image);
            return Err("Failed to create CGBitmapContext");
        }

        CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
        CGContextRelease(context);
        CGImageRelease(image);

        return Ok(DecodedImage{
            .data = std::move(output),
            .width = static_cast<uint16_t>(width),
            .height = static_cast<uint16_t>(height),
            .bit_depth = 8,
            .hasAlpha = true,
            .isPreMultiplied = true
        });
    }
}
IMAGE_PLUS_END_NAMESPACE