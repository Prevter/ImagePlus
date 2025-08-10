#include <events.hpp>

#include <api.hpp>

using namespace imgp::v1;
using namespace imgp::__detail;

static FunctionTable functionTable = {
    .version = 1,
    .guessFormat = &guessFormat,
    .tryDecode = &tryDecode,

    // == Type Detection == //
    .isJpeg = &formats::isJpeg,
    .isAPng = &formats::isAPng,
    .isPng = &formats::isPng,
    .isGif = &formats::isGif,
    .isWebp = &formats::isWebp,
    .isTiff = &formats::isTiff,
    .isQoi = &formats::isQoi,
    .isJpegXL = &formats::isJpegXL,

    // == Static Image Decoding == //
    .decodePng = &decode::png,
    .decodeQoi = &decode::qoi,

    // == Animated Image Decoding == //
    .decodeJpegXL = &decode::jpegxl,
    .decodeWebp = &decode::webp,
    .decodeGif = &decode::gif,

    // == Static Image Encoding == //
    .encodePng = &encode::png,
    .encodeQoi = &encode::qoi,
    .encodeWebp = &encode::webp,
    .encodeJpegXL = &encode::jpegxl,

    // == Animated Image Encoding == //
    .encodeWebpAnim = &encode::webp,
    .encodeJpegXLAnim = &encode::jpegxl,

    // == AnimatedSprite == // (this is scuffed)
    .AnimatedSprite_isAnimated = reinterpret_cast<FunctionTable::AnimatedSpriteBoolRet>(&AnimatedSprite::isAnimated),
    .AnimatedSprite_stop = reinterpret_cast<FunctionTable::AnimatedSpriteVoidRet>(&AnimatedSprite::stop),
    .AnimatedSprite_pause = reinterpret_cast<FunctionTable::AnimatedSpriteVoidRet>(&AnimatedSprite::pause),
    .AnimatedSprite_play = reinterpret_cast<FunctionTable::AnimatedSpriteVoidRet>(&AnimatedSprite::play),
    .AnimatedSprite_isPaused = reinterpret_cast<FunctionTable::AnimatedSpriteBoolRet>(&AnimatedSprite::isPaused),
    .AnimatedSprite_setPlaybackSpeed = reinterpret_cast<FunctionTable::AnimatedSpriteSetPlaybackSpeed>(&AnimatedSprite::setPlaybackSpeed),
    .AnimatedSprite_getPlaybackSpeed = reinterpret_cast<FunctionTable::AnimatedSpriteGetPlaybackSpeed>(&AnimatedSprite::getPlaybackSpeed),
    .AnimatedSprite_setForceLoop = reinterpret_cast<FunctionTable::AnimatedSpriteSetForceLoop>(&AnimatedSprite::setForceLoop),
    .AnimatedSprite_getForceLoop = reinterpret_cast<FunctionTable::AnimatedSpriteGetForceLoop>(&AnimatedSprite::getForceLoop),
    .AnimatedSprite_getCurrentFrame = reinterpret_cast<FunctionTable::AnimatedSpriteGetCurrentFrame>(&AnimatedSprite::getCurrentFrame),
    .AnimatedSprite_setCurrentFrame = reinterpret_cast<FunctionTable::AnimatedSpriteSetCurrentFrame>(&AnimatedSprite::setCurrentFrame),
    .AnimatedSprite_getFrameCount = reinterpret_cast<FunctionTable::AnimatedSpriteGetFrameCount>(&AnimatedSprite::getFrameCount),
};

$on_mod(Loaded) {
    // Register the event to fetch the function table
    using namespace geode::prelude;
    new EventListener<EventFilter<FetchTableEvent>>(+[](FetchTableEvent* e) {
        e->table = &functionTable;
        return ListenerResult::Stop;
    });
}