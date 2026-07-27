#include <api.hpp>
#include <array>
#include <algorithm>

#include "backends/IBackend.hpp"

IMAGE_PLUS_BEGIN_NAMESPACE

struct HackCCTexture2D : cocos2d::CCTexture2D {
    bool initWithGLName(GLuint name, cocos2d::CCTexture2DPixelFormat fmt, unsigned int pixelsWide, unsigned int pixelsHigh) {
        m_uName = name;
        float scale = cocos2d::CCDirector::get()->getContentScaleFactor();
        m_tContentSize = cocos2d::CCSize{ static_cast<float>(pixelsWide) / scale, static_cast<float>(pixelsHigh) / scale };
        m_uPixelsWide = pixelsWide;
        m_uPixelsHigh = pixelsHigh;
        m_ePixelFormat = fmt;
        m_fMaxS = 1.0f;
        m_fMaxT = 1.0f;
        m_bHasPremultipliedAlpha = false;
        m_bHasMipmaps = false;
        this->setShaderProgram(cocos2d::CCShaderCache::sharedShaderCache()->programForKey(kCCShader_PositionTexture));
        return true;
    }
};

constexpr auto NV12_VERTEX_SHADER = R"(
attribute vec4 a_position;
attribute vec4 a_color;
attribute vec2 a_texCoord;

#ifdef GL_ES
varying lowp vec4 v_fragmentColor;
varying mediump vec2 v_texCoord;
#else
varying vec4 v_fragmentColor;
varying vec2 v_texCoord;
#endif

void main() {
    gl_Position = CC_MVPMatrix * a_position;
    v_fragmentColor = a_color;
    v_texCoord = a_texCoord;
})";

constexpr auto NV12_FRAGMENT_SHADER = R"(
#ifdef GL_ES
precision mediump float;
#endif

varying vec4 v_fragmentColor;
varying vec2 v_texCoord;
uniform sampler2D CC_Texture0; // Y plane
uniform sampler2D CC_Texture1; // UV plane

uniform vec3 u_yuvOffset;
uniform vec3 u_mtxR;
uniform vec3 u_mtxG;
uniform vec3 u_mtxB;

void main() {
    float y = texture2D(CC_Texture0, v_texCoord).r;
    vec2 uv = texture2D(CC_Texture1, v_texCoord).ra;
    vec3 yuv = vec3(y - u_yuvOffset.x, uv.x - u_yuvOffset.y, uv.y - u_yuvOffset.z);

    float r = dot(u_mtxR, yuv);
    float g = dot(u_mtxG, yuv);
    float b = dot(u_mtxB, yuv);

    gl_FragColor = vec4(clamp(r, 0.0, 1.0), clamp(g, 0.0, 1.0), clamp(b, 0.0, 1.0), 1.0) * v_fragmentColor;
})";

static cocos2d::CCGLProgram* getNV12Program() {
    auto shaderCache = cocos2d::CCShaderCache::sharedShaderCache();
    auto program = shaderCache->programForKey("nv12"_spr);
    if (program) {
        return program;
    }

    program = new cocos2d::CCGLProgram();
    program->initWithVertexShaderByteArray(NV12_VERTEX_SHADER, NV12_FRAGMENT_SHADER);
    program->addAttribute("a_position", cocos2d::kCCVertexAttrib_Position);
    program->addAttribute("a_color", cocos2d::kCCVertexAttrib_Color);
    program->addAttribute("a_texCoord", cocos2d::kCCVertexAttrib_TexCoords);

    if (!program->link()) {
        geode::log::error("Failed to link shader program for NV12");
        program->release();
        return nullptr;
    }

    program->updateUniforms();

    program->use();
    program->setUniformLocationWith1i(program->getUniformLocationForName("CC_Texture0"), 0);
    program->setUniformLocationWith1i(program->getUniformLocationForName("CC_Texture1"), 1);

    shaderCache->addProgram(program, "nv12"_spr);
    program->release();

    return program;
}

struct VideoNode::Impl {
    VideoHandle m_handle;
    std::vector<uint8_t> m_frameData;

    FMOD::Sound* m_sound = nullptr;
    FMOD::Channel* m_channel = nullptr;

