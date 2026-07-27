#include "IBackend.hpp"

#ifdef GEODE_IS_WINDOWS

#include <algorithm>
#include <cstring>

#include <fmod.hpp>

#include <mfapi.h>
#include <mferror.h>
#include <mfobjects.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class SimpleReader : public IMFByteStream {
protected:
    std::atomic<ULONG> m_refCount{};
    size_t m_position = 0;

public:
    virtual ~SimpleReader() = default;

    // IUnknown
    HRESULT QueryInterface(REFIID riid, void** ppvObject) override {
        if (!ppvObject) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMFByteStream)) {
            *ppvObject = static_cast<IMFByteStream*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    ULONG AddRef() override { return ++m_refCount; }
    ULONG Release() override {
        auto ref = --m_refCount;
        if (ref == 0) {
            delete this;
        }
        return ref;
    }

    // IMFByteStream
    HRESULT GetCapabilities(DWORD* pdwCapabilities) override {
        if (!pdwCapabilities) return E_POINTER;
        *pdwCapabilities = MFBYTESTREAM_IS_READABLE | MFBYTESTREAM_IS_SEEKABLE;
        return S_OK;
    }

    HRESULT GetCurrentPosition(QWORD* pqwPosition) override {
        if (!pqwPosition) return E_POINTER;
        *pqwPosition = m_position;
        return S_OK;
    }

    HRESULT IsEndOfStream(BOOL* pfEndOfStream) override = 0;
    HRESULT GetLength(QWORD* pqwLength) override = 0;
    HRESULT SetCurrentPosition(QWORD qwPosition) override = 0;
    HRESULT Seek(MFBYTESTREAM_SEEK_ORIGIN SeekOrigin, LONGLONG llSeekOffset, DWORD dwSeekFlags, QWORD* pqwCurrentPosition) override = 0;

    HRESULT Read(BYTE* pb, ULONG cb, ULONG* pcbRead) override = 0;
    HRESULT BeginRead(BYTE* pb, ULONG cb, IMFAsyncCallback* pCallback, IUnknown* punkState) override = 0;
    HRESULT EndRead(IMFAsyncResult* pResult, ULONG* pcbRead) override = 0;

    HRESULT SetLength(QWORD) override { return E_NOTIMPL; }
    HRESULT Write(BYTE const*, ULONG, ULONG*) override { return STG_E_ACCESSDENIED; }
    HRESULT BeginWrite(BYTE const*, ULONG, IMFAsyncCallback*, IUnknown*) override { return E_NOTIMPL; }
    HRESULT EndWrite(IMFAsyncResult*, ULONG*) override { return E_NOTIMPL; }

    HRESULT Flush() override { return S_OK; }
    HRESULT Close() override { return S_OK; }
};

class BufferReader final : public SimpleReader {
private:
    std::vector<uint8_t> m_buffer;
    ULONG m_lastAsyncReadCount = 0;

public:
    BufferReader() = default;
    BufferReader(std::vector<uint8_t>&& data) : m_buffer(std::move(data)) {}

    void reset(std::vector<uint8_t>&& data) {
        m_buffer = std::move(data);
        m_position = 0;
    }

    HRESULT IsEndOfStream(BOOL* pfEndOfStream) override {
        if (!pfEndOfStream) return E_POINTER;
        *pfEndOfStream = m_position >= m_buffer.size() ? TRUE : FALSE;
        return S_OK;
    }

    HRESULT GetLength(QWORD* pqwLength) override {
        if (!pqwLength) return E_POINTER;
        *pqwLength = m_buffer.size();
        return S_OK;
    }

    HRESULT SetCurrentPosition(QWORD qwPosition) override {
        if (qwPosition > m_buffer.size()) return E_INVALIDARG;
        m_position = qwPosition;
        return S_OK;
    }

    HRESULT Seek(MFBYTESTREAM_SEEK_ORIGIN SeekOrigin, LONGLONG llSeekOffset, DWORD, QWORD* pqwCurrentPosition) override {
        size_t newPos = 0;
        switch (SeekOrigin) {
            case msoBegin:
                newPos = llSeekOffset;
                break;
            case msoCurrent:
                newPos = static_cast<LONGLONG>(m_position) + llSeekOffset;
                break;
            default:
                return E_INVALIDARG;
        }

        if (newPos > m_buffer.size()) return E_INVALIDARG;

        m_position = newPos;
        if (pqwCurrentPosition) {
            *pqwCurrentPosition = m_position;
        }

        return S_OK;
    }

