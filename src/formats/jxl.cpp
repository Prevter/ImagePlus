#include <api.hpp>
#include <memory>

#include <jxl/decode.h>
#include <jxl/decode_cxx.h>
#include <jxl/encode.h>
#include <jxl/encode_cxx.h>
#include <jxl/resizable_parallel_runner_cxx.h>

using namespace geode;

IMAGE_PLUS_BEGIN_NAMESPACE
namespace decode {
    Result<DecodedResult> jpegxl(void const* data, size_t size) {
        auto runner = JxlResizableParallelRunnerMake(nullptr);
        auto decoder = JxlDecoderMake(nullptr);
        if (!runner || !decoder)
            return Err("Failed to allocate JPEG XL decoder or runner");

        constexpr uint32_t events = JXL_DEC_BASIC_INFO
                                  | JXL_DEC_COLOR_ENCODING
                                  | JXL_DEC_FRAME
                                  | JXL_DEC_FULL_IMAGE;
        if (JxlDecoderSubscribeEvents(decoder.get(), events) != JXL_DEC_SUCCESS)
            return Err("Failed to subscribe to JPEG XL decoder events");

        if (JxlDecoderSetParallelRunner(
                decoder.get(),
                JxlResizableParallelRunner,
                runner.get()
            ) != JXL_DEC_SUCCESS) {
            return Err("Failed to set JPEG XL parallel runner");
        }

        JxlDecoderSetInput(decoder.get(), static_cast<uint8_t const*>(data), size);

        JxlBasicInfo info{};
        JxlPixelFormat format{};
        JxlFrameHeader frame_header{};
        DecodedAnimation anim;
        bool isAnim = false;

        std::unique_ptr<uint8_t[]> frameBuf;
        size_t frameBufSize = 0;

        while (true) {
            auto status = JxlDecoderProcessInput(decoder.get());

            if (status == JXL_DEC_ERROR)
                return Err("JPEG XL decoder error");
            if (status == JXL_DEC_NEED_MORE_INPUT)
                return Err("JPEG XL needs more input unexpectedly");

            if (status == JXL_DEC_FRAME) {
                if (JxlDecoderGetFrameHeader(decoder.get(), &frame_header) != JXL_DEC_SUCCESS)
                    return Err("Failed to get JPEG XL frame header");
            } else if (status == JXL_DEC_BASIC_INFO) {
                JxlDecoderGetBasicInfo(decoder.get(), &info);
                anim.width = static_cast<uint16_t>(info.xsize);
                anim.height = static_cast<uint16_t>(info.ysize);
                anim.hasAlpha = info.alpha_bits > 0;
                isAnim = info.have_animation;

                format.data_type = (info.bits_per_sample > 8)
                                       ? JXL_TYPE_UINT16
                                       : JXL_TYPE_UINT8;
                format.num_channels = anim.hasAlpha ? 4 : 3;
                format.endianness = JXL_NATIVE_ENDIAN;
                format.align = 0;

                int threads = JxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize);
                JxlResizableParallelRunnerSetThreads(runner.get(), threads);
            } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
                JxlDecoderImageOutBufferSize(decoder.get(), &format, &frameBufSize);
                frameBuf.reset(new uint8_t[frameBufSize]);

                if (JxlDecoderSetImageOutBuffer(
                        decoder.get(),
                        &format,
                        frameBuf.get(),
                        frameBufSize
                    ) != JXL_DEC_SUCCESS) {
                    return Err("Failed to set JPEG XL output buffer");
                }
            } else if (status == JXL_DEC_FULL_IMAGE) {
                AnimationFrame frame;
                frame.data = std::move(frameBuf);
                if (isAnim) {
                    uint64_t ticks = frame_header.duration;
                    uint64_t ms_per_tick = 1000ULL * info.animation.tps_denominator / info.animation.tps_numerator;
                    frame.delay = static_cast<uint32_t>(ticks * ms_per_tick);
                }

                anim.frames.push_back(std::move(frame));

                frameBuf.reset();
                frameBufSize = 0;
            } else if (status == JXL_DEC_SUCCESS) {
                if (isAnim && anim.frames.size() > 1)
                    return Ok(DecodedResult{std::move(anim)});

                if (!anim.frames.empty()) {
                    auto& f = anim.frames.front();
                    return Ok(DecodedImage{
                        .data = std::move(f.data),
                        .width = anim.width,
                        .height = anim.height,
                        .bit_depth = static_cast<uint8_t>(info.bits_per_sample),
                        .hasAlpha = anim.hasAlpha,
                    });
                }
                return Err("No frames decoded");
            }
        }
    }
}

