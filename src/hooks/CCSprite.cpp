#include "CCSprite.hpp"
#include "../StateManager.hpp"

using namespace geode::prelude;

size_t ImagePlusSprite::getFrameCount() {
    auto fields = m_fields.self();
    return fields->animation ? fields->animation->getFrameCount() : 0;
}

uint32_t ImagePlusSprite::getCurrentFrame() {
    auto fields = m_fields.self();
    return fields->animation ? fields->frameIndex : 0;
}

void ImagePlusSprite::setCurrentFrame(uint32_t frame) {
    auto fields = m_fields.self();
    if (fields->animation && frame < fields->animation->getFrameCount()) {
        fields->frameIndex = frame;
        fields->frameTime = 0.0f; // reset frame time
        this->setTexture(fields->animation->getFrame(frame));
    }
}

void ImagePlusSprite::play() {
    m_fields->paused = false;
}

void ImagePlusSprite::pause() {
    m_fields->paused = true;
}

void ImagePlusSprite::stop() {
    m_fields->paused = true;
    this->setCurrentFrame(0);
}

bool ImagePlusSprite::isPaused() {
    return m_fields->paused;
}

void ImagePlusSprite::setForceLoop(std::optional<bool> forceLoop) {
    m_fields->forceLoop = forceLoop;
}

std::optional<bool> ImagePlusSprite::getForceLoop() {
    return m_fields->forceLoop;
}

float ImagePlusSprite::getPlaybackSpeed() {
    return m_fields->playbackSpeed;
}

void ImagePlusSprite::setPlaybackSpeed(float speed) {
    m_fields->playbackSpeed = speed;
}

void ImagePlusSprite::stopAndClearAnimation(Fields* fields) {
    this->unschedule(schedule_selector(ImagePlusSprite::animationUpdate));
    fields->animation = nullptr;
}

int ImagePlusSprite::getActualLoopCount(Fields* fields, imgp::Animation const* anim) {
    int loopCount = anim->getLoopCount();
    if (fields->forceLoop.has_value()) {
        return fields->forceLoop.value() ? 0 : 1; // 0 means loop indefinitely, 1 means play once
    }
    return loopCount;
}

CCTexture2D* ImagePlusSprite::advanceFrame(Fields* fields, imgp::Animation const* anim) {
    int64_t idx = fields->frameIndex;
    auto loopCount = getActualLoopCount(fields, anim);

    while (fields->frameTime >= anim->getDelay(idx)) {
        fields->frameTime -= anim->getDelay(idx);
        idx++;

        if (idx >= anim->getFrameCount()) {
            idx = 0;
            fields->loopCount++;

            // if loop count is 0, we loop indefinitely
            if (loopCount > 0 && fields->loopCount >= loopCount) {
                return nullptr;
            }
        }
    }

    fields->frameIndex = idx;
    return anim->getFrame(idx);
}

CCTexture2D* ImagePlusSprite::backwardFrame(Fields* fields, imgp::Animation const* anim) {
    int64_t idx = fields->frameIndex;
    auto loopCount = getActualLoopCount(fields, anim);

    while (fields->frameTime < 0.0) {
        idx--;
        if (idx < 0) {
            idx = anim->getFrameCount() - 1;
            fields->loopCount++;

            if (loopCount > 0 && fields->loopCount >= loopCount) {
                return nullptr; // stop the animation
            }
        }

        fields->frameTime += anim->getDelay(idx);
    }

    fields->frameIndex = idx;
    return anim->getFrame(idx);
}

void ImagePlusSprite::animationUpdate(float dt) {
    auto fields = m_fields.self();

    if (fields->paused) {
        return;
    }

    auto anim = fields->animation;
    if (!anim) {
        return this->unschedule(schedule_selector(ImagePlusSprite::animationUpdate));
    }

    auto totalFrames = anim->getFrameCount();
    if (totalFrames == 0) {
        return this->stopAndClearAnimation(fields);
    }

    auto deltaMs = dt * 1000.0f * fields->playbackSpeed;
    fields->frameTime += deltaMs;

    if (fields->frameIndex >= totalFrames) {
        fields->frameIndex = 0;
    }

    CCTexture2D* frame = nullptr;
    if (fields->playbackSpeed >= 0) {
        frame = this->advanceFrame(fields, anim.get());
    } else {
        frame = this->backwardFrame(fields, anim.get());
    }

    if (!frame) {
        return;
    }

    this->setTexture(frame);
}

bool ImagePlusSprite::initWithTexture(CCTexture2D* texture, CCRect const& rect, bool rotated) {
    if (!CCSprite::initWithTexture(texture, rect, rotated)) {
        return false;
    }

    if (auto anim = imgp::StateManager::get().findTextureStorage(texture)) {
        auto fields = m_fields.self();
        fields->animation = *anim;
        this->schedule(schedule_selector(ImagePlusSprite::animationUpdate));
    }

    return true;
}
