#include "anim.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace opensaints {

// Quat implementation

Quat Quat::slerp(const Quat& a, const Quat& b, float t) {
    // Compute dot product
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

    // If dot is negative, negate one quaternion to take shorter path
    Quat b2 = b;
    if (dot < 0.0f) {
        b2.x = -b.x;
        b2.y = -b.y;
        b2.z = -b.z;
        b2.w = -b.w;
        dot = -dot;
    }

    // If quaternions are very close, use linear interpolation
    if (dot > 0.9995f) {
        return Quat(
            a.x + t * (b2.x - a.x),
            a.y + t * (b2.y - a.y),
            a.z + t * (b2.z - a.z),
            a.w + t * (b2.w - a.w)
        ).normalized();
    }

    // Compute slerp
    float theta0 = std::acos(dot);
    float theta = theta0 * t;
    float sinTheta = std::sin(theta);
    float sinTheta0 = std::sin(theta0);

    float s0 = std::cos(theta) - dot * sinTheta / sinTheta0;
    float s1 = sinTheta / sinTheta0;

    return Quat(
        s0 * a.x + s1 * b2.x,
        s0 * a.y + s1 * b2.y,
        s0 * a.z + s1 * b2.z,
        s0 * a.w + s1 * b2.w
    );
}

Quat Quat::normalized() const {
    float len = std::sqrt(x * x + y * y + z * z + w * w);
    if (len < 0.0001f) return Quat::identity();
    return Quat(x / len, y / len, z / len, w / len);
}

Quat Quat::operator*(const Quat& other) const {
    return Quat(
        w * other.x + x * other.w + y * other.z - z * other.y,
        w * other.y - x * other.z + y * other.w + z * other.x,
        w * other.z + x * other.y - y * other.x + z * other.w,
        w * other.w - x * other.x - y * other.y - z * other.z
    );
}

// Vec3Anim implementation

Vec3Anim Vec3Anim::lerp(const Vec3Anim& a, const Vec3Anim& b, float t) {
    return Vec3Anim(
        a.x + t * (b.x - a.x),
        a.y + t * (b.y - a.y),
        a.z + t * (b.z - a.z)
    );
}

Vec3Anim Vec3Anim::operator+(const Vec3Anim& other) const {
    return Vec3Anim(x + other.x, y + other.y, z + other.z);
}

Vec3Anim Vec3Anim::operator*(float scalar) const {
    return Vec3Anim(x * scalar, y * scalar, z * scalar);
}

// BoneTrack implementation

void BoneTrack::sample(float time, Vec3Anim& position, Quat& rotation, Vec3Anim& scale) const {
    if (keyframes.empty()) {
        position = Vec3Anim(0, 0, 0);
        rotation = Quat::identity();
        scale = Vec3Anim(1, 1, 1);
        return;
    }

    if (keyframes.size() == 1) {
        position = keyframes[0].position;
        rotation = keyframes[0].rotation;
        scale = keyframes[0].scale;
        return;
    }

    // Find the two keyframes to interpolate between
    size_t idx0 = 0;
    size_t idx1 = 1;

    for (size_t i = 0; i < keyframes.size() - 1; ++i) {
        if (time >= keyframes[i].time && time < keyframes[i + 1].time) {
            idx0 = i;
            idx1 = i + 1;
            break;
        }
    }

    // Handle time before first keyframe
    if (time < keyframes[0].time) {
        position = keyframes[0].position;
        rotation = keyframes[0].rotation;
        scale = keyframes[0].scale;
        return;
    }

    // Handle time after last keyframe
    if (time >= keyframes.back().time) {
        position = keyframes.back().position;
        rotation = keyframes.back().rotation;
        scale = keyframes.back().scale;
        return;
    }

    // Calculate interpolation factor
    const BoneKeyframe& kf0 = keyframes[idx0];
    const BoneKeyframe& kf1 = keyframes[idx1];
    float t = (time - kf0.time) / (kf1.time - kf0.time);

    // Interpolate
    position = Vec3Anim::lerp(kf0.position, kf1.position, t);
    rotation = Quat::slerp(kf0.rotation, kf1.rotation, t);
    scale = Vec3Anim::lerp(kf0.scale, kf1.scale, t);
}

// AnimationClip implementation

void AnimationClip::sample(float time, std::vector<Vec3Anim>& positions,
                           std::vector<Quat>& rotations, std::vector<Vec3Anim>& scales) const {
    positions.resize(tracks.size());
    rotations.resize(tracks.size());
    scales.resize(tracks.size());

    for (size_t i = 0; i < tracks.size(); ++i) {
        tracks[i].sample(time, positions[i], rotations[i], scales[i]);
    }
}

const BoneTrack* AnimationClip::findTrack(const std::string& boneName) const {
    for (const auto& track : tracks) {
        if (track.boneName == boneName) {
            return &track;
        }
    }
    return nullptr;
}

const BoneTrack* AnimationClip::findTrack(int32_t boneIndex) const {
    for (const auto& track : tracks) {
        if (track.boneIndex == boneIndex) {
            return &track;
        }
    }
    return nullptr;
}

// AnimationFile implementation

bool AnimationFile::loadFromFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open animation file: " << path << "\n";
        return false;
    }

    m_path = path;
    m_clip.name = path.stem().string();

    // Read file into memory
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);

    return loadFromMemory(data.data(), size);
}

bool AnimationFile::loadFromMemory(const uint8_t* data, size_t size) {
    if (size < sizeof(AnimHeader)) {
        std::cerr << "Animation file too small\n";
        return false;
    }

    if (!parseHeader(data, size)) {
        return false;
    }

    m_loaded = true;
    return true;
}

bool AnimationFile::parseHeader(const uint8_t* data, size_t size) {
    AnimHeader header;
    std::memcpy(&header, data, sizeof(AnimHeader));

    // Store basic info
    m_clip.duration = header.duration;
    m_clip.fps = header.fps > 0 ? header.fps : 30.0f;
    m_clip.looping = false; // Determine from flags

    // Parse keyframes
    if (header.data_offset + header.data_size <= size) {
        if (!parseKeyframes(data, size, header.data_offset)) {
            return false;
        }
    }

    // If no tracks were parsed, create placeholder
    if (m_clip.tracks.empty()) {
        // Create a single bone track with identity transforms
        BoneTrack track;
        track.boneName = "root";
        track.boneIndex = 0;

        // Create keyframes at start and end
        BoneKeyframe kf0, kf1;
        kf0.time = 0;
        kf0.position = Vec3Anim(0, 0, 0);
        kf0.rotation = Quat::identity();
        kf0.scale = Vec3Anim(1, 1, 1);

        kf1.time = m_clip.duration > 0 ? m_clip.duration : 1.0f;
        kf1.position = Vec3Anim(0, 0, 0);
        kf1.rotation = Quat::identity();
        kf1.scale = Vec3Anim(1, 1, 1);

        track.keyframes.push_back(kf0);
        track.keyframes.push_back(kf1);

        m_clip.tracks.push_back(track);
    }

    std::cout << "Loaded animation: " << m_clip.name
              << " (" << m_clip.tracks.size() << " tracks, "
              << m_clip.duration << "s)\n";

    return true;
}

bool AnimationFile::parseKeyframes(const uint8_t* data, size_t size, size_t offset) {
    // Placeholder implementation
    // Real implementation needs format reverse-engineering

    // The actual format would contain:
    // - Per-bone keyframe data
    // - Compressed position/rotation/scale values
    // - Time stamps

    return true;
}

} // namespace opensaints