namespace encode {
    Result<ByteVector> jpegxl(void const* image, uint16_t width, uint16_t height, bool hasAlpha, float quality) {
        if (!image)
            return Err("Invalid image data");

        if (width == 0 || height == 0)
            return Err("Invalid image dimensions");

        auto runner = JxlResizableParallelRunnerMake(nullptr);
        auto encoder = JxlEncoderMake(nullptr);
        if (!runner || !encoder)
            return Err("Failed to allocate JPEG XL encoder or runner");

        if (JxlEncoderSetParallelRunner(encoder.get(), JxlResizableParallelRunner, runner.get()) != JXL_ENC_SUCCESS)
            return Err("Failed to set JPEG XL parallel runner");

        JxlBasicInfo basic_info{};
        JxlEncoderInitBasicInfo(&basic_info);
        basic_info.xsize = static_cast<uint32_t>(width);
        basic_info.ysize = static_cast<uint32_t>(height);
        basic_info.bits_per_sample = 8;
        basic_info.alpha_bits = hasAlpha ? 8u : 0u;
        basic_info.num_extra_channels = hasAlpha ? 1u : 0u;
        basic_info.have_animation = JXL_FALSE;
        basic_info.uses_original_profile = quality >= 99.0f ? JXL_TRUE : JXL_FALSE;

        if (JxlEncoderSetBasicInfo(encoder.get(), &basic_info) != JXL_ENC_SUCCESS)
            return Err("Failed to set JPEG XL basic info");

        auto frame_settings = JxlEncoderFrameSettingsCreate(encoder.get(), nullptr);
        if (!frame_settings)
            return Err("Failed to create JPEG XL frame settings");

        if (quality >= 99.0f) {
            if (JxlEncoderSetFrameLossless(frame_settings, JXL_TRUE) != JXL_ENC_SUCCESS)
                return Err("Failed to enable lossless JPEG XL encode");
        } else {
            float distance = JxlEncoderDistanceFromQuality(quality);
            if (JxlEncoderSetFrameDistance(frame_settings, distance) != JXL_ENC_SUCCESS)
                return Err("Failed to set JPEG XL frame distance");
        }

        if (JxlEncoderFrameSettingsSetOption(frame_settings, JXL_ENC_FRAME_SETTING_EFFORT, 7) != JXL_ENC_SUCCESS)
            return Err("Failed to set JPEG XL effort");

        JxlPixelFormat pixel_format{};
        pixel_format.data_type = JXL_TYPE_UINT8;
        pixel_format.num_channels = hasAlpha ? 4 : 3;
        pixel_format.endianness = JXL_NATIVE_ENDIAN;
        pixel_format.align = 0;

        size_t bytes_per_pixel = (pixel_format.data_type == JXL_TYPE_UINT16) ? 2u : 1u;
        size_t total_bytes = static_cast<size_t>(width) * static_cast<size_t>(height)
                             * pixel_format.num_channels * bytes_per_pixel;

        if (JxlEncoderAddImageFrame(frame_settings, &pixel_format, image, total_bytes) != JXL_ENC_SUCCESS)
            return Err("Failed to add image frame to JPEG XL encoder");

        JxlEncoderCloseInput(encoder.get());

        ByteVector out;
        constexpr size_t chunk_size = 1 << 15;
        std::vector<uint8_t> buffer(chunk_size);

        while (true) {
            uint8_t* out_ptr = buffer.data();
            size_t avail = buffer.size();
            JxlEncoderStatus status = JxlEncoderProcessOutput(encoder.get(), &out_ptr, &avail);

            size_t written = buffer.size() - avail;
            if (written)
                out.insert(out.end(), buffer.data(), buffer.data() + written);

            if (status == JXL_ENC_SUCCESS)
                break;

            if (status == JXL_ENC_ERROR) {
                auto err = JxlEncoderGetError(encoder.get());
                return Err(fmt::format("JPEG XL encoding error: {}", static_cast<uint32_t>(err)));
            }
        }

        return Ok(std::move(out));
    }