    HRESULT Read(BYTE* pb, ULONG cb, ULONG* pcbRead) override {
        if (!pcbRead) return E_POINTER;
        if (cb == 0) { *pcbRead = 0; return S_OK; }
        if (!pb) return E_POINTER;
        if (m_position >= m_buffer.size()) { *pcbRead = 0; return S_OK; }

        size_t remaining = m_buffer.size() - m_position;
        size_t toRead = std::min<size_t>(cb, remaining);
        if (toRead > 0) {
            std::memcpy(pb, m_buffer.data() + m_position, toRead);
            m_position += toRead;
        }

        *pcbRead = static_cast<ULONG>(toRead);
        return S_OK;
    }

    HRESULT BeginRead(BYTE* pb, ULONG cb, IMFAsyncCallback* pCallback, IUnknown* punkState) override {
        if (!pCallback) return E_POINTER;

        ULONG bytesRead = 0;
        HRESULT hrRead = this->Read(pb, cb, &bytesRead);
        if (FAILED(hrRead)) return hrRead;

        ComPtr<IMFAsyncResult> ar;
        HRESULT hr = MFCreateAsyncResult(nullptr, pCallback, punkState, &ar);
        if (FAILED(hr)) return hr;

        m_lastAsyncReadCount = bytesRead;
        return pCallback->Invoke(ar.Get());
    }

    HRESULT EndRead(IMFAsyncResult*, ULONG* pcbRead) override {
        if (!pcbRead) return E_POINTER;
        *pcbRead = m_lastAsyncReadCount;
        m_lastAsyncReadCount = 0;
        return S_OK;
    }
};

class FileReader final : public SimpleReader {
private:
    HANDLE m_file = INVALID_HANDLE_VALUE;
    size_t m_length = 0;
    ULONG m_lastAsyncReadCount = 0;

public:
    explicit FileReader(std::filesystem::path const& path) {
        m_file = CreateFileW(
            path.native().c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr
        );

        if (m_file != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER size;
            if (GetFileSizeEx(m_file, &size) && size.QuadPart >= 0) {
                m_length = static_cast<size_t>(size.QuadPart);
            } else {
                CloseHandle(m_file);
                m_file = INVALID_HANDLE_VALUE;
            }
        }
    }

    ~FileReader() override {
        if (m_file != INVALID_HANDLE_VALUE) {
            CloseHandle(m_file);
            m_file = INVALID_HANDLE_VALUE;
        }
    }

    HRESULT IsEndOfStream(BOOL* pfEndOfStream) override {
        if (!pfEndOfStream) return E_POINTER;
        *pfEndOfStream = m_position >= m_length ? TRUE : FALSE;
        return S_OK;
    }

    HRESULT GetLength(QWORD* pqwLength) override {
        if (!pqwLength) return E_POINTER;
        *pqwLength = m_length;
        return S_OK;
    }