    GLuint m_yTex = 0, m_uvTex = 0;
    geode::Ref<cocos2d::CCTexture2D> m_yTexture, m_uvTexture;

    double m_currentTime = 0.0;
    double m_frameTime = 0.0;
    double m_nextFrameTime = 0.0; // next frame timestamp

    float m_playbackSpeed = 1.0f;
    bool m_paused = false;
    bool m_loop = true;

    static GLuint createTexture(GLsizei width, GLsizei height, GLenum internalFormat, GLenum format) {
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, nullptr);
        return tex;
    }

    void initTextures() {
        if (!m_handle) return;

        GLsizei bufW = static_cast<GLsizei>(m_handle.getBufferWidth());
        GLsizei bufH = static_cast<GLsizei>(m_handle.getBufferHeight());
        GLsizei uvW = (bufW + 1) / 2;
        GLsizei uvH = (bufH + 1) / 2;

        m_yTex = createTexture(bufW, bufH, GL_LUMINANCE, GL_LUMINANCE);
        m_uvTex = createTexture(uvW, uvH, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA);

        glBindTexture(GL_TEXTURE_2D, 0);

        auto texY = new HackCCTexture2D();
        texY->initWithGLName(m_yTex, cocos2d::kCCTexture2DPixelFormat_I8, static_cast<unsigned>(bufW), static_cast<unsigned>(bufH));
        texY->autorelease();
        m_yTexture = texY;

        auto texUV = new HackCCTexture2D();
        texUV->initWithGLName(m_uvTex, cocos2d::kCCTexture2DPixelFormat_AI88, static_cast<unsigned>(uvW), static_cast<unsigned>(uvH));
        texUV->autorelease();
        m_uvTexture = texUV;

        this->uploadTextureData();
    }

    void uploadTextureData() const {
        if (m_frameData.empty() || !m_handle) return;

        GLsizei bufW = static_cast<GLsizei>(m_handle.getBufferWidth());
        GLsizei bufH = static_cast<GLsizei>(m_handle.getBufferHeight());
        GLsizei uvW = (bufW + 1) / 2;
        GLsizei uvH = (bufH + 1) / 2;
        size_t yBytes = static_cast<size_t>(bufW) * static_cast<size_t>(bufH);

        auto Y = m_frameData.data();
        auto UV = Y + yBytes;

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glBindTexture(GL_TEXTURE_2D, m_yTex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bufW, bufH, GL_LUMINANCE, GL_UNSIGNED_BYTE, Y);

        glBindTexture(GL_TEXTURE_2D, m_uvTex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, uvW, uvH, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, UV);

        glBindTexture(GL_TEXTURE_2D, 0);
    }
};

VideoNode::VideoNode() : m_impl(new Impl()) {}

VideoNode::~VideoNode() {
    if (m_impl) {
        delete m_impl;
        m_impl = nullptr;
    }
}

