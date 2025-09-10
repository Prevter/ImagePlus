#include <api.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_GIF
#include <stb_image.h>

#include <memory>
#include <vector>

using namespace geode;

IMAGE_PLUS_BEGIN_NAMESPACE
namespace decode {
    Result<DecodedResult> gif(void const* data, size_t size) {
        int* delays = nullptr;
        int x, y, n, frames;

        uint8_t* raw = stbi_load_gif_from_memory(
            static_cast<stbi_uc const*>(data),
            static_cast<int>(size),
            &delays, &x, &y, &frames, &n, 4
        );

        if (!raw) {
            return Err(fmt::format("Failed to decode GIF: {}", stbi_failure_reason()));
        }

        if (frames <= 0) {
            STBI_FREE(raw);
            STBI_FREE(delays);
            return Err("No frames found in GIF");
        }

        if (frames == 1) {
            DecodedImage img;
            img.width  = static_cast<uint16_t>(x);
            img.height = static_cast<uint16_t>(y);
            img.hasAlpha = true;
            size_t imageSize = static_cast<size_t>(x) * y * 4;
            img.data = std::make_unique<uint8_t[]>(imageSize);
            std::memcpy(img.data.get(), raw, imageSize);

            STBI_FREE(raw);
            STBI_FREE(delays);

            return Ok(DecodedResult{std::move(img)});
        }

        DecodedAnimation anim;
        anim.width  = static_cast<uint16_t>(x);
        anim.height = static_cast<uint16_t>(y);
        anim.hasAlpha = true;
        anim.loopCount = 0;

        size_t frameSize = static_cast<size_t>(x) * y * 4;
        for (int i = 0; i < frames; i++) {
            AnimationFrame f;
            f.delay = std::max(1, delays[i]);
            f.data = std::make_unique<uint8_t[]>(frameSize);
            std::memcpy(f.data.get(), raw + i * frameSize, frameSize);
            anim.frames.push_back(std::move(f));
        }

        STBI_FREE(raw);
        STBI_FREE(delays);

        return Ok(DecodedResult{std::move(anim)});
    }
}
IMAGE_PLUS_END_NAMESPACE