    HRESULT SetCurrentPosition(QWORD qwPosition) override {
        if (qwPosition > m_length) return E_INVALIDARG;
        m_position = qwPosition;

        LARGE_INTEGER li;
        li.QuadPart = static_cast<LONGLONG>(m_position);
        if (!SetFilePointerEx(m_file, li, nullptr, FILE_BEGIN)) {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        return S_OK;
    }

    HRESULT Seek(MFBYTESTREAM_SEEK_ORIGIN SeekOrigin, LONGLONG llSeekOffset, DWORD, QWORD* pqwCurrentPosition) override {
        if (m_file == INVALID_HANDLE_VALUE) return MF_E_NOT_INITIALIZED;

        DWORD moveMethod;
        switch (SeekOrigin) {
            case msoBegin:
                moveMethod = FILE_BEGIN;
                break;
            case msoCurrent:
                moveMethod = FILE_CURRENT;
                break;
            default:
                return E_INVALIDARG;
        }

        LARGE_INTEGER li;
        li.QuadPart = llSeekOffset;
        if (!SetFilePointerEx(m_file, li, nullptr, moveMethod)) {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        // Update m_position
        LARGE_INTEGER newPos;
        if (!SetFilePointerEx(m_file, {0}, &newPos, FILE_CURRENT)) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        m_position = static_cast<size_t>(newPos.QuadPart);

        if (pqwCurrentPosition) {
            *pqwCurrentPosition = m_position;
        }

        return S_OK;
    }

    HRESULT Read(BYTE* pb, ULONG cb, ULONG* pcbRead) override {
        if (!pcbRead) return E_POINTER;
        if (cb == 0) { *pcbRead = 0; return S_OK; }
        if (!pb) return E_POINTER;
        if (m_file == INVALID_HANDLE_VALUE) { *pcbRead = 0; return MF_E_NOT_INITIALIZED; }
        if (m_position >= m_length) { *pcbRead = 0; return S_OK; }

        size_t remaining = m_length - m_position;
        size_t toRead = std::min<size_t>(cb, remaining);
        if (toRead > 0) {
            DWORD bytesRead = 0;
            if (!ReadFile(m_file, pb, toRead, &bytesRead, nullptr)) {
                return HRESULT_FROM_WIN32(GetLastError());
            }
            m_position += bytesRead;
            toRead = bytesRead; // actual bytes read
        }

        *pcbRead = static_cast<ULONG>(toRead);
        return S_OK;
    }

    HRESULT BeginRead(BYTE* pb, ULONG cb, IMFAsyncCallback* pCallback, IUnknown* punkState) override {
        if (!pCallback) return E_POINTER;

        ULONG bytesRead = 0;
        HRESULT hrRead = this->Read(pb, cb, &bytesRead);
        if (FAILED(hrRead)) return hrRead;

        ComPtr<IMFAsyncResult> ar;
        HRESULT hr = MFCreateAsyncResult(nullptr, pCallback, punkState, &ar);
        if (FAILED(hr)) return hr;

        m_lastAsyncReadCount = bytesRead;
        return pCallback->Invoke(ar.Get());
    }

    HRESULT EndRead(IMFAsyncResult*, ULONG* pcbRead) override {
        if (!pcbRead) return E_POINTER;
        *pcbRead = m_lastAsyncReadCount;
        m_lastAsyncReadCount = 0;
        return S_OK;
    }
};

// TODO: load functions without linking
struct {


} g_mediaFoundation;

static bool InitMediaFoundation() {
    static bool initialized = false;
    static bool initOnce = false;

    if (!initOnce) {
        HRESULT hr = MFStartup(MF_VERSION);
        if (SUCCEEDED(hr)) {
            initialized = true;
        } else {
            geode::log::error("Failed to initialize Media Foundation: 0x{:08X}", hr);
        }
        initOnce = true;
    }

    return initialized;
}

struct VideoHandle::Impl {
    ComPtr<IMFSourceReader> m_reader;
    ComPtr<IMFMediaType> m_videoType;
    ComPtr<IMFMediaType> m_outputType;
    ComPtr<IMFByteStream> m_byteStream;

    DWORD m_videoStreamIndex = static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM);
    DWORD m_audioStreamIndex = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);

    // == Video properties ==

    // Visible size
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // Buffer size (with padding)
    uint32_t m_bufferWidth = 0;
    uint32_t m_bufferHeight = 0;

    // Strides
    uint32_t m_strideY = 0;
    uint32_t m_strideUV = 0;

    ColorSpace m_colorSpace = ColorSpace::BT_709;
    ColorRange m_colorRange = ColorRange::Limited;

    uint32_t m_fpsNumerator = 0;
    uint32_t m_fpsDenominator = 1;

    uint64_t m_duration = 0; // in 100-nanosecond units
    uint64_t m_currentPosition = 0;
    bool m_eof = false;

    // == Audio properties ==
    ComPtr<IMFSourceReader> m_audioReader;
    FMOD::Sound* m_sound = nullptr;
    FMOD::Channel* m_channel = nullptr;
    std::mutex m_audioMutex;

    bool m_hasAudio = false;
    uint32_t m_audioSampleRate = 0;
    uint32_t m_audioChannels = 0;
    uint32_t m_audioBitsPerSample = 0;
    std::vector<uint8_t> m_audioData;

    geode::Result<> init();
    geode::Result<> initAudio();

    static FMOD_RESULT readAudioData(FMOD_SOUND* sound, void* data, unsigned int length);
    static FMOD_RESULT setAudioPosition(FMOD_SOUND* sound, int subsound, unsigned int position, FMOD_TIMEUNIT postype);

    ~Impl() {
        if (m_sound) {
            m_sound->release();
            m_sound = nullptr;
        }
        if (m_channel) {
            m_channel->stop();
            m_channel = nullptr;
        }
    }
};

VideoHandle::VideoHandle(Impl* impl) : m_impl(impl) {}

geode::Result<VideoHandle> VideoHandle::createFromMemory(std::vector<uint8_t>&& data) {
    if (data.empty()) {
        return geode::Err("Data buffer is empty");
    }

    auto impl = new Impl();
    impl->m_byteStream = ComPtr<IMFByteStream>(new BufferReader(std::move(data)));

    if (GEODE_UNWRAP_IF_ERR(err, impl->init())) {
        delete impl;
        return geode::Err(std::move(err));
    }

    return geode::Ok(VideoHandle(impl));
}

