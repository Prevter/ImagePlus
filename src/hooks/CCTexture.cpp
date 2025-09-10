#include <Geode/modify/CCTexture2D.hpp>
#include "../StateManager.hpp"

using namespace geode::prelude;

class ImagePlusTexture : public CCTexture2D {
public:
    ~ImagePlusTexture() override {
        imgp::StateManager::get().onTextureRemoval(this);
    }

    static void** getVTable() {
        static void** vtable = []() {
            ImagePlusTexture tex{};
            return *reinterpret_cast<void***>(&tex);
        }();
        return vtable;
    }

    static std::shared_ptr<imgp::Animation>& hook(CCTexture2D* self) {
        *reinterpret_cast<void***>(self) = getVTable();
        return imgp::StateManager::get().getTextureStorage(self);
    }
};

class $modify(ImagePlusTextureHook, CCTexture2D) {
    bool initWithImage(CCImage* image) {
        if (auto anim = imgp::StateManager::get().findImageStorage(image)) {
            ImagePlusTexture::hook(this) = std::make_shared<imgp::Animation>(*anim, this);
        }
        return CCTexture2D::initWithImage(image);
    }
};