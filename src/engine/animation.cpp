#include "animation.h"
#include "../formats/anim.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace opensaints {

// Skeleton implementation

int32_t Skeleton::findBone(const std::string& name) const {
    for (size_t i = 0; i < bones.size(); ++i) {
        if (bones[i].name == name) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

void Skeleton::computeWorldTransforms(const std::vector<float>& localMatrices,
                                      std::vector<float>& worldMatrices) const {
    worldMatrices.resize(bones.size() * 16);

    for (size_t i = 0; i < bones.size(); ++i) {
        const Bone& bone = bones[i];
        float* world = &worldMatrices[i * 16];
        const float* local = &localMatrices[i * 16];

        if (bone.parentIndex < 0) {
            // Root bone - world = local
            std::memcpy(world, local, 16 * sizeof(float));
        } else {
            // Child bone - world = parent_world * local
            const float* parentWorld = &worldMatrices[bone.parentIndex * 16];

            // Matrix multiplication
            for (int row = 0; row < 4; ++row) {
                for (int col = 0; col < 4; ++col) {
                    world[row * 4 + col] =
                        parentWorld[row * 4 + 0] * local[0 * 4 + col] +
                        parentWorld[row * 4 + 1] * local[1 * 4 + col] +
                        parentWorld[row * 4 + 2] * local[2 * 4 + col] +
                        parentWorld[row * 4 + 3] * local[3 * 4 + col];
                }
            }
        }
    }
}

void Skeleton::computeSkinningMatrices(const std::vector<float>& worldMatrices,
                                       std::vector<float>& skinningMatrices) const {
    skinningMatrices.resize(bones.size() * 16);

    for (size_t i = 0; i < bones.size(); ++i) {
        const Bone& bone = bones[i];
        float* skinning = &skinningMatrices[i * 16];
        const float* world = &worldMatrices[i * 16];

        // skinning = world * invBindPose
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                skinning[row * 4 + col] =
                    world[row * 4 + 0] * bone.invBindPose[0 * 4 + col] +
                    world[row * 4 + 1] * bone.invBindPose[1 * 4 + col] +
                    world[row * 4 + 2] * bone.invBindPose[2 * 4 + col] +
                    world[row * 4 + 3] * bone.invBindPose[3 * 4 + col];
            }
        }
    }
}

// AnimationState implementation

void AnimationState::update(float deltaTime) {
    if (!playing || !clip) return;

    time += deltaTime * speed;

    if (time >= clip->duration) {
        if (looping) {
            time = std::fmod(time, clip->duration);
        } else {
            time = clip->duration;
            finished = true;
            playing = false;
        }
    }

    if (time < 0) {
        if (looping) {
            time = clip->duration + std::fmod(time, clip->duration);
        } else {
            time = 0;
        }
    }
}

void AnimationState::reset() {
    time = 0;
    finished = false;
}

// AnimationController implementation

AnimationController::AnimationController() {
    // Create default layer
    AnimationLayer layer;
    layer.name = "Base";
    m_layers.push_back(layer);
}

void AnimationController::setSkeleton(std::shared_ptr<Skeleton> skeleton) {
    m_skeleton = skeleton;
    if (skeleton) {
        m_boneMatrices.resize(skeleton->bones.size() * 16);
        m_skinningMatrices.resize(skeleton->bones.size() * 16);

        // Initialize to identity
        for (size_t i = 0; i < skeleton->bones.size(); ++i) {
            float* mat = &m_boneMatrices[i * 16];
            std::memset(mat, 0, 16 * sizeof(float));
            mat[0] = mat[5] = mat[10] = mat[15] = 1.0f;
        }
    }
}

void AnimationController::addAnimation(const std::string& name, std::shared_ptr<AnimationClip> clip) {
    m_animations[name] = clip;
}

void AnimationController::removeAnimation(const std::string& name) {
    m_animations.erase(name);
}

std::shared_ptr<AnimationClip> AnimationController::getAnimation(const std::string& name) const {
    auto it = m_animations.find(name);
    return (it != m_animations.end()) ? it->second : nullptr;
}

void AnimationController::play(const std::string& animationName, int layer, float transitionTime) {
    auto clip = getAnimation(animationName);
    if (!clip) {
        std::cerr << "Animation not found: " << animationName << "\n";
        return;
    }

    // Ensure layer exists
    while (static_cast<int>(m_layers.size()) <= layer) {
        AnimationLayer newLayer;
        newLayer.name = "Layer_" + std::to_string(m_layers.size());
        m_layers.push_back(newLayer);
    }

    AnimationLayer& l = m_layers[layer];

    if (transitionTime > 0 && l.state.playing) {
        // Start transition
        l.previousState = l.state;
        l.transitioning = true;
        l.transitionTime = 0;
        l.transitionDuration = transitionTime;
    }

    l.state.clip = clip;
    l.state.time = 0;
    l.state.playing = true;
    l.state.finished = false;
    l.state.looping = clip->looping;
}