geode::Result<VideoHandle> VideoHandle::createFromFile(std::filesystem::path const& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return geode::Err("File does not exist");
    }

    if (!std::filesystem::is_regular_file(path, ec) || ec) {
        return geode::Err("Path is not a regular file");
    }

    auto impl = new Impl();
    impl->m_byteStream = ComPtr<IMFByteStream>(new FileReader(path));
    if (GEODE_UNWRAP_IF_ERR(err, impl->init())) {
        delete impl;
        return geode::Err(std::move(err));
    }

    return geode::Ok(VideoHandle(impl));
}

#define UNWRAP_HR(expr, msg) if (auto hr = (expr); FAILED(hr)) \
    return geode::Err(fmt::format(msg ": 0x{:08X}", static_cast<uint32_t>(hr)));

geode::Result<> VideoHandle::Impl::init() {
    if (!InitMediaFoundation()) {
        return geode::Err("Failed to initialize Media Foundation");
    }

    ComPtr<IMFAttributes> attrs;
    UNWRAP_HR(MFCreateAttributes(&attrs, 2),
        "Failed to create attributes");
    UNWRAP_HR(attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE),
        "Failed to enable video processing");

    UNWRAP_HR(MFCreateSourceReaderFromByteStream(m_byteStream.Get(), attrs.Get(), &m_reader),
        "Failed to create source reader");
    UNWRAP_HR(m_reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE),
        "Failed to deselect all streams");
    UNWRAP_HR(m_reader->SetStreamSelection(m_videoStreamIndex, TRUE),
        "Failed to select video stream");
    UNWRAP_HR(m_reader->GetNativeMediaType(m_videoStreamIndex, 0, &m_videoType),
        "Failed to get native media type");

    // Extract video properties
    UNWRAP_HR(MFGetAttributeSize(m_videoType.Get(), MF_MT_FRAME_SIZE, &m_width, &m_height),
        "Failed to get frame size");
    UNWRAP_HR(MFGetAttributeRatio(m_videoType.Get(), MF_MT_FRAME_RATE, &m_fpsNumerator, &m_fpsDenominator),
        "Failed to get frame rate");

    // Create output media type
    UNWRAP_HR(MFCreateMediaType(&m_outputType),
        "Failed to create output media type");
    UNWRAP_HR(m_outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
        "Failed to set major type");
    UNWRAP_HR(m_outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12),
        "Failed to set subtype");
    UNWRAP_HR(MFSetAttributeSize(m_outputType.Get(), MF_MT_FRAME_SIZE, m_width, m_height),
        "Failed to set output frame size");
    UNWRAP_HR(m_reader->SetCurrentMediaType(m_videoStreamIndex, nullptr, m_outputType.Get()),
        "Failed to set current media type");

    ComPtr<IMFMediaType> currentType;
    UNWRAP_HR(m_reader->GetCurrentMediaType(m_videoStreamIndex, &currentType),
        "Failed to get current media type");

    // Color range
    {
        uint32_t range = MFGetAttributeUINT32(currentType.Get(), MF_MT_VIDEO_NOMINAL_RANGE, 0);
        if (!range) range = MFGetAttributeUINT32(m_videoType.Get(), MF_MT_VIDEO_NOMINAL_RANGE, 0);

        if (range == MFNominalRange_0_255 || range == MFNominalRange_48_208) {
            m_colorRange = ColorRange::Full;
        } else if (range == MFNominalRange_16_235 || range == MFNominalRange_64_127) {
            m_colorRange = ColorRange::Limited;
        } else {
            m_colorRange = ColorRange::Limited;
        }

        (void)m_outputType->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, range ? range : MFNominalRange_16_235);
    }

    // Color space
    {
        uint32_t mtx = MFGetAttributeUINT32(currentType.Get(), MF_MT_YUV_MATRIX, 0);
        if (!mtx) mtx = MFGetAttributeUINT32(m_videoType.Get(), MF_MT_YUV_MATRIX, 0);

        if (!mtx) {
            uint32_t prim = MFGetAttributeUINT32(currentType.Get(), MF_MT_VIDEO_PRIMARIES, 0);
            if (!prim) prim = MFGetAttributeUINT32(m_videoType.Get(), MF_MT_VIDEO_PRIMARIES, 0);
            if (prim == MFVideoPrimaries_BT709) {
                mtx = MFVideoTransferMatrix_BT709;
            } else if (prim == MFVideoPrimaries_BT2020) {
                mtx = MFVideoTransferMatrix_BT2020_10;
            }
        }

        if (mtx == MFVideoTransferMatrix_BT709) {
            m_colorSpace = ColorSpace::BT_709;
        } else if (mtx == MFVideoTransferMatrix_BT2020_10 || mtx == MFVideoTransferMatrix_BT2020_12) {
            m_colorSpace = ColorSpace::BT_2020;
        } else if (mtx == MFVideoTransferMatrix_BT601) {
            m_colorSpace = ColorSpace::BT_601;
        } else {
            if (m_width >= 3840 || m_height >= 2160) {
                m_colorSpace = ColorSpace::BT_2020;
            } else if (m_height >= 720 || m_width >= 1280) {
                m_colorSpace = ColorSpace::BT_709;
            } else {
                m_colorSpace = ColorSpace::BT_601;
            }
        }

        if (mtx) (void)m_outputType->SetUINT32(MF_MT_YUV_MATRIX, mtx);
    }

    // Get duration
    ComPtr<IMFPresentationDescriptor> presDesc;
    if (SUCCEEDED(m_reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, nullptr))) {
        PROPVARIANT var;
        PropVariantInit(&var);
        if (SUCCEEDED(m_reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var)) && var.vt == VT_UI8) {
            m_duration = var.uhVal.QuadPart;
        }
        PropVariantClear(&var);
    }

    // force 30fps if something is wrong
    if (m_fpsDenominator == 0 || m_fpsNumerator == 0) {
        m_fpsNumerator = 30;
        m_fpsDenominator = 1;
        geode::log::warn("Invalid frame rate detected, defaulting to 30 fps");
    }

    auto res = this->initAudio();
    if (res.isErr()) {
        geode::log::warn("Failed to initialize audio: {}", res.unwrapErr());
    }

    return geode::Ok();
}

