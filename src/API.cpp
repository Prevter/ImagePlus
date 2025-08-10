#include <api.hpp>
#include "hooks/CCSprite.hpp"

IMAGE_PLUS_BEGIN_NAMESPACE
    bool AnimatedSprite::isAnimated() {
        return ImagePlusSprite::from(this)->m_fields->animation != nullptr;
    }

    void AnimatedSprite::stop() {
        ImagePlusSprite::from(this)->stop();
    }

    void AnimatedSprite::pause() {
        ImagePlusSprite::from(this)->pause();
    }

    void AnimatedSprite::play() {
        ImagePlusSprite::from(this)->play();
    }

    bool AnimatedSprite::isPaused() {
        return ImagePlusSprite::from(this)->isPaused();
    }

    void AnimatedSprite::setPlaybackSpeed(float speed) {
        ImagePlusSprite::from(this)->setPlaybackSpeed(speed);
    }

    float AnimatedSprite::getPlaybackSpeed() {
        return ImagePlusSprite::from(this)->getPlaybackSpeed();
    }

    void AnimatedSprite::setForceLoop(std::optional<bool> forceLoop) {
        ImagePlusSprite::from(this)->setForceLoop(forceLoop);
    }

    std::optional<bool> AnimatedSprite::getForceLoop() {
        return ImagePlusSprite::from(this)->getForceLoop();
    }

    uint32_t AnimatedSprite::getCurrentFrame() {
        return ImagePlusSprite::from(this)->getCurrentFrame();
    }

    void AnimatedSprite::setCurrentFrame(uint32_t frame) {
        ImagePlusSprite::from(this)->setCurrentFrame(frame);
    }

    size_t AnimatedSprite::getFrameCount() {
        return ImagePlusSprite::from(this)->getFrameCount();
    }
IMAGE_PLUS_END_NAMESPACE
