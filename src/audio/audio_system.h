#pragma once
// Audio System for OpenSaints
// Provides 3D spatial audio using OpenAL

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <queue>

namespace opensaints {

// Audio handle types
using AudioBufferHandle = uint32_t;
using AudioSourceHandle = uint32_t;

constexpr AudioBufferHandle InvalidAudioBuffer = 0;
constexpr AudioSourceHandle InvalidAudioSource = 0;

// Audio format
enum class AudioFormat {
    Mono8,
    Mono16,
    Stereo8,
    Stereo16
};

// Audio source state
enum class AudioSourceState {
    Initial,
    Playing,
    Paused,
    Stopped
};

// Listener (camera) position/orientation
struct AudioListener {
    float position[3] = {0, 0, 0};
    float velocity[3] = {0, 0, 0};
    float forward[3] = {0, 0, -1};
    float up[3] = {0, 1, 0};
};

// Audio source properties
struct AudioSourceParams {
    float position[3] = {0, 0, 0};
    float velocity[3] = {0, 0, 0};
    float direction[3] = {0, 0, 0};

    float gain = 1.0f;           // Volume (0.0 to 1.0+)
    float pitch = 1.0f;          // Playback speed
    float minDistance = 1.0f;    // Distance for full volume
    float maxDistance = 100.0f;  // Distance for zero volume
    float rolloffFactor = 1.0f;  // Attenuation rate
    float coneInnerAngle = 360.0f;  // Full volume cone
    float coneOuterAngle = 360.0f;  // Transition cone
    float coneOuterGain = 0.0f;     // Volume outside cone

    bool looping = false;
    bool relative = false;  // Position relative to listener
};

// Audio statistics
struct AudioStats {
    uint32_t buffersLoaded;
    uint32_t sourcesActive;
    uint32_t sourcesPlaying;
    size_t memoryUsage;
};

// Audio system configuration
struct AudioConfig {
    int maxSources = 64;
    float masterVolume = 1.0f;
    float musicVolume = 1.0f;
    float sfxVolume = 1.0f;
    float voiceVolume = 1.0f;
    float dopplerFactor = 1.0f;
    float speedOfSound = 343.3f;  // meters per second
};

// Audio channel for mixing
enum class AudioChannel {
    Master,
    Music,
    SFX,
    Voice,
    Ambient
};

// Audio System
class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    // Initialize/shutdown
    bool initialize(const AudioConfig& config = AudioConfig{});
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // Buffer management (load audio data)
    AudioBufferHandle createBuffer(const void* data, size_t size, AudioFormat format, int sampleRate);
    AudioBufferHandle loadWAV(const std::string& path);
    AudioBufferHandle loadFromMemory(const uint8_t* data, size_t size);
    void destroyBuffer(AudioBufferHandle buffer);

    // Source management (playback instance)
    AudioSourceHandle createSource();
    void destroySource(AudioSourceHandle source);

    // Attach buffer to source
    void setSourceBuffer(AudioSourceHandle source, AudioBufferHandle buffer);

    // Playback control
    void play(AudioSourceHandle source);
    void pause(AudioSourceHandle source);
    void stop(AudioSourceHandle source);
    void rewind(AudioSourceHandle source);

    // Source properties
    void setSourceParams(AudioSourceHandle source, const AudioSourceParams& params);
    void setSourcePosition(AudioSourceHandle source, float x, float y, float z);
    void setSourceVelocity(AudioSourceHandle source, float x, float y, float z);
    void setSourceGain(AudioSourceHandle source, float gain);
    void setSourcePitch(AudioSourceHandle source, float pitch);
    void setSourceLooping(AudioSourceHandle source, bool looping);
    void setSourceChannel(AudioSourceHandle source, AudioChannel channel);

    // Query source state
    AudioSourceState getSourceState(AudioSourceHandle source) const;
    float getSourcePlaybackPosition(AudioSourceHandle source) const;
    bool isSourcePlaying(AudioSourceHandle source) const;

    // Listener (typically camera)
    void setListener(const AudioListener& listener);
    void setListenerPosition(float x, float y, float z);
    void setListenerVelocity(float x, float y, float z);
    void setListenerOrientation(float forwardX, float forwardY, float forwardZ,
                                float upX, float upY, float upZ);

    // Channel volumes
    void setMasterVolume(float volume);
    void setChannelVolume(AudioChannel channel, float volume);
    float getMasterVolume() const { return m_config.masterVolume; }
    float getChannelVolume(AudioChannel channel) const;

    // Convenience: Play one-shot sound
    AudioSourceHandle playOneShot(AudioBufferHandle buffer, float volume = 1.0f);
    AudioSourceHandle playSoundAt(AudioBufferHandle buffer, float x, float y, float z, float volume = 1.0f);

    // Update (call each frame for source management)
    void update(float deltaTime);

    // Statistics
    AudioStats getStats() const;

    // Pause/resume all
    void pauseAll();
    void resumeAll();
    void stopAll();

private:
    bool m_initialized = false;
    AudioConfig m_config;

    // OpenAL handles
    void* m_device = nullptr;    // ALCdevice*
    void* m_context = nullptr;   // ALCcontext*

    // Managed buffers
    struct ManagedBuffer {
        uint32_t alBuffer = 0;
        size_t size = 0;
        AudioFormat format;
        int sampleRate = 0;
    };
    std::unordered_map<AudioBufferHandle, ManagedBuffer> m_buffers;
    AudioBufferHandle m_nextBufferHandle = 1;

    // Managed sources
    struct ManagedSource {
        uint32_t alSource = 0;
        AudioBufferHandle buffer = InvalidAudioBuffer;
        AudioChannel channel = AudioChannel::SFX;
        float baseGain = 1.0f;
        bool oneShot = false;
    };
    std::unordered_map<AudioSourceHandle, ManagedSource> m_sources;
    AudioSourceHandle m_nextSourceHandle = 1;

    // Source pool for one-shots
    std::queue<AudioSourceHandle> m_availableSources;
    std::vector<AudioSourceHandle> m_oneShotSources;

    // Channel volumes
    std::unordered_map<AudioChannel, float> m_channelVolumes;

    // Listener
    AudioListener m_listener;

    // Internal helpers
    void updateSourceGain(AudioSourceHandle source);
    void cleanupFinishedOneShotSources();

    // WAV file parsing
    bool parseWAV(const uint8_t* data, size_t size, std::vector<uint8_t>& pcmData,
                  AudioFormat& format, int& sampleRate);
};

// Global audio system access
AudioSystem& getAudioSystem();

} // namespace opensaints