geode::Result<> VideoHandle::Impl::initAudio() {
    // Check for audio stream
    DWORD streamIndex = MF_SOURCE_READER_FIRST_AUDIO_STREAM;
    ComPtr<IMFMediaType> audioType;
    if (SUCCEEDED(m_reader->GetNativeMediaType(streamIndex, 0, &audioType))) {
        m_hasAudio = true;
        m_audioStreamIndex = streamIndex;

        // create new reader for audio
        UNWRAP_HR(MFCreateSourceReaderFromByteStream(m_byteStream.Get(), nullptr, &m_audioReader),
            "Failed to create source reader for audio");
        UNWRAP_HR(m_audioReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE),
            "Failed to deselect all streams in audio reader");
        UNWRAP_HR(m_audioReader->SetStreamSelection(m_audioStreamIndex, TRUE),
            "Failed to select audio stream in audio reader");
        UNWRAP_HR(m_audioReader->SetCurrentMediaType(m_audioStreamIndex, nullptr, audioType.Get()),
            "Failed to set current media type for audio stream");

        ComPtr<IMFMediaType> currentAudioType;
        UNWRAP_HR(m_audioReader->GetCurrentMediaType(m_audioStreamIndex, &currentAudioType),
            "Failed to get current media type for audio stream");

        // Extract audio properties
        UNWRAP_HR(currentAudioType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &m_audioSampleRate),
            "Failed to get audio sample rate");
        UNWRAP_HR(currentAudioType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &m_audioChannels),
            "Failed to get audio channel count");
        UNWRAP_HR(currentAudioType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &m_audioBitsPerSample),
            "Failed to get audio bits per sample");

        // check which type of audio format
        GUID subtype = {0};
        UNWRAP_HR(currentAudioType->GetGUID(MF_MT_SUBTYPE, &subtype),
            "Failed to get audio subtype");
        if (subtype != MFAudioFormat_PCM && subtype != MFAudioFormat_Float) {
            return geode::Err("Unsupported audio format (only PCM and Float are supported)");
        }

        geode::log::debug("Audio stream detected: {} Hz, {} channels, {} bits per sample, format: {}",
            m_audioSampleRate, m_audioChannels, m_audioBitsPerSample, (subtype == MFAudioFormat_PCM) ? "PCM" : "Float");

        // Create FMOD sound
        FMOD_CREATESOUNDEXINFO exinfo = {};
        exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
        exinfo.numchannels = static_cast<int>(m_audioChannels);
        exinfo.defaultfrequency = static_cast<int>(m_audioSampleRate);
        exinfo.format = subtype == MFAudioFormat_PCM ?
            (m_audioBitsPerSample == 16 ? FMOD_SOUND_FORMAT_PCM16 :
             m_audioBitsPerSample == 24 ? FMOD_SOUND_FORMAT_PCM24 :
             m_audioBitsPerSample == 32 ? FMOD_SOUND_FORMAT_PCM32 : FMOD_SOUND_FORMAT_PCM16) :
            FMOD_SOUND_FORMAT_PCMFLOAT;

        exinfo.length = m_audioSampleRate * m_audioChannels * (m_audioBitsPerSample / 8)
            * (m_duration / 10000000.0); // duration in seconds

        exinfo.userdata = this;
        exinfo.pcmreadcallback = &Impl::readAudioData;

        auto system = FMODAudioEngine::get()->m_system;
        auto res = system->createSound(
            nullptr,
            FMOD_OPENUSER | FMOD_LOOP_NORMAL,
            &exinfo,
            &m_sound
        );

        if (res != FMOD_OK) {
            m_hasAudio = false;
            m_sound = nullptr;
            return geode::Err("Failed to create FMOD sound for audio stream: {}", (int)res);
        }
    } else {
        m_hasAudio = false;
    }

    return geode::Ok();
}

