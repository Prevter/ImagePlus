#pragma once
#include <Geode/modify/CCSprite.hpp>
#include "../StateManager.hpp"

struct ImagePlusSprite : geode::Modify<ImagePlusSprite, cocos2d::CCSprite>{
    struct Fields {
        std::shared_ptr<imgp::Animation> animation = nullptr;

        double frameTime = 0.0f; // time spent on the current frame
        float playbackSpeed = 1.0f; // speed of playback, 1.0 is normal speed (negative values are reversed)
        size_t frameIndex = 0; // current frame index
        size_t loopCount = 0; // number of completed loops
        bool paused = false; // whether the animation is paused
        std::optional<bool> forceLoop = std::nullopt; // whether to force looping
    };

    static ImagePlusSprite* from(CCSprite* sprite) {
        return static_cast<ImagePlusSprite*>(sprite);
    }

    size_t getFrameCount();

    uint32_t getCurrentFrame();
    void setCurrentFrame(uint32_t frame);

    void play();
    void pause();
    void stop();

    bool isPaused();

    void setForceLoop(std::optional<bool> forceLoop);
    std::optional<bool> getForceLoop();

    float getPlaybackSpeed();
    void setPlaybackSpeed(float speed);

    void stopAndClearAnimation(Fields* fields);
    int getActualLoopCount(Fields* fields, imgp::Animation const* anim);

    cocos2d::CCTexture2D* advanceFrame(Fields* fields, imgp::Animation const* anim);
    cocos2d::CCTexture2D* backwardFrame(Fields* fields, imgp::Animation const* anim);

    void animationUpdate(float dt);

    bool initWithTexture(cocos2d::CCTexture2D* texture, cocos2d::CCRect const& rect, bool rotated) override;
};