void AnimationController::stop(int layer) {
    if (layer < static_cast<int>(m_layers.size())) {
        m_layers[layer].state.playing = false;
        m_layers[layer].state.time = 0;
        m_layers[layer].transitioning = false;
    }
}

void AnimationController::pause(int layer) {
    if (layer < static_cast<int>(m_layers.size())) {
        m_layers[layer].state.playing = false;
    }
}

void AnimationController::resume(int layer) {
    if (layer < static_cast<int>(m_layers.size())) {
        m_layers[layer].state.playing = true;
    }
}

AnimationLayer* AnimationController::getLayer(int index) {
    if (index < static_cast<int>(m_layers.size())) {
        return &m_layers[index];
    }
    return nullptr;
}

int AnimationController::addLayer(const std::string& name) {
    AnimationLayer layer;
    layer.name = name;
    m_layers.push_back(layer);
    return static_cast<int>(m_layers.size()) - 1;
}

void AnimationController::setLayerWeight(int layer, float weight) {
    if (layer < static_cast<int>(m_layers.size())) {
        m_layers[layer].blendWeight = std::clamp(weight, 0.0f, 1.0f);
    }
}

void AnimationController::setLayerBlendMode(int layer, BlendMode mode) {
    if (layer < static_cast<int>(m_layers.size())) {
        m_layers[layer].blendMode = mode;
    }
}

void AnimationController::update(float deltaTime) {
    deltaTime *= m_globalSpeed;

    // Update all layers
    for (auto& layer : m_layers) {
        // Update transition
        if (layer.transitioning) {
            layer.transitionTime += deltaTime;
            if (layer.transitionTime >= layer.transitionDuration) {
                layer.transitioning = false;
            }
            layer.previousState.update(deltaTime);
        }

        // Update current state
        layer.state.update(deltaTime);
    }

    // Evaluate final pose
    evaluatePose();
}

void AnimationController::evaluatePose() {
    if (!m_skeleton || m_skeleton->bones.empty()) return;

    size_t boneCount = m_skeleton->bones.size();
    std::vector<float> localPose(boneCount * 16);

    // Initialize to bind pose
    for (size_t i = 0; i < boneCount; ++i) {
        std::memcpy(&localPose[i * 16], m_skeleton->bones[i].localBindPose, 16 * sizeof(float));
    }

    // Apply each layer
    for (auto& layer : m_layers) {
        if (layer.blendWeight <= 0) continue;
        if (!layer.state.clip) continue;

        std::vector<float> layerPose(boneCount * 16);
        clipToMatrices(*layer.state.clip, layer.state.time, layerPose);

        // Handle transition blending
        if (layer.transitioning && layer.previousState.clip) {
            std::vector<float> prevPose(boneCount * 16);
            clipToMatrices(*layer.previousState.clip, layer.previousState.time, prevPose);

            float t = layer.transitionTime / layer.transitionDuration;
            blendPoses(prevPose, layerPose, t, layerPose);
        }

        // Apply layer to final pose
        float weight = layer.blendWeight * layer.state.weight;
        switch (layer.blendMode) {
            case BlendMode::Override:
                blendPoses(localPose, layerPose, weight, localPose);
                break;
            case BlendMode::Additive:
                // Additive blending (add difference from bind pose)
                for (size_t i = 0; i < boneCount * 16; ++i) {
                    float bindValue = m_skeleton->bones[i / 16].localBindPose[i % 16];
                    float diff = layerPose[i] - bindValue;
                    localPose[i] += diff * weight;
                }
                break;
            case BlendMode::Blend:
                blendPoses(localPose, layerPose, weight, localPose);
                break;
        }
    }

    // Compute world transforms
    m_skeleton->computeWorldTransforms(localPose, m_boneMatrices);

    // Compute skinning matrices
    m_skeleton->computeSkinningMatrices(m_boneMatrices, m_skinningMatrices);
}

void AnimationController::blendPoses(const std::vector<float>& poseA, const std::vector<float>& poseB,
                                     float weight, std::vector<float>& result) {
    // Simple linear interpolation of matrices
    // (Real implementation should decompose to TRS and blend properly)
    for (size_t i = 0; i < poseA.size(); ++i) {
        result[i] = poseA[i] * (1.0f - weight) + poseB[i] * weight;
    }
}

