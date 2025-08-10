#ifdef INCLUDE_TESTS

#include <api.hpp>
#include <Geode/modify/MenuLayer.hpp>

using namespace imgp;

static constexpr std::array<uint8_t, 16> TEST_IMAGE = {
    255,   0,   0, 128,   // Red
      0, 255,   0, 255,   // Green
      0,   0, 255, 128,   // Blue
    255, 255,   0, 255    // Yellow
};

struct ScopedNest {
    ScopedNest() { geode::log::pushNest(); }
    ~ScopedNest() { geode::log::popNest(); }
};

template <typename EncodeFn, typename DecodeFn>
void testEncoder(std::string_view name, EncodeFn encode, DecodeFn decode) {
    geode::log::info("[TEST] {} ... ", name);
    ScopedNest nest;

    auto enc = encode(TEST_IMAGE.data(), 2, 2, true);
    if (!enc.isOk()) {
        geode::log::error("Encoding failed: {}", enc.unwrapErr());
        return;
    }

    auto bytes = enc.unwrap();
    auto dec = decode(bytes.data(), bytes.size());
    if (!dec.isOk()) {
        geode::log::error("Decoding failed: {}", dec.unwrapErr());
        return;
    }

    auto decoded = std::move(dec).unwrap();

    void* data = nullptr;
    uint16_t width = 0, height = 0;

    if constexpr (std::is_same_v<decltype(decoded), DecodedImage>) {
        data = decoded.data.get();
        width = decoded.width;
        height = decoded.height;
    } else if constexpr (std::is_same_v<decltype(decoded), DecodedResult>) {
        if (std::holds_alternative<DecodedImage>(decoded)) {
            auto& img = std::get<DecodedImage>(decoded);
            if (!img) {
                geode::log::error("Decoded image is empty");
                return;
            }
            data = img.data.get();
            width = img.width;
            height = img.height;
            if (img.hasAlpha) {
                geode::log::info("Decoded image has alpha channel");
            } else {
                geode::log::info("Decoded image does not have alpha channel");
            }
        } else if (std::holds_alternative<DecodedAnimation>(decoded)) {
            auto& anim = std::get<DecodedAnimation>(decoded);
            if (anim.frames.empty()) {
                geode::log::error("Decoded animation has no frames");
                return;
            }
            auto& img = anim.frames[0];
            if (!img.data) {
                geode::log::error("Decoded animation frame is empty");
                return;
            }
            data = img.data.get();
            width = anim.width;
            height = anim.height;
            geode::log::info("Decoded animation with {} frames", anim.frames.size());
        } else {
            geode::log::error("Unexpected decoded result type");
            return;
        }
    } else {
        geode::log::error("Unexpected decoded result type");
        return;
    }

    if (width == 2 && height == 2) {
        geode::log::info("Image dimensions are correct: {}x{}", width, height);
    } else {
        geode::log::error("Image dimensions mismatch: expected 2x2, got {}x{}", width, height);
        return;
    }

    // Check pixel data
    auto match = std::memcmp(data, TEST_IMAGE.data(), TEST_IMAGE.size());
    if (match == 0) {
        geode::log::info("Pixel data matches expected values");
    } else {
        geode::log::error("Pixel data mismatch");
        for (size_t i = 0; i < TEST_IMAGE.size(); i += 4) {
            geode::log::error("Expected: {} {} {} {}, Got: {} {} {} {}",
                TEST_IMAGE[i], TEST_IMAGE[i + 1], TEST_IMAGE[i + 2], TEST_IMAGE[i + 3],
                static_cast<uint8_t*>(data)[i], static_cast<uint8_t*>(data)[i + 1],
                static_cast<uint8_t*>(data)[i + 2], static_cast<uint8_t*>(data)[i + 3]);
        }
        return;
    }

    geode::log::info("{} completed successfully", name);
}

class $modify(ImagePlusTest, MenuLayer) {
    bool init() override {
        if (!MenuLayer::init()) {
            return false;
        }

        auto myButton = CCMenuItemSpriteExtra::create(
            cocos2d::CCSprite::createWithSpriteFrameName("GJ_helpBtn_001.png"),
            this, menu_selector(ImagePlusTest::onTestButton)
        );

        auto menu = this->getChildByID("bottom-menu");
        menu->addChild(myButton);
        myButton->setID("test-button"_spr);
        menu->updateLayout();

        return true;
    }

    void onTestButton(CCObject*) {
        testEncoder("PNG", encode::png, decode::png);
        testEncoder("QOI", encode::qoi, decode::qoi);
        testEncoder(
            "WEBP",
            [](auto* img, uint16_t w, uint16_t h, bool a) {
                return encode::webp(img, w, h, a, 100.f);
            },
            decode::webp
        );
        testEncoder("JPEG XL",
            [](auto* img, uint16_t w, uint16_t h, bool a) {
                return encode::jpegxl(img, w, h, a, 100.f);
            },
            decode::jpegxl
        );

        geode::log::info("[TEST] All image format tests completed");
    }
};


#endif // INCLUDE_TESTS