#include <api.hpp>
#include <gif_lib.h>

#include <cstring>
#include <memory>
#include <vector>

using namespace geode;

IMAGE_PLUS_BEGIN_NAMESPACE
namespace decode {
    struct GifMemoryReader {
        const uint8_t* data;
        size_t size;
        size_t pos;
    };

    static int readFromMemory(GifFileType* gif, GifByteType* buf, int size) {
        auto reader = static_cast<GifMemoryReader*>(gif->UserData);
        if (reader->pos >= reader->size) return 0;

        int toRead = std::min(size, static_cast<int>(reader->size - reader->pos));
        std::memcpy(buf, reader->data + reader->pos, toRead);
        reader->pos += toRead;
        return toRead;
    }

    static void applyPaletteToFrame(
        uint8_t* dest, const uint8_t* src,
        const ColorMapObject* colorMap,
        int width, int height,
        int transparentIndex = -1
    ) {
        for (int i = 0; i < width * height; i++) {
            uint8_t index = src[i];

            if (transparentIndex >= 0 && index == transparentIndex) {
                dest[i * 4 + 0] = 0;
                dest[i * 4 + 1] = 0;
                dest[i * 4 + 2] = 0;
                dest[i * 4 + 3] = 0;
                continue;
            }

            if (colorMap && index < colorMap->ColorCount) {
                GifColorType& color = colorMap->Colors[index];
                dest[i * 4 + 0] = color.Red;
                dest[i * 4 + 1] = color.Green;
                dest[i * 4 + 2] = color.Blue;
                dest[i * 4 + 3] = 255;
            } else {
                dest[i * 4 + 0] = 0;
                dest[i * 4 + 1] = 0;
                dest[i * 4 + 2] = 0;
                dest[i * 4 + 3] = 255;
            }
        }
    }

    static void renderFrameToCanvas(
        uint8_t* canvas, const uint8_t* frameData,
        int canvasWidth, int canvasHeight,
        int frameWidth, int frameHeight,
        int offsetX, int offsetY,
        bool hasTransparency
    ) {
        int startX = std::max(0, offsetX);
        int startY = std::max(0, offsetY);
        int endX = std::min(canvasWidth, offsetX + frameWidth);
        int endY = std::min(canvasHeight, offsetY + frameHeight);

        for (int y = startY; y < endY; y++) {
            for (int x = startX; x < endX; x++) {
                int canvasPos = (y * canvasWidth + x) * 4;
                int frameX = x - offsetX;
                int frameY = y - offsetY;
                int framePos = (frameY * frameWidth + frameX) * 4;

                if (frameX >= 0 && frameX < frameWidth && frameY >= 0 && frameY < frameHeight) {
                    if (hasTransparency && frameData[framePos + 3] == 0) {
                        continue;
                    }

                    canvas[canvasPos + 0] = frameData[framePos + 0];
                    canvas[canvasPos + 1] = frameData[framePos + 1];
                    canvas[canvasPos + 2] = frameData[framePos + 2];
                    canvas[canvasPos + 3] = frameData[framePos + 3];
                }
            }
        }
    }

    static void clearCanvasRegion(
        uint8_t* canvas, int canvasWidth, int canvasHeight,
        int regionX, int regionY, int regionWidth, int regionHeight
    ) {
        int startX = std::max(0, regionX);
        int startY = std::max(0, regionY);
        int endX = std::min(canvasWidth, regionX + regionWidth);
        int endY = std::min(canvasHeight, regionY + regionHeight);

        for (int y = startY; y < endY; y++) {
            for (int x = startX; x < endX; x++) {
                int pos = (y * canvasWidth + x) * 4;
                canvas[pos + 0] = 0;
                canvas[pos + 1] = 0;
                canvas[pos + 2] = 0;
                canvas[pos + 3] = 0;
            }
        }
    }

