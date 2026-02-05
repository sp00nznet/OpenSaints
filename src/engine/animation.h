#pragma once
// Animation playback system for OpenSaints
// Handles skeletal animation, blending, and state machines

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace opensaints {

// Forward declarations
struct AnimationClip;
struct Vec3Anim;
struct Quat;

// Bone in a skeleton
struct Bone {
    std::string name;
    int32_t parentIndex;     // -1 if root
    float localBindPose[16]; // Local bind pose matrix
    float invBindPose[16];   // Inverse bind pose (for skinning)
};

// Skeleton definition
struct Skeleton {
    std::string name;
    std::vector<Bone> bones;

    // Find bone by name
    int32_t findBone(const std::string& name) const;

    // Compute world transforms from local transforms
    void computeWorldTransforms(const std::vector<float>& localMatrices,
                                std::vector<float>& worldMatrices) const;

    // Compute skinning matrices
    void computeSkinningMatrices(const std::vector<float>& worldMatrices,
                                 std::vector<float>& skinningMatrices) const;
};

// Animation playback state
struct AnimationState {
    std::shared_ptr<AnimationClip> clip;
    float time = 0;
    float speed = 1.0f;
    float weight = 1.0f;
    bool playing = false;
    bool looping = true;
    bool finished = false;

    void update(float deltaTime);
    void reset();
};

// Blend mode for multiple animations
enum class BlendMode {
    Override,  // Replace previous
    Additive,  // Add to previous
    Blend      // Weighted blend
};

// Animation layer for blending
struct AnimationLayer {
    std::string name;
    AnimationState state;
    BlendMode blendMode = BlendMode::Override;
    float blendWeight = 1.0f;
    uint32_t boneMask = 0xFFFFFFFF; // Which bones this layer affects

    // Transition
    bool transitioning = false;
    float transitionTime = 0;
    float transitionDuration = 0.3f;
    AnimationState previousState;
};

// Animation controller for an entity
class AnimationController {
public:
    AnimationController();
    ~AnimationController() = default;

    // Set skeleton
    void setSkeleton(std::shared_ptr<Skeleton> skeleton);
    std::shared_ptr<Skeleton> getSkeleton() const { return m_skeleton; }

    // Animation management
    void addAnimation(const std::string& name, std::shared_ptr<AnimationClip> clip);
    void removeAnimation(const std::string& name);
    std::shared_ptr<AnimationClip> getAnimation(const std::string& name) const;

    // Playback control
    void play(const std::string& animationName, int layer = 0, float transitionTime = 0.2f);
    void stop(int layer = 0);
    void pause(int layer = 0);
    void resume(int layer = 0);

    // Layer management
    AnimationLayer* getLayer(int index);
    int addLayer(const std::string& name);
    void setLayerWeight(int layer, float weight);
    void setLayerBlendMode(int layer, BlendMode mode);

    // Update
    void update(float deltaTime);

    // Get final pose
    const std::vector<float>& getBoneMatrices() const { return m_boneMatrices; }
    const std::vector<float>& getSkinningMatrices() const { return m_skinningMatrices; }

    // Speed control
    void setSpeed(float speed) { m_globalSpeed = speed; }
    float getSpeed() const { return m_globalSpeed; }

    // Callbacks
    using AnimationEventCallback = std::function<void(const std::string& event)>;
    void setEventCallback(AnimationEventCallback callback) { m_eventCallback = callback; }

    // Query
    bool isPlaying(int layer = 0) const;
    float getCurrentTime(int layer = 0) const;
    float getNormalizedTime(int layer = 0) const;
    std::string getCurrentAnimation(int layer = 0) const;

private:
    std::shared_ptr<Skeleton> m_skeleton;
    std::unordered_map<std::string, std::shared_ptr<AnimationClip>> m_animations;
    std::vector<AnimationLayer> m_layers;
    float m_globalSpeed = 1.0f;

    // Computed pose
    std::vector<float> m_boneMatrices;      // World-space bone transforms
    std::vector<float> m_skinningMatrices;  // Final skinning matrices

    AnimationEventCallback m_eventCallback;

    void evaluatePose();
    void blendPoses(const std::vector<float>& poseA, const std::vector<float>& poseB,
                    float weight, std::vector<float>& result);
    void clipToMatrices(const AnimationClip& clip, float time, std::vector<float>& matrices);
};

// Animation state machine for complex animation logic
class AnimationStateMachine {
public:
    AnimationStateMachine();
    ~AnimationStateMachine() = default;

    // State definition
    struct State {
        std::string name;
        std::string animation;
        float speed = 1.0f;
        bool loop = true;
    };

    // Transition definition
    struct Transition {
        std::string fromState;
        std::string toState;
        std::string trigger;       // Trigger name (empty for auto)
        float duration = 0.2f;
        std::function<bool()> condition; // Optional condition
    };

    // Setup
    void addState(const State& state);
    void addTransition(const Transition& transition);
    void setInitialState(const std::string& stateName);

    // Control
    void setController(AnimationController* controller);
    void trigger(const std::string& triggerName);
    void setParameter(const std::string& name, float value);
    void setParameter(const std::string& name, bool value);

    // Update
    void update(float deltaTime);

    // Query
    const std::string& getCurrentState() const { return m_currentState; }
    bool isInTransition() const { return m_inTransition; }

private:
    AnimationController* m_controller = nullptr;
    std::unordered_map<std::string, State> m_states;
    std::vector<Transition> m_transitions;
    std::string m_currentState;
    std::string m_nextState;
    bool m_inTransition = false;
    float m_transitionProgress = 0;

    std::unordered_map<std::string, float> m_floatParams;
    std::unordered_map<std::string, bool> m_boolParams;
    std::unordered_set<std::string> m_pendingTriggers;

    void checkTransitions();
    void performTransition(const Transition& transition);
};

} // namespace opensaints