FMOD_RESULT VideoHandle::Impl::readAudioData(FMOD_SOUND* sound_, void* data, unsigned int length) {
    auto sound = reinterpret_cast<FMOD::Sound*>(sound_);
    Impl* self = nullptr;

    if (sound->getUserData(reinterpret_cast<void**>(&self)) != FMOD_OK || !self) {
        return FMOD_ERR_INVALID_PARAM;
    }

    std::unique_lock lock(self->m_audioMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        std::memset(data, 0, length);
        return FMOD_OK;
    }

    if (!self->m_audioReader) {
        std::memset(data, 0, length);
        return FMOD_OK;
    }

    unsigned int totalRead = 0;
    while (totalRead < length) {
        ComPtr<IMFSample> sample;
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        HRESULT hr = self->m_audioReader->ReadSample(
            self->m_audioStreamIndex,
            0,
            nullptr,
            &flags,
            &timestamp,
            &sample
        );

        if (FAILED(hr) || flags & MF_SOURCE_READERF_ENDOFSTREAM || !sample) {
            std::memset(static_cast<uint8_t*>(data) + totalRead, 0, length - totalRead);
            return FMOD_OK;
        }

        ComPtr<IMFMediaBuffer> buffer;
        hr = sample->ConvertToContiguousBuffer(&buffer);
        if (FAILED(hr)) {
            std::memset(static_cast<uint8_t*>(data) + totalRead, 0, length - totalRead);
            return FMOD_OK;
        }

        DWORD bufLen = 0;
        BYTE* bufData = nullptr;
        hr = buffer->Lock(&bufData, nullptr, &bufLen);
        if (FAILED(hr)) {
            std::memset(static_cast<uint8_t*>(data) + totalRead, 0, length - totalRead);
            return FMOD_OK;
        }

        if (bufData && bufLen > 0) {
            size_t toCopy = std::min<size_t>(bufLen, length - totalRead);
            std::memcpy(static_cast<uint8_t*>(data) + totalRead, bufData, toCopy);
            totalRead += static_cast<unsigned int>(toCopy);
        }

        buffer->Unlock();
    }

    return FMOD_OK;
}

