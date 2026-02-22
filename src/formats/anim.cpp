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
    AnimHeader header;
    std::memcpy(&header, data, sizeof(AnimHeader));

    uint32_t numBones = header.num_bones;
    uint32_t numFrames = header.num_frames;
    float duration = header.duration > 0 ? header.duration : 1.0f;
    float fps = header.fps > 0 ? header.fps : 30.0f;

    if (numBones == 0 || numFrames == 0) {
        return true; // Nothing to parse, fallback identity track will be created
    }

    // Sanity limits
    if (numBones > 256 || numFrames > 100000) {
        std::cerr << "Animation data has implausible bone/frame counts: "
                  << numBones << " bones, " << numFrames << " frames\n";
        return true; // Let the fallback handle it
    }

    // Try to read bone name table
    // Bone names are expected as null-terminated strings starting right after AnimHeader
    size_t nameTableOffset = sizeof(AnimHeader);
    std::vector<std::string> boneNames;

    if (nameTableOffset < offset) {
        size_t pos = nameTableOffset;
        while (pos < offset && boneNames.size() < numBones) {
            if (pos >= size) break;
            if (data[pos] == 0) {
                pos++;
                continue;
            }
            const char* namePtr = reinterpret_cast<const char*>(data + pos);
            size_t maxLen = std::min(size - pos, size_t(64));
            size_t nameLen = strnlen(namePtr, maxLen);
            if (nameLen > 0 && nameLen < 64) {
                boneNames.emplace_back(namePtr, nameLen);
                pos += nameLen + 1;
            } else {
                break;
            }
        }
    }

    // Pad bone names if we didn't find enough
    while (boneNames.size() < numBones) {
        boneNames.push_back("bone_" + std::to_string(boneNames.size()));
    }

    // Parse keyframe data
    // SR2 compressed keyframe format (best-effort):
    // Per frame, per bone: 3 x int16 position + 4 x int16 rotation = 14 bytes per bone per frame
    // Some formats use 6 bytes (rotation only, 3 x int16 compressed quat)
    // Try the full format first, then the compact format

    size_t fullFrameSize = numBones * 14; // 3*int16 pos + 4*int16 rot
    size_t compactFrameSize = numBones * 6; // 3*int16 compressed quat (reconstruct w)
    size_t dataAvailable = (offset + header.data_size <= size) ? header.data_size : (size - offset);

    // Determine which format fits
    enum class KeyframeFormat { Full14, CompactRot6, Unknown };
    KeyframeFormat fmt = KeyframeFormat::Unknown;

    if (numFrames * fullFrameSize <= dataAvailable) {
        fmt = KeyframeFormat::Full14;
    } else if (numFrames * compactFrameSize <= dataAvailable) {
        fmt = KeyframeFormat::CompactRot6;
    }

    if (fmt == KeyframeFormat::Unknown) {
        // Data doesn't match expected sizes, let fallback handle it
        return true;
    }

    // Create bone tracks
    m_clip.tracks.resize(numBones);
    for (uint32_t b = 0; b < numBones; b++) {
        m_clip.tracks[b].boneName = boneNames[b];
        m_clip.tracks[b].boneIndex = static_cast<int32_t>(b);
        m_clip.tracks[b].keyframes.resize(numFrames);
    }

    float timePerFrame = (numFrames > 1) ? (duration / (numFrames - 1)) : 0.0f;

    // Position scale factor for int16 -> world units
    // Typically positions are scaled to fit in [-32768, 32767] representing
    // a range around the origin. A common scale is 1/100 or 1/1024.
    constexpr float POS_SCALE = 1.0f / 1024.0f;
    constexpr float ROT_SCALE = 1.0f / 32767.0f;

    for (uint32_t f = 0; f < numFrames; f++) {
        for (uint32_t b = 0; b < numBones; b++) {
            BoneKeyframe& kf = m_clip.tracks[b].keyframes[f];
            kf.time = f * timePerFrame;
            kf.scale = Vec3Anim(1, 1, 1);

            if (fmt == KeyframeFormat::Full14) {
                size_t boneOffset = offset + f * fullFrameSize + b * 14;
                if (boneOffset + 14 > size) {
                    kf.position = Vec3Anim(0, 0, 0);
                    kf.rotation = Quat::identity();
                    continue;
                }

                // Position: 3 x int16
                int16_t px, py, pz;
                std::memcpy(&px, data + boneOffset + 0, 2);
                std::memcpy(&py, data + boneOffset + 2, 2);
                std::memcpy(&pz, data + boneOffset + 4, 2);
                kf.position = Vec3Anim(px * POS_SCALE, py * POS_SCALE, pz * POS_SCALE);

                // Rotation: 4 x int16 quaternion
                int16_t rx, ry, rz, rw;
                std::memcpy(&rx, data + boneOffset + 6, 2);
                std::memcpy(&ry, data + boneOffset + 8, 2);
                std::memcpy(&rz, data + boneOffset + 10, 2);
                std::memcpy(&rw, data + boneOffset + 12, 2);
                kf.rotation = Quat(rx * ROT_SCALE, ry * ROT_SCALE,
                                   rz * ROT_SCALE, rw * ROT_SCALE).normalized();

            } else if (fmt == KeyframeFormat::CompactRot6) {
                size_t boneOffset = offset + f * compactFrameSize + b * 6;
                if (boneOffset + 6 > size) {
                    kf.position = Vec3Anim(0, 0, 0);
                    kf.rotation = Quat::identity();
                    continue;
                }

                kf.position = Vec3Anim(0, 0, 0);

                // Rotation: 3 x int16 (x, y, z), reconstruct w
                int16_t rx, ry, rz;
                std::memcpy(&rx, data + boneOffset + 0, 2);
                std::memcpy(&ry, data + boneOffset + 2, 2);
                std::memcpy(&rz, data + boneOffset + 4, 2);

                float qx = rx * ROT_SCALE;
                float qy = ry * ROT_SCALE;
                float qz = rz * ROT_SCALE;
                float wSq = 1.0f - (qx * qx + qy * qy + qz * qz);
                float qw = wSq > 0.0f ? std::sqrt(wSq) : 0.0f;

                kf.rotation = Quat(qx, qy, qz, qw).normalized();
            }
        }
    }

    return true;
}

} // namespace opensaints
