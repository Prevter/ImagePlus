#include "StateManager.hpp"

namespace imgp {
    Animation::Animation(std::shared_ptr<DecodedAnimation> animation, cocos2d::CCTexture2D* first) {
        m_frames.reserve(animation->frames.size());
        m_delays.reserve(animation->frames.size());

        if (animation->frames.empty()) {
            geode::log::warn("Animation has no frames, cannot create Animation object");
            return;
        }

        m_frames.push_back(first);
        m_delays.push_back(animation->frames[0].delay);

        for (size_t i = 1; i < animation->frames.size(); ++i) {
            auto& frame = animation->frames[i];
            auto texture = new cocos2d::CCTexture2D();
            texture->initWithData(
                frame.data.get(),
                animation->hasAlpha ? cocos2d::kTexture2DPixelFormat_RGBA8888 : cocos2d::kTexture2DPixelFormat_RGB888,
                animation->width, animation->height,
                cocos2d::CCSize{ static_cast<float>(animation->width), static_cast<float>(animation->height) }
            );
            texture->autorelease();
            texture->retain();
            m_frames.emplace_back(texture);
            m_delays.push_back(frame.delay);
        }

        m_loopCount = animation->loopCount;
    }

    Animation::~Animation() {
        // Release all frames (except the first one, which holds the entire animation)
        for (size_t i = 1; i < m_frames.size(); ++i) {
            m_frames[i]->release();
        }
    }

    void StateManager::onTextureRemoval(cocos2d::CCTexture2D* texture) {
        std::lock_guard lock(m_textureStorageMutex);
        if (auto it = m_textureStorage.find(texture); it != m_textureStorage.end()) {
            m_textureStorage.erase(it);
        }
    }

    bool StateManager::onImageRemoval(cocos2d::CCImage* image) {
        std::lock_guard lock(m_imageStorageMutex);
        if (auto it = m_imageStorage.find(image); it != m_imageStorage.end()) {
            std::weak_ptr weakAnim = it->second;
            m_imageStorage.erase(it);
            if (auto anim = weakAnim.lock()) {} else {
                return true;
            }
        }
        return false;
    }

    std::shared_ptr<DecodedAnimation>& StateManager::getImageStorage(cocos2d::CCImage* image) {
        std::lock_guard lock(m_imageStorageMutex);
        return m_imageStorage[image];
    }

    std::shared_ptr<Animation>& StateManager::getTextureStorage(cocos2d::CCTexture2D* texture) {
        std::lock_guard lock(m_textureStorageMutex);
        return m_textureStorage[texture];
    }

    std::optional<std::shared_ptr<DecodedAnimation>> StateManager::findImageStorage(cocos2d::CCImage* image) {
        std::lock_guard lock(m_imageStorageMutex);
        if (auto it = m_imageStorage.find(image); it != m_imageStorage.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::optional<std::shared_ptr<Animation>> StateManager::findTextureStorage(cocos2d::CCTexture2D* texture) {
        std::lock_guard lock(m_textureStorageMutex);
        if (auto it = m_textureStorage.find(texture); it != m_textureStorage.end()) {
            return it->second;
        }
        return std::nullopt;
    }
}
