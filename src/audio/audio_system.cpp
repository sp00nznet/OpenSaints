#include "audio_system.h"
#include <iostream>
#include <cstring>
#include <algorithm>

#ifdef HAVE_OPENAL
#include <AL/al.h>
#include <AL/alc.h>
#endif

namespace opensaints {

// Global audio system instance
static std::unique_ptr<AudioSystem> g_audioSystem;

AudioSystem& getAudioSystem() {
    if (!g_audioSystem) {
        g_audioSystem = std::make_unique<AudioSystem>();
    }
    return *g_audioSystem;
}

AudioSystem::AudioSystem() {
    m_channelVolumes[AudioChannel::Master] = 1.0f;
    m_channelVolumes[AudioChannel::Music] = 1.0f;
    m_channelVolumes[AudioChannel::SFX] = 1.0f;
    m_channelVolumes[AudioChannel::Voice] = 1.0f;
    m_channelVolumes[AudioChannel::Ambient] = 1.0f;
}

AudioSystem::~AudioSystem() {
    shutdown();
}

#ifdef HAVE_OPENAL

bool AudioSystem::initialize(const AudioConfig& config) {
    if (m_initialized) return true;

    m_config = config;

    // Open default audio device
    m_device = alcOpenDevice(nullptr);
    if (!m_device) {
        std::cerr << "Failed to open OpenAL device\n";
        return false;
    }

    // Create context
    m_context = alcCreateContext(static_cast<ALCdevice*>(m_device), nullptr);
    if (!m_context) {
        std::cerr << "Failed to create OpenAL context\n";
        alcCloseDevice(static_cast<ALCdevice*>(m_device));
        m_device = nullptr;
        return false;
    }

    alcMakeContextCurrent(static_cast<ALCcontext*>(m_context));

    // Set doppler effect
    alDopplerFactor(config.dopplerFactor);
    alSpeedOfSound(config.speedOfSound);

    // Initialize listener
    setListener(m_listener);

    // Pre-create source pool
    for (int i = 0; i < std::min(config.maxSources, 32); ++i) {
        AudioSourceHandle handle = createSource();
        if (handle != InvalidAudioSource) {
            m_availableSources.push(handle);
        }
    }

    m_initialized = true;
    std::cout << "Audio system initialized (OpenAL)\n";
    return true;
}

void AudioSystem::shutdown() {
    if (!m_initialized) return;

    // Stop all sources
    stopAll();

    // Destroy all sources
    for (auto& [handle, source] : m_sources) {
        alDeleteSources(1, &source.alSource);
    }
    m_sources.clear();

    // Destroy all buffers
    for (auto& [handle, buffer] : m_buffers) {
        alDeleteBuffers(1, &buffer.alBuffer);
    }
    m_buffers.clear();

    // Clean up context and device
    alcMakeContextCurrent(nullptr);
    if (m_context) {
        alcDestroyContext(static_cast<ALCcontext*>(m_context));
        m_context = nullptr;
    }
    if (m_device) {
        alcCloseDevice(static_cast<ALCdevice*>(m_device));
        m_device = nullptr;
    }

    m_initialized = false;
    std::cout << "Audio system shutdown\n";
}

AudioBufferHandle AudioSystem::createBuffer(const void* data, size_t size, AudioFormat format, int sampleRate) {
    if (!m_initialized) return InvalidAudioBuffer;

    ALuint alBuffer;
    alGenBuffers(1, &alBuffer);

    ALenum alFormat;
    switch (format) {
        case AudioFormat::Mono8: alFormat = AL_FORMAT_MONO8; break;
        case AudioFormat::Mono16: alFormat = AL_FORMAT_MONO16; break;
        case AudioFormat::Stereo8: alFormat = AL_FORMAT_STEREO8; break;
        case AudioFormat::Stereo16: alFormat = AL_FORMAT_STEREO16; break;
        default: alFormat = AL_FORMAT_MONO16; break;
    }

    alBufferData(alBuffer, alFormat, data, static_cast<ALsizei>(size), sampleRate);

    if (alGetError() != AL_NO_ERROR) {
        alDeleteBuffers(1, &alBuffer);
        return InvalidAudioBuffer;
    }

    AudioBufferHandle handle = m_nextBufferHandle++;
    ManagedBuffer managed;
    managed.alBuffer = alBuffer;
    managed.size = size;
    managed.format = format;
    managed.sampleRate = sampleRate;
    m_buffers[handle] = managed;

    return handle;
}

void AudioSystem::destroyBuffer(AudioBufferHandle buffer) {
    auto it = m_buffers.find(buffer);
    if (it != m_buffers.end()) {
        alDeleteBuffers(1, &it->second.alBuffer);
        m_buffers.erase(it);
    }
}

AudioSourceHandle AudioSystem::createSource() {
    if (!m_initialized) return InvalidAudioSource;

    ALuint alSource;
    alGenSources(1, &alSource);

    if (alGetError() != AL_NO_ERROR) {
        return InvalidAudioSource;
    }

    AudioSourceHandle handle = m_nextSourceHandle++;
    ManagedSource managed;
    managed.alSource = alSource;
    m_sources[handle] = managed;

    return handle;
}

void AudioSystem::destroySource(AudioSourceHandle source) {
    auto it = m_sources.find(source);
    if (it != m_sources.end()) {
        alDeleteSources(1, &it->second.alSource);
        m_sources.erase(it);
    }
}

void AudioSystem::setSourceBuffer(AudioSourceHandle source, AudioBufferHandle buffer) {
    auto srcIt = m_sources.find(source);
    auto bufIt = m_buffers.find(buffer);

    if (srcIt != m_sources.end() && bufIt != m_buffers.end()) {
        alSourcei(srcIt->second.alSource, AL_BUFFER, bufIt->second.alBuffer);
        srcIt->second.buffer = buffer;
    }
}

void AudioSystem::play(AudioSourceHandle source) {
    auto it = m_sources.find(source);
    if (it != m_sources.end()) {
        alSourcePlay(it->second.alSource);
    }
}

void AudioSystem::pause(AudioSourceHandle source) {
    auto it = m_sources.find(source);
    if (it != m_sources.end()) {
        alSourcePause(it->second.alSource);
    }
}

void AudioSystem::stop(AudioSourceHandle source) {
    auto it = m_sources.find(source);
    if (it != m_sources.end()) {
        alSourceStop(it->second.alSource);
    }
}

void AudioSystem::rewind(AudioSourceHandle source) {
    auto it = m_sources.find(source);
    if (it != m_sources.end()) {
        alSourceRewind(it->second.alSource);
    }
}

void AudioSystem::setSourceParams(AudioSourceHandle source, const AudioSourceParams& params) {
    auto it = m_sources.find(source);
    if (it == m_sources.end()) return;

    ALuint alSource = it->second.alSource;

    alSource3f(alSource, AL_POSITION, params.position[0], params.position[1], params.position[2]);
    alSource3f(alSource, AL_VELOCITY, params.velocity[0], params.velocity[1], params.velocity[2]);
    alSource3f(alSource, AL_DIRECTION, params.direction[0], params.direction[1], params.direction[2]);

    it->second.baseGain = params.gain;
    updateSourceGain(source);

    alSourcef(alSource, AL_PITCH, params.pitch);
    alSourcef(alSource, AL_REFERENCE_DISTANCE, params.minDistance);
    alSourcef(alSource, AL_MAX_DISTANCE, params.maxDistance);
    alSourcef(alSource, AL_ROLLOFF_FACTOR, params.rolloffFactor);
    alSourcef(alSource, AL_CONE_INNER_ANGLE, params.coneInnerAngle);
    alSourcef(alSource, AL_CONE_OUTER_ANGLE, params.coneOuterAngle);
    alSourcef(alSource, AL_CONE_OUTER_GAIN, params.coneOuterGain);
    alSourcei(alSource, AL_LOOPING, params.looping ? AL_TRUE : AL_FALSE);
    alSourcei(alSource, AL_SOURCE_RELATIVE, params.relative ? AL_TRUE : AL_FALSE);
}

void AudioSystem::setSourcePosition(AudioSourceHandle source, float x, float y, float z) {
    auto it = m_sources.find(source);
    if (it != m_sources.end()) {
        alSource3f(it->second.alSource, AL_POSITION, x, y, z);
    }
}

void AudioSystem::setSourceVelocity(AudioSourceHandle source, float x, float y, float z) {
    auto it = m_sources.find(source);
    if (it != m_sources.end()) {
        alSource3f(it->second.alSource, AL_VELOCITY, x, y, z);
    }
}

void AudioSystem::setSourceGain(AudioSourceHandle source, float gain) {
    auto it = m_sources.find(source);
    if (it != m_sources.end()) {
        it->second.baseGain = gain;
        updateSourceGain(source);
    }
}

void AudioSystem::setSourcePitch(AudioSourceHandle source, float pitch) {
    auto it = m_sources.find(source);
    if (it != m_sources.end()) {
        alSourcef(it->second.alSource, AL_PITCH, pitch);
    }
}

void AudioSystem::setSourceLooping(AudioSourceHandle source, bool looping) {
    auto it = m_sources.find(source);
    if (it != m_sources.end()) {
        alSourcei(it->second.alSource, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
    }
}

void AudioSystem::setSourceChannel(AudioSourceHandle source, AudioChannel channel) {
    auto it = m_sources.find(source);
    if (it != m_sources.end()) {
        it->second.channel = channel;
        updateSourceGain(source);
    }
}

AudioSourceState AudioSystem::getSourceState(AudioSourceHandle source) const {
    auto it = m_sources.find(source);
    if (it == m_sources.end()) return AudioSourceState::Stopped;

    ALint state;
    alGetSourcei(it->second.alSource, AL_SOURCE_STATE, &state);

    switch (state) {
        case AL_INITIAL: return AudioSourceState::Initial;
        case AL_PLAYING: return AudioSourceState::Playing;
        case AL_PAUSED: return AudioSourceState::Paused;
        case AL_STOPPED: return AudioSourceState::Stopped;
        default: return AudioSourceState::Stopped;
    }
}

float AudioSystem::getSourcePlaybackPosition(AudioSourceHandle source) const {
    auto it = m_sources.find(source);
    if (it == m_sources.end()) return 0;

    ALfloat pos;
    alGetSourcef(it->second.alSource, AL_SEC_OFFSET, &pos);
    return pos;
}

bool AudioSystem::isSourcePlaying(AudioSourceHandle source) const {
    return getSourceState(source) == AudioSourceState::Playing;
}

void AudioSystem::setListener(const AudioListener& listener) {
    m_listener = listener;

    alListener3f(AL_POSITION, listener.position[0], listener.position[1], listener.position[2]);
    alListener3f(AL_VELOCITY, listener.velocity[0], listener.velocity[1], listener.velocity[2]);

    float orientation[6] = {
        listener.forward[0], listener.forward[1], listener.forward[2],
        listener.up[0], listener.up[1], listener.up[2]
    };
    alListenerfv(AL_ORIENTATION, orientation);
}

void AudioSystem::setListenerPosition(float x, float y, float z) {
    m_listener.position[0] = x;
    m_listener.position[1] = y;
    m_listener.position[2] = z;
    alListener3f(AL_POSITION, x, y, z);
}

void AudioSystem::setListenerVelocity(float x, float y, float z) {
    m_listener.velocity[0] = x;
    m_listener.velocity[1] = y;
    m_listener.velocity[2] = z;
    alListener3f(AL_VELOCITY, x, y, z);
}

void AudioSystem::setListenerOrientation(float forwardX, float forwardY, float forwardZ,
                                          float upX, float upY, float upZ) {
    m_listener.forward[0] = forwardX;
    m_listener.forward[1] = forwardY;
    m_listener.forward[2] = forwardZ;
    m_listener.up[0] = upX;
    m_listener.up[1] = upY;
    m_listener.up[2] = upZ;

    float orientation[6] = {forwardX, forwardY, forwardZ, upX, upY, upZ};
    alListenerfv(AL_ORIENTATION, orientation);
}

void AudioSystem::setMasterVolume(float volume) {
    m_config.masterVolume = std::clamp(volume, 0.0f, 1.0f);
    m_channelVolumes[AudioChannel::Master] = m_config.masterVolume;
    alListenerf(AL_GAIN, m_config.masterVolume);
}

void AudioSystem::setChannelVolume(AudioChannel channel, float volume) {
    m_channelVolumes[channel] = std::clamp(volume, 0.0f, 1.0f);

    // Update all sources on this channel
    for (auto& [handle, source] : m_sources) {
        if (source.channel == channel) {
            updateSourceGain(handle);
        }
    }
}

float AudioSystem::getChannelVolume(AudioChannel channel) const {
    auto it = m_channelVolumes.find(channel);
    return (it != m_channelVolumes.end()) ? it->second : 1.0f;
}

AudioSourceHandle AudioSystem::playOneShot(AudioBufferHandle buffer, float volume) {
    AudioSourceHandle source;

    if (!m_availableSources.empty()) {
        source = m_availableSources.front();
        m_availableSources.pop();
    } else {
        source = createSource();
    }

    if (source == InvalidAudioSource) return InvalidAudioSource;

    setSourceBuffer(source, buffer);
    setSourceGain(source, volume);
    setSourceLooping(source, false);

    auto it = m_sources.find(source);
    if (it != m_sources.end()) {
        it->second.oneShot = true;
    }

    m_oneShotSources.push_back(source);
    play(source);

    return source;
}

AudioSourceHandle AudioSystem::playSoundAt(AudioBufferHandle buffer, float x, float y, float z, float volume) {
    AudioSourceHandle source = playOneShot(buffer, volume);
    if (source != InvalidAudioSource) {
        setSourcePosition(source, x, y, z);
    }
    return source;
}

void AudioSystem::update(float deltaTime) {
    cleanupFinishedOneShotSources();
}

AudioStats AudioSystem::getStats() const {
    AudioStats stats = {};
    stats.buffersLoaded = static_cast<uint32_t>(m_buffers.size());
    stats.sourcesActive = static_cast<uint32_t>(m_sources.size());

    for (const auto& [handle, source] : m_sources) {
        if (isSourcePlaying(handle)) {
            stats.sourcesPlaying++;
        }
    }

    for (const auto& [handle, buffer] : m_buffers) {
        stats.memoryUsage += buffer.size;
    }

    return stats;
}

void AudioSystem::pauseAll() {
    for (auto& [handle, source] : m_sources) {
        if (isSourcePlaying(handle)) {
            pause(handle);
        }
    }
}

void AudioSystem::resumeAll() {
    for (auto& [handle, source] : m_sources) {
        ALint state;
        alGetSourcei(source.alSource, AL_SOURCE_STATE, &state);
        if (state == AL_PAUSED) {
            play(handle);
        }
    }
}

void AudioSystem::stopAll() {
    for (auto& [handle, source] : m_sources) {
        stop(handle);
    }
}

void AudioSystem::updateSourceGain(AudioSourceHandle source) {
    auto it = m_sources.find(source);
    if (it == m_sources.end()) return;

    float channelVol = getChannelVolume(it->second.channel);
    float finalGain = it->second.baseGain * channelVol;
    alSourcef(it->second.alSource, AL_GAIN, finalGain);
}

void AudioSystem::cleanupFinishedOneShotSources() {
    auto it = m_oneShotSources.begin();
    while (it != m_oneShotSources.end()) {
        if (!isSourcePlaying(*it)) {
            // Return to pool
            stop(*it);
            alSourcei(m_sources[*it].alSource, AL_BUFFER, 0); // Detach buffer
            m_sources[*it].oneShot = false;
            m_availableSources.push(*it);
            it = m_oneShotSources.erase(it);
        } else {
            ++it;
        }
    }
}

AudioBufferHandle AudioSystem::loadWAV(const std::string& path) {
    // TODO: Load from file
    std::cerr << "WAV file loading not implemented: " << path << "\n";
    return InvalidAudioBuffer;
}

AudioBufferHandle AudioSystem::loadFromMemory(const uint8_t* data, size_t size) {
    std::vector<uint8_t> pcmData;
    AudioFormat format;
    int sampleRate;

    if (!parseWAV(data, size, pcmData, format, sampleRate)) {
        return InvalidAudioBuffer;
    }

    return createBuffer(pcmData.data(), pcmData.size(), format, sampleRate);
}

bool AudioSystem::parseWAV(const uint8_t* data, size_t size, std::vector<uint8_t>& pcmData,
                           AudioFormat& format, int& sampleRate) {
    if (size < 44) return false;

    // Check RIFF header
    if (std::memcmp(data, "RIFF", 4) != 0) return false;
    if (std::memcmp(data + 8, "WAVE", 4) != 0) return false;

    // Find fmt chunk
    size_t pos = 12;
    while (pos + 8 < size) {
        uint32_t chunkId = *reinterpret_cast<const uint32_t*>(data + pos);
        uint32_t chunkSize = *reinterpret_cast<const uint32_t*>(data + pos + 4);

        if (std::memcmp(data + pos, "fmt ", 4) == 0) {
            uint16_t audioFormat = *reinterpret_cast<const uint16_t*>(data + pos + 8);
            uint16_t numChannels = *reinterpret_cast<const uint16_t*>(data + pos + 10);
            sampleRate = *reinterpret_cast<const int32_t*>(data + pos + 12);
            uint16_t bitsPerSample = *reinterpret_cast<const uint16_t*>(data + pos + 22);

            if (audioFormat != 1) return false; // Only PCM

            if (numChannels == 1) {
                format = (bitsPerSample == 8) ? AudioFormat::Mono8 : AudioFormat::Mono16;
            } else {
                format = (bitsPerSample == 8) ? AudioFormat::Stereo8 : AudioFormat::Stereo16;
            }
        } else if (std::memcmp(data + pos, "data", 4) == 0) {
            pcmData.resize(chunkSize);
            std::memcpy(pcmData.data(), data + pos + 8, chunkSize);
            return true;
        }

        pos += 8 + chunkSize;
        if (chunkSize % 2) pos++; // Align
    }

    return false;
}

#else // No OpenAL - stub implementation

bool AudioSystem::initialize(const AudioConfig& config) {
    m_config = config;
    m_initialized = true;
    std::cout << "Audio system initialized (stub - no OpenAL)\n";
    return true;
}

void AudioSystem::shutdown() {
    m_initialized = false;
}

AudioBufferHandle AudioSystem::createBuffer(const void*, size_t, AudioFormat, int) {
    return m_nextBufferHandle++;
}

AudioBufferHandle AudioSystem::loadWAV(const std::string&) { return InvalidAudioBuffer; }
AudioBufferHandle AudioSystem::loadFromMemory(const uint8_t*, size_t) { return InvalidAudioBuffer; }
void AudioSystem::destroyBuffer(AudioBufferHandle) {}

AudioSourceHandle AudioSystem::createSource() { return m_nextSourceHandle++; }
void AudioSystem::destroySource(AudioSourceHandle) {}

void AudioSystem::setSourceBuffer(AudioSourceHandle, AudioBufferHandle) {}
void AudioSystem::play(AudioSourceHandle) {}
void AudioSystem::pause(AudioSourceHandle) {}
void AudioSystem::stop(AudioSourceHandle) {}
void AudioSystem::rewind(AudioSourceHandle) {}

void AudioSystem::setSourceParams(AudioSourceHandle, const AudioSourceParams&) {}
void AudioSystem::setSourcePosition(AudioSourceHandle, float, float, float) {}
void AudioSystem::setSourceVelocity(AudioSourceHandle, float, float, float) {}
void AudioSystem::setSourceGain(AudioSourceHandle, float) {}
void AudioSystem::setSourcePitch(AudioSourceHandle, float) {}
void AudioSystem::setSourceLooping(AudioSourceHandle, bool) {}
void AudioSystem::setSourceChannel(AudioSourceHandle, AudioChannel) {}

AudioSourceState AudioSystem::getSourceState(AudioSourceHandle) const { return AudioSourceState::Stopped; }
float AudioSystem::getSourcePlaybackPosition(AudioSourceHandle) const { return 0; }
bool AudioSystem::isSourcePlaying(AudioSourceHandle) const { return false; }

void AudioSystem::setListener(const AudioListener&) {}
void AudioSystem::setListenerPosition(float, float, float) {}
void AudioSystem::setListenerVelocity(float, float, float) {}
void AudioSystem::setListenerOrientation(float, float, float, float, float, float) {}

void AudioSystem::setMasterVolume(float volume) { m_config.masterVolume = volume; }
void AudioSystem::setChannelVolume(AudioChannel channel, float volume) { m_channelVolumes[channel] = volume; }
float AudioSystem::getChannelVolume(AudioChannel channel) const {
    auto it = m_channelVolumes.find(channel);
    return (it != m_channelVolumes.end()) ? it->second : 1.0f;
}

AudioSourceHandle AudioSystem::playOneShot(AudioBufferHandle, float) { return InvalidAudioSource; }
AudioSourceHandle AudioSystem::playSoundAt(AudioBufferHandle, float, float, float, float) { return InvalidAudioSource; }

void AudioSystem::update(float) {}

AudioStats AudioSystem::getStats() const { return {}; }

void AudioSystem::pauseAll() {}
void AudioSystem::resumeAll() {}
void AudioSystem::stopAll() {}

void AudioSystem::updateSourceGain(AudioSourceHandle) {}
void AudioSystem::cleanupFinishedOneShotSources() {}

bool AudioSystem::parseWAV(const uint8_t*, size_t, std::vector<uint8_t>&, AudioFormat&, int&) {
    return false;
}

#endif // HAVE_OPENAL

} // namespace opensaints
