#pragma once
// Animation file parser for Saints Row 2
// Handles .anim_pc files containing skeletal animation data

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace opensaints {

#pragma pack(push, 1)

// Animation file header (preliminary - needs verification)
struct AnimHeader {
    uint32_t signature;       // File signature
    uint32_t version;         // Format version
    uint32_t flags;           // Animation flags
    uint32_t num_bones;       // Number of bones
    uint32_t num_frames;      // Number of frames
    float duration;           // Total duration in seconds
    float fps;                // Frames per second
    uint32_t data_offset;     // Offset to keyframe data
    uint32_t data_size;       // Size of keyframe data
};

#pragma pack(pop)

// Quaternion for rotation
struct Quat {
    float x, y, z, w;

    Quat() : x(0), y(0), z(0), w(1) {}
    Quat(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    static Quat identity() { return Quat(0, 0, 0, 1); }
    static Quat slerp(const Quat& a, const Quat& b, float t);
    Quat normalized() const;
    Quat operator*(const Quat& other) const;
};

// Vector3 for position/scale
struct Vec3Anim {
    float x, y, z;

    Vec3Anim() : x(0), y(0), z(0) {}
    Vec3Anim(float x, float y, float z) : x(x), y(y), z(z) {}

    static Vec3Anim lerp(const Vec3Anim& a, const Vec3Anim& b, float t);
    Vec3Anim operator+(const Vec3Anim& other) const;
    Vec3Anim operator*(float scalar) const;
};

// Keyframe for a single bone at a specific time
struct BoneKeyframe {
    float time;           // Time in seconds
    Vec3Anim position;    // Local position
    Quat rotation;        // Local rotation (quaternion)
    Vec3Anim scale;       // Local scale
};

// Animation track for a single bone
struct BoneTrack {
    std::string boneName;
    int32_t boneIndex;
    std::vector<BoneKeyframe> keyframes;

    // Get interpolated transform at time
    void sample(float time, Vec3Anim& position, Quat& rotation, Vec3Anim& scale) const;
};

// Animation clip data
struct AnimationClip {
    std::string name;
    float duration;
    float fps;
    bool looping;
    std::vector<BoneTrack> tracks;

    // Sample all bones at time
    void sample(float time, std::vector<Vec3Anim>& positions,
                std::vector<Quat>& rotations, std::vector<Vec3Anim>& scales) const;

    // Get bone track by name
    const BoneTrack* findTrack(const std::string& boneName) const;
    const BoneTrack* findTrack(int32_t boneIndex) const;
};

// Animation file parser
class AnimationFile {
public:
    AnimationFile() = default;
    ~AnimationFile() = default;

    // Load from file
    bool loadFromFile(const std::filesystem::path& path);

    // Load from memory
    bool loadFromMemory(const uint8_t* data, size_t size);

    // Get animation clip
    const AnimationClip& clip() const { return m_clip; }

    // Check if loaded
    bool isLoaded() const { return m_loaded; }

    // Get source path
    const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path;
    AnimationClip m_clip;
    bool m_loaded = false;

    bool parseHeader(const uint8_t* data, size_t size);
    bool parseKeyframes(const uint8_t* data, size_t size, size_t offset);
};

} // namespace opensaints