bool VideoHandle::readNextFrame(std::vector<uint8_t>& frameData) {
    if (!m_impl) {
        return false;
    }

    ComPtr<IMFSample> sample;
    DWORD streamIndex = 0, flags = 0;
    LONGLONG timestamp = 0;

    HRESULT hr = m_impl->m_reader->ReadSample(
        m_impl->m_videoStreamIndex,
        0,
        &streamIndex,
        &flags,
        &timestamp,
        &sample
    );

    if (FAILED(hr)) {
        return false;
    }

    if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
        m_impl->m_eof = true;
        return false;
    }

    if (!sample) {
        return false;
    }

    m_impl->m_currentPosition = timestamp;

    ComPtr<IMFMediaBuffer> buffer;
    hr = sample->ConvertToContiguousBuffer(&buffer);
    if (FAILED(hr)) {
        return false;
    }

    // Read buffer data
    ComPtr<IMF2DBuffer> buf2d;
    if (SUCCEEDED(buffer.As(&buf2d)) && buf2d) {
        BYTE* base = nullptr;
        LONG pitch = 0;
        hr = buf2d->Lock2D(&base, &pitch);
        if (SUCCEEDED(hr) && base && pitch > 0) {
            DWORD totalLen = 0;
            (void)buffer->GetCurrentLength(&totalLen);

            uint32_t w = m_impl->m_width;
            uint32_t h = m_impl->m_height;
            size_t hEven = (static_cast<size_t>(h) + 1u) & ~1u;

            size_t alignedH = hEven;
            if (totalLen > 0) {
                size_t est = (static_cast<size_t>(totalLen) * 2u) / (static_cast<size_t>(pitch) * 3u);
                est &= ~1u; // even
                if (est < hEven) est = hEven;
                auto totalFor = [&](size_t H){ return static_cast<size_t>(pitch) * (H + (H >> 1) + (H & 1 ? 1 : 0)); };
                while (est >= hEven && totalFor(est) > static_cast<size_t>(totalLen)) est -= 2u;
                alignedH = (est >= hEven) ? est : hEven;
            }

            m_impl->m_strideY = static_cast<uint32_t>(pitch);
            m_impl->m_strideUV = static_cast<uint32_t>(pitch);
            m_impl->m_bufferWidth = static_cast<uint32_t>(pitch);
            m_impl->m_bufferHeight = static_cast<uint32_t>(alignedH);

            size_t yBytes = static_cast<size_t>(pitch) * alignedH;
            size_t uvBytes = static_cast<size_t>(pitch) * (alignedH >> 1);
            frameData.resize(yBytes + uvBytes);

            std::memcpy(frameData.data(), base, yBytes + uvBytes);

            buf2d->Unlock2D();
            return true;
        }

        if (SUCCEEDED(hr)) {
            buf2d->Unlock2D();
        }
    }

    // geode::log::warn("Failed to use IMF2DBuffer");

    // try to read the buffer instead
    BYTE* data = nullptr;
    DWORD maxLength = 0, currentLength = 0;

    hr = buffer->Lock(&data, &maxLength, &currentLength);
    if (FAILED(hr)) {
        return false;
    }

    uint32_t w = m_impl->m_width;
    uint32_t h = m_impl->m_height;
    uint32_t uvW = (w + 1) / 2;
    uint32_t uvH = (h + 1) / 2;
    size_t ySize = static_cast<size_t>(w) * h;
    size_t uvRowBytes = static_cast<size_t>(uvW) * 2;
    size_t tightExpected = ySize + uvRowBytes * uvH;

    if (currentLength == tightExpected) {
        frameData.resize(tightExpected);
        std::memcpy(frameData.data(), data, tightExpected);

        m_impl->m_strideY = w;
        m_impl->m_strideUV = static_cast<uint32_t>(uvRowBytes);
        m_impl->m_bufferWidth = w;
        m_impl->m_bufferHeight = h;
    } else {
        // handle stride/padding
        size_t hEven = static_cast<size_t>(h) + 1u & ~1u;
        size_t denom = hEven + (hEven >> 1); // H + H/2

        bool strideFromLenUsed = false;
        if (denom > 0) {
            size_t inferred = static_cast<size_t>(currentLength) / denom;
            if (inferred * denom == static_cast<size_t>(currentLength) && inferred >= static_cast<size_t>(w)) {
                m_impl->m_strideY = static_cast<uint32_t>(inferred);
                m_impl->m_strideUV = static_cast<uint32_t>(inferred);
                strideFromLenUsed = true;
            }
        }

        if (!strideFromLenUsed) {
            LONG defStride = 0;
            if (SUCCEEDED(MFGetStrideForBitmapInfoHeader(MFVideoFormat_NV12.Data1, w, &defStride)) && defStride > 0) {
                m_impl->m_strideY = defStride;
                m_impl->m_strideUV = defStride;
            } else {
                LONG attrStride = 0;
                m_impl->m_outputType->GetUINT32(MF_MT_DEFAULT_STRIDE, reinterpret_cast<uint32_t*>(&attrStride));
                m_impl->m_strideY = attrStride > 0 ? attrStride : w;
                m_impl->m_strideUV = m_impl->m_strideY;
            }
        }

        size_t hEven2 = static_cast<size_t>(h) + 1u & ~1u;
        size_t alignedH = hEven2;

        if (m_impl->m_strideY > 0 && currentLength > 0) {
            if (strideFromLenUsed) {
                alignedH = hEven2;
            } else {
                geode::log::warn("i'm here");
                size_t est = (static_cast<size_t>(currentLength) * 2u) / (static_cast<size_t>(m_impl->m_strideY) * 3u);
                est &= ~1u;
                if (est < hEven2) est = hEven2;
                auto totalFor = [&](size_t H) {
                    return static_cast<size_t>(m_impl->m_strideY) * (H + (H >> 1) + (H & 1 ? 1 : 0));
                };
                while (est >= hEven2 && totalFor(est) > static_cast<size_t>(currentLength)) est -= 2u;
                alignedH = (est >= hEven2) ? est : hEven2;
            }
        }

        m_impl->m_bufferWidth = m_impl->m_strideY;
        m_impl->m_bufferHeight = static_cast<uint32_t>(alignedH);

        size_t yBytes = static_cast<size_t>(m_impl->m_strideY) * alignedH;
        size_t uvBytes = static_cast<size_t>(m_impl->m_strideUV) * (alignedH >> 1);

        frameData.resize(yBytes + uvBytes);

        auto toCopy = std::min<size_t>(currentLength, yBytes);
        std::memcpy(frameData.data(), data, toCopy);
        if (currentLength > yBytes) {
            auto uvCopy = std::min<size_t>(currentLength - yBytes, uvBytes);
            std::memcpy(frameData.data() + yBytes, data + yBytes, uvCopy);
            if (uvCopy < uvBytes) {
                std::memset(frameData.data() + yBytes + uvCopy, 0, uvBytes - uvCopy);
            }
        } else {
            std::memset(frameData.data() + yBytes, 0, uvBytes);
        }
    }

    buffer->Unlock();
    return true;
}

