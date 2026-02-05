#pragma once
// Physics System for OpenSaints
// Provides collision detection, rigid body dynamics, and raycasting

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace opensaints {

// Forward declarations
class Entity;
class PhysicsWorld;

// Physics handle types
using ColliderHandle = uint32_t;
using RigidbodyHandle = uint32_t;

constexpr ColliderHandle InvalidCollider = 0;
constexpr RigidbodyHandle InvalidRigidbody = 0;

// 3D Vector for physics
struct Vec3Physics {
    float x = 0, y = 0, z = 0;

    Vec3Physics() = default;
    Vec3Physics(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3Physics operator+(const Vec3Physics& other) const {
        return {x + other.x, y + other.y, z + other.z};
    }
    Vec3Physics operator-(const Vec3Physics& other) const {
        return {x - other.x, y - other.y, z - other.z};
    }
    Vec3Physics operator*(float scalar) const {
        return {x * scalar, y * scalar, z * scalar};
    }
    Vec3Physics& operator+=(const Vec3Physics& other) {
        x += other.x; y += other.y; z += other.z;
        return *this;
    }

    float dot(const Vec3Physics& other) const {
        return x * other.x + y * other.y + z * other.z;
    }
    Vec3Physics cross(const Vec3Physics& other) const {
        return {
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        };
    }
    float length() const;
    float lengthSquared() const { return x*x + y*y + z*z; }
    Vec3Physics normalized() const;

    static Vec3Physics zero() { return {0, 0, 0}; }
    static Vec3Physics up() { return {0, 1, 0}; }
    static Vec3Physics forward() { return {0, 0, 1}; }
    static Vec3Physics right() { return {1, 0, 0}; }
};

// Quaternion for physics rotations
struct QuatPhysics {
    float x = 0, y = 0, z = 0, w = 1;

    QuatPhysics() = default;
    QuatPhysics(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    static QuatPhysics identity() { return {0, 0, 0, 1}; }
    static QuatPhysics fromAxisAngle(const Vec3Physics& axis, float angle);
    static QuatPhysics fromEuler(float pitch, float yaw, float roll);

    QuatPhysics operator*(const QuatPhysics& other) const;
    Vec3Physics rotate(const Vec3Physics& v) const;
    QuatPhysics normalized() const;
    QuatPhysics conjugate() const { return {-x, -y, -z, w}; }
};

// Transform for physics objects
struct PhysicsTransform {
    Vec3Physics position;
    QuatPhysics rotation;

    PhysicsTransform() = default;
    PhysicsTransform(const Vec3Physics& pos, const QuatPhysics& rot)
        : position(pos), rotation(rot) {}

    Vec3Physics transformPoint(const Vec3Physics& point) const;
    Vec3Physics transformVector(const Vec3Physics& vec) const;
    PhysicsTransform inverse() const;
};

// Collision shape types
enum class ColliderType {
    Box,
    Sphere,
    Capsule,
    Cylinder,
    ConvexHull,
    TriangleMesh,
    Heightfield,
    Compound
};

// Collision layers for filtering
enum class CollisionLayer : uint32_t {
    Default     = 1 << 0,
    Static      = 1 << 1,
    Dynamic     = 1 << 2,
    Character   = 1 << 3,
    Vehicle     = 1 << 4,
    Projectile  = 1 << 5,
    Trigger     = 1 << 6,
    Water       = 1 << 7,
    Debris      = 1 << 8,
    All         = 0xFFFFFFFF
};

inline CollisionLayer operator|(CollisionLayer a, CollisionLayer b) {
    return static_cast<CollisionLayer>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

// Collider description
struct ColliderDesc {
    ColliderType type = ColliderType::Box;
    Vec3Physics offset;             // Local offset from rigidbody
    QuatPhysics rotation;           // Local rotation

    // Shape parameters
    Vec3Physics boxHalfExtents = {0.5f, 0.5f, 0.5f};
    float sphereRadius = 0.5f;
    float capsuleRadius = 0.25f;
    float capsuleHeight = 1.0f;

    // Mesh data (for ConvexHull/TriangleMesh)
    std::vector<Vec3Physics> vertices;
    std::vector<uint32_t> indices;

    // Material properties
    float friction = 0.5f;
    float restitution = 0.3f;

    // Collision filtering
    CollisionLayer layer = CollisionLayer::Default;
    CollisionLayer mask = CollisionLayer::All;  // What to collide with

    bool isTrigger = false;  // If true, no physical response
};

// Rigidbody type
enum class RigidbodyType {
    Static,     // Never moves
    Kinematic,  // Moved by code, not physics
    Dynamic     // Simulated by physics
};

// Rigidbody description
struct RigidbodyDesc {
    RigidbodyType type = RigidbodyType::Dynamic;
    Vec3Physics position;
    QuatPhysics rotation;

    float mass = 1.0f;
    float linearDamping = 0.01f;
    float angularDamping = 0.05f;

    bool useGravity = true;
    bool freezeRotationX = false;
    bool freezeRotationY = false;
    bool freezeRotationZ = false;

    std::vector<ColliderDesc> colliders;
};

// Collision info
struct CollisionInfo {
    ColliderHandle colliderA;
    ColliderHandle colliderB;
    RigidbodyHandle bodyA;
    RigidbodyHandle bodyB;
    Entity* entityA = nullptr;
    Entity* entityB = nullptr;

    Vec3Physics contactPoint;
    Vec3Physics contactNormal;
    float penetrationDepth = 0;

    bool isTrigger = false;
};

// Raycast result
struct RaycastHit {
    bool hit = false;
    Vec3Physics point;
    Vec3Physics normal;
    float distance = 0;
    ColliderHandle collider = InvalidCollider;
    RigidbodyHandle rigidbody = InvalidRigidbody;
    Entity* entity = nullptr;
};

// Collision callback
using CollisionCallback = std::function<void(const CollisionInfo& info)>;
using TriggerCallback = std::function<void(ColliderHandle trigger, ColliderHandle other, bool enter)>;

// Rigidbody state
struct RigidbodyState {
    Vec3Physics position;
    QuatPhysics rotation;
    Vec3Physics linearVelocity;
    Vec3Physics angularVelocity;
    bool awake = true;
};

// Vehicle wheel info
struct WheelInfo {
    Vec3Physics connectionPoint;
    Vec3Physics direction = {0, -1, 0};
    Vec3Physics axle = {1, 0, 0};

    float suspensionRestLength = 0.3f;
    float suspensionStiffness = 5.88f;
    float suspensionDamping = 0.88f;
    float maxSuspensionTravel = 0.5f;

    float wheelRadius = 0.4f;
    float wheelFriction = 1.0f;
    float rollInfluence = 0.1f;

    bool isFrontWheel = true;
    bool steerable = true;
    bool powered = true;
    bool hasBrake = true;
};

// Vehicle description
struct VehicleDesc {
    RigidbodyHandle chassis = InvalidRigidbody;
    std::vector<WheelInfo> wheels;

    float maxEngineForce = 2500.0f;
    float maxBrakeForce = 1000.0f;
    float maxSteeringAngle = 0.5f;  // radians

    float enginePowerCurve[10] = {1, 1, 1, 1, 1, 1, 0.9f, 0.8f, 0.7f, 0.5f};
};

// Vehicle handle
using VehicleHandle = uint32_t;
constexpr VehicleHandle InvalidVehicle = 0;

// Physics World
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    // World settings
    void setGravity(const Vec3Physics& gravity);
    Vec3Physics getGravity() const { return m_gravity; }

    // Simulation
    void step(float deltaTime);
    void setTimeStep(float fixedTimeStep) { m_fixedTimeStep = fixedTimeStep; }
    float getTimeStep() const { return m_fixedTimeStep; }

    // Rigidbody management
    RigidbodyHandle createRigidbody(const RigidbodyDesc& desc);
    void destroyRigidbody(RigidbodyHandle body);
    bool rigidbodyExists(RigidbodyHandle body) const;

    // Rigidbody state
    RigidbodyState getRigidbodyState(RigidbodyHandle body) const;
    void setRigidbodyState(RigidbodyHandle body, const RigidbodyState& state);

    void setPosition(RigidbodyHandle body, const Vec3Physics& position);
    void setRotation(RigidbodyHandle body, const QuatPhysics& rotation);
    void setLinearVelocity(RigidbodyHandle body, const Vec3Physics& velocity);
    void setAngularVelocity(RigidbodyHandle body, const Vec3Physics& velocity);

    Vec3Physics getPosition(RigidbodyHandle body) const;
    QuatPhysics getRotation(RigidbodyHandle body) const;
    Vec3Physics getLinearVelocity(RigidbodyHandle body) const;
    Vec3Physics getAngularVelocity(RigidbodyHandle body) const;

    // Forces and impulses
    void applyForce(RigidbodyHandle body, const Vec3Physics& force);
    void applyForceAtPosition(RigidbodyHandle body, const Vec3Physics& force, const Vec3Physics& position);
    void applyTorque(RigidbodyHandle body, const Vec3Physics& torque);
    void applyImpulse(RigidbodyHandle body, const Vec3Physics& impulse);
    void applyImpulseAtPosition(RigidbodyHandle body, const Vec3Physics& impulse, const Vec3Physics& position);

    // Collider management
    ColliderHandle addCollider(RigidbodyHandle body, const ColliderDesc& desc);
    void removeCollider(ColliderHandle collider);

    // Entity association
    void setEntityForRigidbody(RigidbodyHandle body, Entity* entity);
    Entity* getEntityForRigidbody(RigidbodyHandle body) const;

    // Raycasting
    RaycastHit raycast(const Vec3Physics& origin, const Vec3Physics& direction, float maxDistance,
                       CollisionLayer mask = CollisionLayer::All) const;
    std::vector<RaycastHit> raycastAll(const Vec3Physics& origin, const Vec3Physics& direction,
                                        float maxDistance, CollisionLayer mask = CollisionLayer::All) const;

    // Sphere/box casts
    RaycastHit sphereCast(const Vec3Physics& origin, float radius, const Vec3Physics& direction,
                          float maxDistance, CollisionLayer mask = CollisionLayer::All) const;
    std::vector<ColliderHandle> overlapSphere(const Vec3Physics& center, float radius,
                                               CollisionLayer mask = CollisionLayer::All) const;
    std::vector<ColliderHandle> overlapBox(const Vec3Physics& center, const Vec3Physics& halfExtents,
                                            const QuatPhysics& rotation, CollisionLayer mask = CollisionLayer::All) const;

    // Callbacks
    void setCollisionCallback(CollisionCallback callback) { m_collisionCallback = callback; }
    void setTriggerCallback(TriggerCallback callback) { m_triggerCallback = callback; }

    // Vehicle physics
    VehicleHandle createVehicle(const VehicleDesc& desc);
    void destroyVehicle(VehicleHandle vehicle);
    void setVehicleInput(VehicleHandle vehicle, float steering, float throttle, float brake);
    void getVehicleWheelTransform(VehicleHandle vehicle, int wheelIndex, Vec3Physics& position, QuatPhysics& rotation) const;

    // Debug
    void setDebugDraw(bool enabled) { m_debugDraw = enabled; }
    bool debugDrawEnabled() const { return m_debugDraw; }

private:
    Vec3Physics m_gravity = {0, -9.81f, 0};
    float m_fixedTimeStep = 1.0f / 60.0f;
    float m_accumulator = 0;

    // Internal data structures
    struct InternalRigidbody {
        RigidbodyDesc desc;
        RigidbodyState state;
        Entity* entity = nullptr;
        std::vector<ColliderHandle> colliders;
    };

    struct InternalCollider {
        ColliderDesc desc;
        RigidbodyHandle body = InvalidRigidbody;
    };

    struct InternalVehicle {
        VehicleDesc desc;
        float currentSteering = 0;
        float currentThrottle = 0;
        float currentBrake = 0;
        std::vector<float> wheelRotation;
        std::vector<float> suspensionCompression;
    };

    std::unordered_map<RigidbodyHandle, InternalRigidbody> m_rigidbodies;
    std::unordered_map<ColliderHandle, InternalCollider> m_colliders;
    std::unordered_map<VehicleHandle, InternalVehicle> m_vehicles;

    RigidbodyHandle m_nextRigidbodyHandle = 1;
    ColliderHandle m_nextColliderHandle = 1;
    VehicleHandle m_nextVehicleHandle = 1;

    CollisionCallback m_collisionCallback;
    TriggerCallback m_triggerCallback;

    bool m_debugDraw = false;

    // Internal physics step
    void integrateRigidbody(InternalRigidbody& body, float dt);
    void detectCollisions();
    void resolveCollisions();
    void updateVehicles(float dt);

    // Collision detection helpers
    bool testBoxBox(const ColliderDesc& a, const PhysicsTransform& ta,
                    const ColliderDesc& b, const PhysicsTransform& tb,
                    CollisionInfo& info) const;
    bool testSphereSphere(const ColliderDesc& a, const PhysicsTransform& ta,
                          const ColliderDesc& b, const PhysicsTransform& tb,
                          CollisionInfo& info) const;
    bool testBoxSphere(const ColliderDesc& box, const PhysicsTransform& tbox,
                       const ColliderDesc& sphere, const PhysicsTransform& tsphere,
                       CollisionInfo& info) const;
};

// Physics System (global manager)
class PhysicsSystem {
public:
    PhysicsSystem();
    ~PhysicsSystem();

    // Initialize/shutdown
    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // World access
    PhysicsWorld* createWorld();
    void destroyWorld(PhysicsWorld* world);
    PhysicsWorld* defaultWorld() { return m_defaultWorld.get(); }

    // Update all worlds
    void update(float deltaTime);

    // Global settings
    void setDefaultGravity(const Vec3Physics& gravity) { m_defaultGravity = gravity; }

private:
    bool m_initialized = false;
    Vec3Physics m_defaultGravity = {0, -9.81f, 0};
    std::unique_ptr<PhysicsWorld> m_defaultWorld;
    std::vector<std::unique_ptr<PhysicsWorld>> m_worlds;
};

// Global physics system access
PhysicsSystem& getPhysicsSystem();

} // namespace opensaints
