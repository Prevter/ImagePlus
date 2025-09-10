#pragma once
#include <api.hpp>

namespace imgp {
    class Animation {
    public:
        Animation(std::shared_ptr<DecodedAnimation> animation, cocos2d::CCTexture2D* first);
        ~Animation();

        cocos2d::CCTexture2D* getFrame(size_t index) const { return m_frames[index]; }
        uint32_t getDelay(size_t index) const { return m_delays[index]; }
        uint16_t getLoopCount() const { return m_loopCount; }
        size_t getFrameCount() const { return m_frames.size(); }

    private:
        std::vector<cocos2d::CCTexture2D*> m_frames;
        std::vector<uint32_t> m_delays;
        uint16_t m_loopCount = 0;
    };

    class StateManager {
    private:
        StateManager() = default;
        ~StateManager() = default;

    public:
        static StateManager& get() {
            static StateManager instance;
            return instance;
        }

        StateManager(StateManager const&) = delete;
        StateManager(StateManager&&) = delete;
        StateManager& operator=(StateManager const&) = delete;
        StateManager& operator=(StateManager&&) = delete;

        void onTextureRemoval(cocos2d::CCTexture2D* texture);

        /// @return true if this was the last image holding a reference to DecodedAnimation
        bool onImageRemoval(cocos2d::CCImage* image);

        std::shared_ptr<DecodedAnimation>& getImageStorage(cocos2d::CCImage* image);
        std::shared_ptr<Animation>& getTextureStorage(cocos2d::CCTexture2D* texture);

        std::optional<std::shared_ptr<DecodedAnimation>> findImageStorage(cocos2d::CCImage* image);
        std::optional<std::shared_ptr<Animation>> findTextureStorage(cocos2d::CCTexture2D* texture);

    private:
        std::unordered_map<cocos2d::CCImage*, std::shared_ptr<DecodedAnimation>> m_imageStorage{};
        std::unordered_map<cocos2d::CCTexture2D*, std::shared_ptr<Animation>> m_textureStorage{};
        std::mutex m_imageStorageMutex{};
        std::mutex m_textureStorageMutex{};
    };
}
