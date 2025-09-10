#include <api.hpp>
#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/encode.h>
#include <webp/mux.h>

#include <cstring>
#include <memory>
#include <vector>

#include "../FakeVector.hpp"

using namespace geode;

IMAGE_PLUS_BEGIN_NAMESPACE
namespace decode {
    static void blend_noover(
        uint8_t* canvas, uint8_t const* src,
        int canvasW, int canvasH,
        int srcW, int srcH,
        int offsetX, int offsetY
    ) {
        for (int row = 0; row < srcH; row++) {
            uint8_t* dstRow = canvas + ((offsetY + row) * canvasW + offsetX) * 3;
            uint8_t const* srcRow = src + row * srcW * 3;
            std::memcpy(dstRow, srcRow, srcW * 3);
        }
    }

    static void blend_alpha(
        uint8_t* canvas, uint8_t const* src,
        int canvasW, int canvasH,
        int srcW, int srcH,
        int offsetX, int offsetY
    ) {
        for (int y = 0; y < srcH; y++) {
            uint8_t* dstRow = canvas + ((offsetY + y) * canvasW + offsetX) * 4;
            const uint8_t* srcRow = src + y * srcW * 4;
            for (int x = 0; x < srcW; x++) {
                uint8_t rs = srcRow[4 * x + 0], gs = srcRow[4 * x + 1],
                        bs = srcRow[4 * x + 2], as = srcRow[4 * x + 3];
                uint8_t* dst = dstRow + 4 * x;
                if (as == 255) {
                    dst[0] = rs; dst[1] = gs; dst[2] = bs; dst[3] = 255;
                } else if (as > 0) {
                    float alpha = as / 255.0f;
                    dst[0] = static_cast<uint8_t>(dst[0] * (1 - alpha) + rs * alpha);
                    dst[1] = static_cast<uint8_t>(dst[1] * (1 - alpha) + gs * alpha);
                    dst[2] = static_cast<uint8_t>(dst[2] * (1 - alpha) + bs * alpha);
                    dst[3] = dst[3] < as ? as : dst[3];
                }
            }
        }
    }

    Result<DecodedResult> webp(void const* data, size_t size) {
        WebPData webpData{static_cast<const uint8_t*>(data), size};

        std::unique_ptr<WebPDemuxer, decltype(&WebPDemuxDelete)> demux(WebPDemux(&webpData), &WebPDemuxDelete);
        if (!demux) return Err("Failed to demux WebP data");

        uint32_t frameCount = WebPDemuxGetI(demux.get(), WEBP_FF_FRAME_COUNT);

        if (frameCount <= 1) {
            WebPBitstreamFeatures feats;
            if (WebPGetFeatures(webpData.bytes, webpData.size, &feats) != VP8_STATUS_OK)
                return Err("Failed to get WebP features");

            uint8_t* decoded = feats.has_alpha
                               ? WebPDecodeRGBA(webpData.bytes, webpData.size, nullptr, nullptr)
                               : WebPDecodeRGB(webpData.bytes, webpData.size, nullptr, nullptr);

            if (!decoded) return Err("Failed to decode static WebP image");

            return Ok(DecodedImage{
                .data = std::unique_ptr<uint8_t[]>(decoded),
                .width = static_cast<uint16_t>(feats.width),
                .height = static_cast<uint16_t>(feats.height),
                .hasAlpha = feats.has_alpha != 0
            });
        }

        uint32_t loopCount = WebPDemuxGetI(demux.get(), WEBP_FF_LOOP_COUNT);
        uint32_t canvasW = WebPDemuxGetI(demux.get(), WEBP_FF_CANVAS_WIDTH);
        uint32_t canvasH = WebPDemuxGetI(demux.get(), WEBP_FF_CANVAS_HEIGHT);
        bool hasAlpha = WebPDemuxGetI(demux.get(), WEBP_FF_FORMAT_FLAGS) & ANIMATION_FLAG;

        // setup animation
        DecodedAnimation anim;
        anim.loopCount = static_cast<uint16_t>(loopCount);
        anim.hasAlpha = hasAlpha;
        anim.width = static_cast<uint16_t>(canvasW);
        anim.height = static_cast<uint16_t>(canvasH);

        size_t canvasSize = canvasW * canvasH * (hasAlpha ? 4 : 3);
        std::vector<uint8_t> canvas(canvasSize, 0);

        WebPIterator iter;
        if (!WebPDemuxGetFrame(demux.get(), 1, &iter))
            return Err("Failed to get initial frame");

        do {
            uint8_t* frameDecoded = hasAlpha
                ? WebPDecodeRGBA(iter.fragment.bytes, iter.fragment.size, nullptr, nullptr)
                : WebPDecodeRGB(iter.fragment.bytes, iter.fragment.size, nullptr, nullptr);

            if (!frameDecoded) {
                WebPDemuxReleaseIterator(&iter);
                return Err("Failed to decode animation frame");
            }

            if (hasAlpha) {
                if (iter.blend_method == WEBP_MUX_NO_BLEND) {
                    for (int row = 0; row < iter.height; row++) {
                        uint8_t* dest = canvas.data() + (((iter.y_offset + row) * canvasW + iter.x_offset) * 4);
                        std::memset(dest, 0, iter.width * 4);
                    }
                }
                blend_alpha(canvas.data(), frameDecoded, canvasW, canvasH, iter.width, iter.height, iter.x_offset, iter.y_offset);
            } else {
                blend_noover(canvas.data(), frameDecoded, canvasW, canvasH, iter.width, iter.height, iter.x_offset, iter.y_offset);
            }

            WebPFree(frameDecoded);

            // copy full canvas to frame
            AnimationFrame frame;
            frame.delay = iter.duration;
            frame.data = std::make_unique<uint8_t[]>(canvasSize);
            std::memcpy(frame.data.get(), canvas.data(), canvasSize);
            anim.frames.push_back(std::move(frame));

            if (iter.dispose_method == WEBP_MUX_DISPOSE_BACKGROUND) {
                for (int row = 0; row < iter.height; row++) {
                    uint8_t* dest = canvas.data() + (((iter.y_offset + row) * canvasW + iter.x_offset) * (hasAlpha ? 4 : 3));
                    std::memset(dest, 0, iter.width * (hasAlpha ? 4 : 3));
                }
            }
        } while (WebPDemuxNextFrame(&iter));

        WebPDemuxReleaseIterator(&iter);
        return Ok(DecodedResult{std::move(anim)});
    }
}