    Result<ByteVector> jpegxl(DecodedAnimation const& anim, float quality) {
        if (anim.frames.empty())
            return Err("Animation has no frames");

        auto runner = JxlResizableParallelRunnerMake(nullptr);
        auto encoder = JxlEncoderMake(nullptr);
        if (!runner || !encoder)
            return Err("Failed to allocate JPEG XL encoder or runner");

        if (JxlEncoderSetParallelRunner(encoder.get(), JxlResizableParallelRunner, runner.get()) != JXL_ENC_SUCCESS)
            return Err("Failed to set JPEG XL parallel runner");

        JxlBasicInfo basic_info{};
        JxlEncoderInitBasicInfo(&basic_info);
        basic_info.xsize = static_cast<uint32_t>(anim.width);
        basic_info.ysize = static_cast<uint32_t>(anim.height);
        basic_info.bits_per_sample = 8;
        basic_info.alpha_bits = anim.hasAlpha ? 8u : 0u;
        basic_info.num_extra_channels = anim.hasAlpha ? 1u : 0u;
        basic_info.have_animation = JXL_TRUE;

        basic_info.animation.tps_numerator = 1000;
        basic_info.animation.tps_denominator = 1;

        if (JxlEncoderSetBasicInfo(encoder.get(), &basic_info) != JXL_ENC_SUCCESS)
            return Err("Failed to set JPEG XL basic info for animation");

        JxlPixelFormat pixel_format{};
        pixel_format.data_type = JXL_TYPE_UINT8;
        pixel_format.num_channels = anim.hasAlpha ? 4 : 3;
        pixel_format.endianness = JXL_NATIVE_ENDIAN;
        pixel_format.align = 0;

        for (auto const& f : anim.frames) {
            auto frame_settings = JxlEncoderFrameSettingsCreate(encoder.get(), nullptr);
            if (!frame_settings)
                return Err("Failed to create JPEG XL frame settings for animation");

            if (quality >= 99.0f) {
                if (JxlEncoderSetFrameLossless(frame_settings, JXL_TRUE) != JXL_ENC_SUCCESS)
                    return Err("Failed to enable lossless JPEG XL encode for animation");
            } else {
                float distance = JxlEncoderDistanceFromQuality(quality);
                if (JxlEncoderSetFrameDistance(frame_settings, distance) != JXL_ENC_SUCCESS)
                    return Err("Failed to set JPEG XL frame distance for animation");
            }

            if (JxlEncoderFrameSettingsSetOption(frame_settings, JXL_ENC_FRAME_SETTING_EFFORT, 7) != JXL_ENC_SUCCESS)
                return Err("Failed to set JPEG XL effort for animation");

            JxlFrameHeader frame_header{};
            JxlEncoderInitFrameHeader(&frame_header);
            uint64_t ticks = f.delay; // milliseconds
            frame_header.duration = ticks;

            if (JxlEncoderSetFrameHeader(frame_settings, &frame_header) != JXL_ENC_SUCCESS)
                return Err("Failed to set JPEG XL frame header");

            size_t bytes_per_pixel = (pixel_format.data_type == JXL_TYPE_UINT16) ? 2u : 1u;
            uint8_t const* ptr = f.data.get();
            size_t total_bytes = static_cast<size_t>(anim.width) * static_cast<size_t>(anim.height)
                                 * pixel_format.num_channels * bytes_per_pixel;

            if (JxlEncoderAddImageFrame(frame_settings, &pixel_format, ptr, total_bytes) != JXL_ENC_SUCCESS)
                return Err("Failed to add animation frame to JPEG XL encoder");
        }

        JxlEncoderCloseInput(encoder.get());

        ByteVector out;
        constexpr size_t chunk_size = 1 << 15;
        std::vector<uint8_t> buffer(chunk_size);

        while (true) {
            uint8_t* out_ptr = buffer.data();
            size_t avail = buffer.size();
            JxlEncoderStatus status = JxlEncoderProcessOutput(encoder.get(), &out_ptr, &avail);

            size_t written = buffer.size() - avail;
            if (written)
                out.insert(out.end(), buffer.data(), buffer.data() + written);

            if (status == JXL_ENC_SUCCESS)
                break;

            if (status == JXL_ENC_ERROR) {
                auto err = JxlEncoderGetError(encoder.get());
                return Err(fmt::format("JPEG XL encoding error: {}", static_cast<uint32_t>(err)));
            }
        }

        return Ok(std::move(out));
    }
}
IMAGE_PLUS_END_NAMESPACE