bool VideoHandle::seek(double timeInSeconds) {
    if (!m_impl || !m_impl->m_reader) {
        return false;
    }

    LONGLONG time100ns = static_cast<LONGLONG>(timeInSeconds * 10'000'000.0);

    PROPVARIANT var;
    PropVariantInit(&var);
    var.vt = VT_I8;
    var.hVal.QuadPart = time100ns;

    HRESULT hr = m_impl->m_reader->SetCurrentPosition(GUID_NULL, var);
    PropVariantClear(&var);

    if (FAILED(hr)) {
        return false;
    }

    m_impl->m_currentPosition = time100ns;
    m_impl->m_eof = false;
    return true;
}

bool VideoHandle::seekToFrame(uint64_t frameNumber) {
    if (!m_impl || m_impl->m_fpsDenominator == 0) {
        return false;
    }

    double timePerFrame = static_cast<double>(m_impl->m_fpsDenominator) / m_impl->m_fpsNumerator;
    return seek(frameNumber * timePerFrame);
}

ColorSpace VideoHandle::getColorSpace() const {
    return m_impl->m_colorSpace;
}

ColorRange VideoHandle::getColorRange() const {
    return m_impl->m_colorRange;
}

uint32_t VideoHandle::getWidth() const {
    return m_impl->m_width;
}

uint32_t VideoHandle::getHeight() const {
    return m_impl->m_height;
}

uint32_t VideoHandle::getBufferWidth() const {
    return m_impl->m_bufferWidth ? m_impl->m_bufferWidth : m_impl->m_width;
}

uint32_t VideoHandle::getBufferHeight() const {
    return m_impl->m_bufferHeight ? m_impl->m_bufferHeight : (m_impl->m_height + 1 & ~1);
}

double VideoHandle::getFps() const {
    return m_impl->m_fpsDenominator > 0 ? static_cast<double>(m_impl->m_fpsNumerator) / m_impl->m_fpsDenominator : 30.0;
}

double VideoHandle::getFrameTime() const {
    return m_impl->m_fpsDenominator > 0 ? static_cast<double>(m_impl->m_fpsDenominator) / m_impl->m_fpsNumerator : 1.0 / 30.0;
}

double VideoHandle::getDurationInSeconds() const {
    return m_impl->m_duration / 10'000'000.0;
}

FMOD::Sound* VideoHandle::getSound() const {
    return m_impl->m_sound;
}

void VideoHandle::assignChannel(FMOD::Channel* channel) {
    m_impl->m_channel = channel;
}

bool VideoHandle::hasAudio() const {
    return m_impl->m_hasAudio;
}

uint32_t VideoHandle::getAudioSampleRate() const {
    return m_impl->m_audioSampleRate;
}

uint32_t VideoHandle::getAudioChannels() const {
    return m_impl->m_audioChannels;
}

uint32_t VideoHandle::getAudioBitsPerSample() const {
    return m_impl->m_audioBitsPerSample;
}

uint8_t const* VideoHandle::getAudioData() const {
    return m_impl->m_audioData.data();
}

size_t VideoHandle::getAudioDataSize() const {
    return m_impl->m_audioData.size();
}

void VideoHandle::destroyImpl(Impl* impl) {
    delete impl;
}

#endif