    static void applyDisposalMethod(
        uint8_t* canvas, const uint8_t* previousCanvas,
        int canvasWidth, int canvasHeight,
        int disposalMethod, int frameX, int frameY, int frameWidth, int frameHeight
    ) {
        size_t canvasSize = canvasWidth * canvasHeight * 4;

        switch (disposalMethod) {
            case DISPOSE_BACKGROUND:
                clearCanvasRegion(
                    canvas, canvasWidth, canvasHeight,
                    frameX, frameY, frameWidth, frameHeight
                );
                break;
            case DISPOSE_PREVIOUS:
                if (previousCanvas)
                    std::memcpy(canvas, previousCanvas, canvasSize);
                break;
            default: break;
        }
    }

    Result<DecodedResult> gif(void const* data, size_t size) {
        GifMemoryReader reader{static_cast<const uint8_t*>(data), size, 0};

        int errorCode;
        GifFileType* gif = DGifOpen(&reader, readFromMemory, &errorCode);
        if (!gif) {
            return Err(fmt::format("Failed to open GIF: {}", GifErrorString(errorCode)));
        }

        if (DGifSlurp(gif) == GIF_ERROR) {
            auto error = fmt::format("Failed to read GIF: {}", GifErrorString(gif->Error));
            DGifCloseFile(gif, &errorCode);
            return Err(error);
        }

        if (gif->ImageCount <= 0) {
            DGifCloseFile(gif, &errorCode);
            return Err("No images found in GIF");
        }

        if (gif->ImageCount == 1) {
            SavedImage& image = gif->SavedImages[0];
            GifImageDesc& desc = image.ImageDesc;
            ColorMapObject* colorMap = desc.ColorMap ? desc.ColorMap : gif->SColorMap;

            if (!colorMap) {
                DGifCloseFile(gif, &errorCode);
                return Err("No color map found");
            }

            int transparentIndex = -1;
            if (image.ExtensionBlockCount > 0) {
                for (int i = 0; i < image.ExtensionBlockCount; i++) {
                    if (image.ExtensionBlocks[i].Function == GRAPHICS_EXT_FUNC_CODE) {
                        GraphicsControlBlock gcb;
                        if (DGifExtensionToGCB(image.ExtensionBlocks[i].ByteCount, image.ExtensionBlocks[i].Bytes, &gcb) == GIF_OK) {
                            if (gcb.TransparentColor != NO_TRANSPARENT_COLOR) {
                                transparentIndex = gcb.TransparentColor;
                            }
                        }
                    }
                }
            }

            bool hasAlpha = transparentIndex >= 0;
            int channels = hasAlpha ? 4 : 3;
            size_t imageSize = desc.Width * desc.Height * channels;

            DecodedImage img;
            img.width = static_cast<uint16_t>(desc.Width);
            img.height = static_cast<uint16_t>(desc.Height);
            img.hasAlpha = hasAlpha;
            img.data = std::make_unique<uint8_t[]>(imageSize);

            if (hasAlpha) {
                applyPaletteToFrame(
                    img.data.get(), image.RasterBits, colorMap,
                    desc.Width, desc.Height, transparentIndex
                );
            } else {
                for (int i = 0; i < desc.Width * desc.Height; i++) {
                    uint8_t index = image.RasterBits[i];
                    if (index < colorMap->ColorCount) {
                        GifColorType& color = colorMap->Colors[index];
                        img.data[i * 3 + 0] = color.Red;
                        img.data[i * 3 + 1] = color.Green;
                        img.data[i * 3 + 2] = color.Blue;
                    }
                }
            }

            DGifCloseFile(gif, &errorCode);
            return Ok(DecodedResult{std::move(img)});
        }

        DecodedAnimation anim;
        anim.width = static_cast<uint16_t>(gif->SWidth);
        anim.height = static_cast<uint16_t>(gif->SHeight);
        anim.hasAlpha = true;
        anim.loopCount = 0;

        for (int i = 0; i < gif->ExtensionBlockCount; i++) {
            if (gif->ExtensionBlocks[i].Function == APPLICATION_EXT_FUNC_CODE &&
                gif->ExtensionBlocks[i].ByteCount >= 11) {
                if (std::memcmp(gif->ExtensionBlocks[i].Bytes, "NETSCAPE2.0", 11) == 0) {
                    if (i + 1 < gif->ExtensionBlockCount &&
                        gif->ExtensionBlocks[i + 1].ByteCount >= 3 &&
                        gif->ExtensionBlocks[i + 1].Bytes[0] == 1) {
                        anim.loopCount = gif->ExtensionBlocks[i + 1].Bytes[1] |
                                         gif->ExtensionBlocks[i + 1].Bytes[2] << 8;
                    }
                }
            }
        }

        size_t canvasSize = gif->SWidth * gif->SHeight * 4;
        std::vector<uint8_t> canvas(canvasSize, 0);
        std::vector<uint8_t> previousCanvas(canvasSize);

        for (int frameIndex = 0; frameIndex < gif->ImageCount; frameIndex++) {
            SavedImage& image = gif->SavedImages[frameIndex];
            GifImageDesc& desc = image.ImageDesc;

            if (desc.Left < 0 || desc.Top < 0 ||
                desc.Left + desc.Width > gif->SWidth ||
                desc.Top + desc.Height > gif->SHeight) {
                continue;
            }

            ColorMapObject* colorMap = desc.ColorMap ? desc.ColorMap : gif->SColorMap;
            if (!colorMap) continue;

            int transparentIndex = -1;
            uint16_t delay = 10;

            GraphicsControlBlock gcb;
            if (DGifSavedExtensionToGCB(gif, frameIndex, &gcb) == GIF_OK) {
                if (gcb.TransparentColor != NO_TRANSPARENT_COLOR) {
                    transparentIndex = gcb.TransparentColor;
                }
                delay = gcb.DelayTime;
            }

            if (frameIndex > 0) {
                GraphicsControlBlock prevGcb;
                int prevDisposal = DISPOSAL_UNSPECIFIED;
                int prevLeft = 0, prevTop = 0, prevWidth = 0, prevHeight = 0;

                if (DGifSavedExtensionToGCB(gif, frameIndex - 1, &prevGcb) == GIF_OK) {
                    prevDisposal = prevGcb.DisposalMode;
                }

                SavedImage& prevImage = gif->SavedImages[frameIndex - 1];
                prevLeft = prevImage.ImageDesc.Left;
                prevTop = prevImage.ImageDesc.Top;
                prevWidth = prevImage.ImageDesc.Width;
                prevHeight = prevImage.ImageDesc.Height;

                applyDisposalMethod(
                    canvas.data(), previousCanvas.data(),
                    gif->SWidth, gif->SHeight, prevDisposal,
                    prevLeft, prevTop, prevWidth, prevHeight
                );
            }

            std::memcpy(previousCanvas.data(), canvas.data(), canvasSize);

            size_t frameSize = desc.Width * desc.Height * 4;
            std::vector<uint8_t> frameData(frameSize);
            applyPaletteToFrame(
                frameData.data(), image.RasterBits, colorMap,
                desc.Width, desc.Height, transparentIndex
            );

            renderFrameToCanvas(
                canvas.data(), frameData.data(),
                gif->SWidth, gif->SHeight,
                desc.Width, desc.Height,
                desc.Left, desc.Top,
                transparentIndex >= 0
            );

            AnimationFrame frame;
            frame.delay = std::max(1, static_cast<int>(delay)) * 10;
            frame.data = std::make_unique<uint8_t[]>(canvasSize);
            std::memcpy(frame.data.get(), canvas.data(), canvasSize);
            anim.frames.push_back(std::move(frame));
        }

        DGifCloseFile(gif, &errorCode);
        return Ok(DecodedResult{std::move(anim)});
    }
}
IMAGE_PLUS_END_NAMESPACE