void AnimationController::clipToMatrices(const AnimationClip& clip, float time, std::vector<float>& matrices) {
    if (!m_skeleton) return;

    size_t boneCount = m_skeleton->bones.size();
    matrices.resize(boneCount * 16);

    // Initialize to identity
    for (size_t i = 0; i < boneCount; ++i) {
        float* mat = &matrices[i * 16];
        std::memset(mat, 0, 16 * sizeof(float));
        mat[0] = mat[5] = mat[10] = mat[15] = 1.0f;
    }

    // Sample each track
    for (const auto& track : clip.tracks) {
        int32_t boneIndex = track.boneIndex;
        if (boneIndex < 0) {
            boneIndex = m_skeleton->findBone(track.boneName);
        }
        if (boneIndex < 0 || boneIndex >= static_cast<int32_t>(boneCount)) continue;

        Vec3Anim position;
        Quat rotation;
        Vec3Anim scale;
        track.sample(time, position, rotation, scale);

        // Build matrix from TRS
        float* mat = &matrices[boneIndex * 16];

        // Rotation (quaternion to matrix)
        float x = rotation.x, y = rotation.y, z = rotation.z, w = rotation.w;
        float x2 = x + x, y2 = y + y, z2 = z + z;
        float xx = x * x2, xy = x * y2, xz = x * z2;
        float yy = y * y2, yz = y * z2, zz = z * z2;
        float wx = w * x2, wy = w * y2, wz = w * z2;

        mat[0] = (1.0f - (yy + zz)) * scale.x;
        mat[1] = (xy + wz) * scale.x;
        mat[2] = (xz - wy) * scale.x;
        mat[3] = 0.0f;

        mat[4] = (xy - wz) * scale.y;
        mat[5] = (1.0f - (xx + zz)) * scale.y;
        mat[6] = (yz + wx) * scale.y;
        mat[7] = 0.0f;

        mat[8] = (xz + wy) * scale.z;
        mat[9] = (yz - wx) * scale.z;
        mat[10] = (1.0f - (xx + yy)) * scale.z;
        mat[11] = 0.0f;

        mat[12] = position.x;
        mat[13] = position.y;
        mat[14] = position.z;
        mat[15] = 1.0f;
    }
}

bool AnimationController::isPlaying(int layer) const {
    if (layer < static_cast<int>(m_layers.size())) {
        return m_layers[layer].state.playing;
    }
    return false;
}

float AnimationController::getCurrentTime(int layer) const {
    if (layer < static_cast<int>(m_layers.size())) {
        return m_layers[layer].state.time;
    }
    return 0;
}

float AnimationController::getNormalizedTime(int layer) const {
    if (layer < static_cast<int>(m_layers.size())) {
        const auto& state = m_layers[layer].state;
        if (state.clip && state.clip->duration > 0) {
            return state.time / state.clip->duration;
        }
    }
    return 0;
}

std::string AnimationController::getCurrentAnimation(int layer) const {
    if (layer < static_cast<int>(m_layers.size())) {
        const auto& state = m_layers[layer].state;
        if (state.clip) {
            return state.clip->name;
        }
    }
    return "";
}

// AnimationStateMachine implementation

AnimationStateMachine::AnimationStateMachine() = default;

void AnimationStateMachine::addState(const State& state) {
    m_states[state.name] = state;
}

void AnimationStateMachine::addTransition(const Transition& transition) {
    m_transitions.push_back(transition);
}

void AnimationStateMachine::setInitialState(const std::string& stateName) {
    m_currentState = stateName;
    if (m_controller) {
        auto it = m_states.find(stateName);
        if (it != m_states.end()) {
            m_controller->play(it->second.animation, 0, 0);
            m_controller->getLayer(0)->state.speed = it->second.speed;
            m_controller->getLayer(0)->state.looping = it->second.loop;
        }
    }
}

void AnimationStateMachine::setController(AnimationController* controller) {
    m_controller = controller;
}

void AnimationStateMachine::trigger(const std::string& triggerName) {
    m_pendingTriggers.insert(triggerName);
}

void AnimationStateMachine::setParameter(const std::string& name, float value) {
    m_floatParams[name] = value;
}

void AnimationStateMachine::setParameter(const std::string& name, bool value) {
    m_boolParams[name] = value;
}

void AnimationStateMachine::update(float deltaTime) {
    if (!m_controller) return;

    if (m_inTransition) {
        m_transitionProgress += deltaTime;
        // Transition is handled by AnimationController
    } else {
        checkTransitions();
    }

    // Clear triggers
    m_pendingTriggers.clear();
}

void AnimationStateMachine::checkTransitions() {
    for (const auto& transition : m_transitions) {
        if (transition.fromState != m_currentState) continue;

        bool shouldTransition = false;

        // Check trigger
        if (!transition.trigger.empty()) {
            if (m_pendingTriggers.find(transition.trigger) != m_pendingTriggers.end()) {
                shouldTransition = true;
            }
        }

        // Check condition
        if (transition.condition) {
            shouldTransition = transition.condition();
        }

        // Auto transition when animation finishes
        if (transition.trigger.empty() && !transition.condition) {
            auto* layer = m_controller->getLayer(0);
            if (layer && layer->state.finished) {
                shouldTransition = true;
            }
        }

        if (shouldTransition) {
            performTransition(transition);
            break;
        }
    }
}

void AnimationStateMachine::performTransition(const Transition& transition) {
    auto it = m_states.find(transition.toState);
    if (it == m_states.end()) return;

    const State& newState = it->second;

    m_controller->play(newState.animation, 0, transition.duration);
    m_controller->getLayer(0)->state.speed = newState.speed;
    m_controller->getLayer(0)->state.looping = newState.loop;

    m_currentState = transition.toState;
    m_inTransition = transition.duration > 0;
    m_transitionProgress = 0;
}

} // namespace opensaints