namespace encode {
    Result<ByteVector> webp(void const* image, uint16_t width, uint16_t height, bool hasAlpha, float quality) {
        if (!image)
            return Err("Invalid image data");

        uint8_t* output = nullptr;
        size_t outputSize = 0;

        #define ARGS(stride) static_cast<uint8_t const*>(image), width, height, stride, quality, &output

        if (quality >= 99.f) {
            if (hasAlpha) {
                outputSize = WebPEncodeLosslessRGBA(
                    static_cast<uint8_t const*>(image),
                    width, height,
                    width * 4,
                    &output
                );
            } else {
                outputSize = WebPEncodeLosslessRGB(
                    static_cast<uint8_t const*>(image),
                    width, height,
                    width * 3,
                    &output
                );
            }
        } else {
            if (hasAlpha) {
                outputSize = WebPEncodeRGBA(
                    static_cast<uint8_t const*>(image),
                    width, height,
                    width * 4,
                    quality,
                    &output
                );
            } else {
                outputSize = WebPEncodeRGB(
                    static_cast<uint8_t const*>(image),
                    width, height,
                    width * 3,
                    quality,
                    &output
                );
            }
        }


        if (outputSize == 0 || !output) {
            return Err("Failed to encode WebP image");
        }

        return Ok(FakeVector(output, outputSize));
    }

    Result<ByteVector> webp(DecodedAnimation const& anim, float quality) {
        if (anim.frames.empty())
            return Err("Animation has no frames");

        WebPAnimEncoderOptions animOptions;
        if (!WebPAnimEncoderOptionsInit(&animOptions))
            return Err("Failed to initialize animation encoder options");

        animOptions.minimize_size = 1;
        animOptions.kmax = 9;

        std::unique_ptr<WebPAnimEncoder, decltype(&WebPAnimEncoderDelete)> enc(
            WebPAnimEncoderNew(anim.width, anim.height, &animOptions),
            &WebPAnimEncoderDelete
        );

        if (!enc)
            return Err("Failed to create WebP animation encoder");

        int timestamp = 0;
        for (size_t i = 0; i < anim.frames.size(); ++i) {
            auto const& frame = anim.frames[i];

            WebPConfig config;
            if (!WebPConfigInit(&config))
                return Err("Failed to initialize WebP config");

            config.quality = quality;
            config.method = 4;
            config.lossless = (quality >= 99.0f);

            if (!WebPValidateConfig(&config))
                return Err("Invalid WebP config");

            WebPPicture picture;
            if (!WebPPictureInit(&picture))
                return Err("Failed to initialize WebP picture");

            picture.width = anim.width;
            picture.height = anim.height;
            picture.use_argb = anim.hasAlpha ? 1 : 0;

            int success;
            if (anim.hasAlpha) {
                success = WebPPictureImportRGBA(&picture, frame.data.get(), anim.width * 4);
            } else {
                success = WebPPictureImportRGB(&picture, frame.data.get(), anim.width * 3);
            }

            if (!success) {
                WebPPictureFree(&picture);
                return Err("Failed to import frame data");
            }

            if (!WebPAnimEncoderAdd(enc.get(), &picture, timestamp, &config)) {
                WebPPictureFree(&picture);
                return Err("Failed to add frame to animation");
            }

            WebPPictureFree(&picture);
            timestamp += frame.delay;
        }

        if (!WebPAnimEncoderAdd(enc.get(), nullptr, timestamp, nullptr))
            return Err("Failed to finalize animation");


        WebPData webpData;
        if (!WebPAnimEncoderAssemble(enc.get(), &webpData))
            return Err("Failed to assemble animation");

        return Ok(FakeVector(webpData.bytes, webpData.size));
    }
}
IMAGE_PLUS_END_NAMESPACE