VideoNode* VideoNode::create(char const* path) {
    auto ret = new VideoNode();
    if (ret->initFromFile(path)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

VideoNode* VideoNode::create(std::vector<uint8_t>&& data) {
    auto ret = new VideoNode();
    if (ret->initFromData(std::move(data))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool VideoNode::initFromFile(char const* path) {
    if (!path) {
        return false;
    }

    std::string fullPath = cocos2d::CCFileUtils::get()->fullPathForFilename(path, false);
    if (fullPath.empty()) {
        return false;
    }

    auto res = VideoHandle::createFromFile(fullPath);
    if (!res) {
        geode::log::warn("Error loading video from file: {}", res.unwrapErr());
        return false;
    }

    m_impl->m_handle = std::move(res).unwrap();
    return this->init();
}

bool VideoNode::initFromData(std::vector<uint8_t>&& data) {
    if (data.empty()) {
        return false;
    }

    auto res = VideoHandle::createFromMemory(std::move(data));
    if (!res) {
        geode::log::warn("Error loading video from data: {}", res.unwrapErr());
        return false;
    }

    m_impl->m_handle = std::move(res).unwrap();
    return this->init();
}

bool VideoNode::init() {
    if (!m_impl->m_handle) {
        return false;
    }

    auto& handle = m_impl->m_handle;

    auto fps = handle.getFps();
    m_impl->m_frameTime = handle.getFrameTime();
    m_impl->m_nextFrameTime = m_impl->m_frameTime;

    geode::log::info("Video loaded: {}x{}, {:.2f} fps, duration: {:.2f} seconds",
        handle.getWidth(), handle.getHeight(), fps, handle.getDurationInSeconds());

    // Read first frame to allocate buffer
    if (!m_impl->m_handle.readNextFrame(m_impl->m_frameData)) {
        geode::log::error("Failed to read initial video frame");
        return false;
    }

    m_impl->initTextures();

    float scale = cocos2d::CCDirector::get()->getContentScaleFactor();
    auto rect = cocos2d::CCRect{
        0, 0,
        static_cast<float>(handle.getWidth()) / scale,
        static_cast<float>(handle.getHeight()) / scale
    };

    if (!CCSprite::initWithTexture(m_impl->m_yTexture, rect)) {
        return false;
    }

    this->scheduleUpdate();
    this->setShaderProgram(getNV12Program());

    if (auto sound = handle.getSound()) {
        auto system = FMODAudioEngine::get()->m_system;
        m_impl->m_sound = sound;
        if (system->playSound(sound, nullptr, true, &m_impl->m_channel) == FMOD_OK) {
            handle.assignChannel(m_impl->m_channel);
            m_impl->m_channel->setPaused(false);
        }
    }

    return true;
}

void VideoNode::update(float delta) {
    if (!m_impl->m_handle || m_impl->m_paused) {
        return;
    }

    m_impl->m_currentTime += delta * m_impl->m_playbackSpeed;

    bool gotNewFrame = false;
    while (m_impl->m_currentTime >= m_impl->m_nextFrameTime) {
        if (m_impl->m_handle.readNextFrame(m_impl->m_frameData)) {
            m_impl->m_nextFrameTime += m_impl->m_frameTime;
            gotNewFrame = true;
        } else {
            if (m_impl->m_loop) {
                // Restart video
                m_impl->m_handle.seek(0.0);
                m_impl->m_currentTime = 0.0;
                m_impl->m_nextFrameTime = m_impl->m_frameTime;

                if (m_impl->m_channel) {
                    m_impl->m_channel->setPosition(0, FMOD_TIMEUNIT_MS);
                }
            } else {
                // Stop playback
                m_impl->m_paused = true;
                if (m_impl->m_channel) {
                    m_impl->m_channel->setPaused(true);
                }
                break;
            }
        }
    }

    if (gotNewFrame) {
        // update textures
        m_impl->uploadTextureData();

        // handle buffer size changes
        auto bufW = m_impl->m_handle.getBufferWidth();
        auto bufH = m_impl->m_handle.getBufferHeight();
        if (bufW != this->getTexture()->getPixelsWide() || bufH != this->getTexture()->getPixelsHigh()) {
            m_impl->initTextures();
            this->setTexture(m_impl->m_yTexture);
            float scale = cocos2d::CCDirector::get()->getContentScaleFactor();
            this->setTextureRect({
                0, 0,
                static_cast<float>(m_impl->m_handle.getWidth()) / scale,
                static_cast<float>(m_impl->m_handle.getHeight()) / scale
            });
        }
    }
}

void VideoNode::setPaused(bool paused) {
    m_impl->m_paused = paused;
    if (m_impl->m_channel) {
        m_impl->m_channel->setPaused(paused);
    }
}

bool VideoNode::isPaused() const {
    return m_impl->m_paused;
}

void VideoNode::setPlaybackSpeed(float speed) {
    m_impl->m_playbackSpeed = std::max(0.0f, speed);
    if (m_impl->m_channel) {
        m_impl->m_channel->setFrequency(m_impl->m_handle.getAudioSampleRate() * m_impl->m_playbackSpeed);
    }
}

float VideoNode::getPlaybackSpeed() const {
    return m_impl->m_playbackSpeed;
}

void VideoNode::setLoop(bool loop) {
    m_impl->m_loop = loop;
    if (m_impl->m_channel) {
        m_impl->m_channel->setMode(loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
    }
}

bool VideoNode::isLooping() const {
    return m_impl->m_loop;
}

double VideoNode::getDuration() const {
    return m_impl->m_handle ? m_impl->m_handle.getDurationInSeconds() : 0.0;
}

double VideoNode::getCurrentTime() const {
    return m_impl->m_currentTime;
}

void VideoNode::seek(double time) {
    if (!m_impl->m_handle) {
        return;
    }

    time = std::clamp(time, 0.0, this->getDuration());
    if (!m_impl->m_handle.seek(time)) {
        return;
    }

    m_impl->m_currentTime = time;
    m_impl->m_nextFrameTime = m_impl->m_currentTime + m_impl->m_frameTime;

    // read frame after seeking
    if (!m_impl->m_handle.readNextFrame(m_impl->m_frameData)) {
        return;
    }

    m_impl->uploadTextureData();

    // update audio position
    if (m_impl->m_channel) {
        unsigned int ms = static_cast<unsigned int>(m_impl->m_currentTime * 1000.0);
        m_impl->m_channel->setPosition(ms, FMOD_TIMEUNIT_MS);
    }
}

bool VideoNode::isMuted() const {
    if (!m_impl->m_channel) {
        return true;
    }

    float vol = 0.0f;
    if (m_impl->m_channel->getVolume(&vol) != FMOD_OK) {
        return true;
    }

    return vol <= 0.001f;
}

void VideoNode::setMuted(bool muted) {
    if (!m_impl->m_channel) {
        return;
    }

    m_impl->m_channel->setVolume(muted ? 0.0f : 1.0f);
}

template <typename T = std::array<float, 3>>
static void setUniform3f(cocos2d::CCGLProgram* program, const char* name, T const& values) {
    program->setUniformLocationWith3f(
        program->getUniformLocationForName(name),
        values[0], values[1], values[2]
    );
}

void VideoNode::draw() {
    if (!m_impl->m_handle) {
        return;
    }

    ccGLEnable(m_eGLServerState);
    auto program = m_pShaderProgram;
    program->use();
    program->setUniformsForBuiltins();

    // apply color space matrix
    auto& matrix = yuv420::getMatrix(m_impl->m_handle.getColorSpace(), m_impl->m_handle.getColorRange());
    setUniform3f(program, "u_mtxR", matrix.m[0]);
    setUniform3f(program, "u_mtxG", matrix.m[1]);
    setUniform3f(program, "u_mtxB", matrix.m[2]);
    setUniform3f(program, "u_yuvOffset", {
        m_impl->m_handle.getColorRange() == ColorRange::Full ? 0.0f : 16.0f / 255.0f,
        0.5f,
        0.5f
    });

    cocos2d::ccGLBlendFunc(m_sBlendFunc.src, m_sBlendFunc.dst);

    cocos2d::ccGLBindTexture2D(m_pobTexture->getName());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_impl->m_uvTexture->getName());

    ccGLEnableVertexAttribs(cocos2d::kCCVertexAttribFlag_PosColorTex);

    constexpr auto kQuadSize = sizeof(m_sQuad.bl);
    auto offset = (uintptr_t)&m_sQuad;

    // vertex
    constexpr auto diff1 = offsetof(cocos2d::ccV3F_C4B_T2F, vertices);
    glVertexAttribPointer(cocos2d::kCCVertexAttrib_Position, 3, GL_FLOAT, GL_FALSE, kQuadSize, reinterpret_cast<void*>(offset + diff1));

    // texCoods
    constexpr auto diff2 = offsetof(cocos2d::ccV3F_C4B_T2F, texCoords);
    glVertexAttribPointer(cocos2d::kCCVertexAttrib_TexCoords, 2, GL_FLOAT, GL_FALSE, kQuadSize, reinterpret_cast<void*>(offset + diff2));

    // color
    constexpr auto diff3 = offsetof(cocos2d::ccV3F_C4B_T2F, colors);
    glVertexAttribPointer(cocos2d::kCCVertexAttrib_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE, kQuadSize, reinterpret_cast<void*>(offset + diff3));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

IMAGE_PLUS_END_NAMESPACE
