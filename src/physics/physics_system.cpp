#include "physics_system.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace opensaints {

// Vec3Physics implementation

float Vec3Physics::length() const {
    return std::sqrt(x*x + y*y + z*z);
}

Vec3Physics Vec3Physics::normalized() const {
    float len = length();
    if (len < 0.0001f) return {0, 0, 0};
    return {x / len, y / len, z / len};
}

// QuatPhysics implementation

QuatPhysics QuatPhysics::fromAxisAngle(const Vec3Physics& axis, float angle) {
    float halfAngle = angle * 0.5f;
    float s = std::sin(halfAngle);
    Vec3Physics n = axis.normalized();
    return {n.x * s, n.y * s, n.z * s, std::cos(halfAngle)};
}

QuatPhysics QuatPhysics::fromEuler(float pitch, float yaw, float roll) {
    float cy = std::cos(yaw * 0.5f);
    float sy = std::sin(yaw * 0.5f);
    float cp = std::cos(pitch * 0.5f);
    float sp = std::sin(pitch * 0.5f);
    float cr = std::cos(roll * 0.5f);
    float sr = std::sin(roll * 0.5f);

    return {
        sr * cp * cy - cr * sp * sy,
        cr * sp * cy + sr * cp * sy,
        cr * cp * sy - sr * sp * cy,
        cr * cp * cy + sr * sp * sy
    };
}

QuatPhysics QuatPhysics::operator*(const QuatPhysics& other) const {
    return {
        w * other.x + x * other.w + y * other.z - z * other.y,
        w * other.y - x * other.z + y * other.w + z * other.x,
        w * other.z + x * other.y - y * other.x + z * other.w,
        w * other.w - x * other.x - y * other.y - z * other.z
    };
}

Vec3Physics QuatPhysics::rotate(const Vec3Physics& v) const {
    // Quaternion rotation: q * v * q^-1
    Vec3Physics qv = {x, y, z};
    Vec3Physics uv = qv.cross(v);
    Vec3Physics uuv = qv.cross(uv);
    return v + (uv * w + uuv) * 2.0f;
}

QuatPhysics QuatPhysics::normalized() const {
    float len = std::sqrt(x*x + y*y + z*z + w*w);
    if (len < 0.0001f) return identity();
    return {x / len, y / len, z / len, w / len};
}

// PhysicsTransform implementation

Vec3Physics PhysicsTransform::transformPoint(const Vec3Physics& point) const {
    return position + rotation.rotate(point);
}

Vec3Physics PhysicsTransform::transformVector(const Vec3Physics& vec) const {
    return rotation.rotate(vec);
}

PhysicsTransform PhysicsTransform::inverse() const {
    QuatPhysics invRot = rotation.conjugate();
    return {invRot.rotate(position * -1.0f), invRot};
}

// PhysicsWorld implementation

PhysicsWorld::PhysicsWorld() = default;

PhysicsWorld::~PhysicsWorld() = default;

void PhysicsWorld::setGravity(const Vec3Physics& gravity) {
    m_gravity = gravity;
}

void PhysicsWorld::step(float deltaTime) {
    m_accumulator += deltaTime;

    // Fixed timestep integration
    while (m_accumulator >= m_fixedTimeStep) {
        // Integrate rigidbodies
        for (auto& [handle, body] : m_rigidbodies) {
            if (body.desc.type == RigidbodyType::Dynamic) {
                integrateRigidbody(body, m_fixedTimeStep);
            }
        }

        // Update vehicles
        updateVehicles(m_fixedTimeStep);

        // Collision detection and response
        detectCollisions();
        resolveCollisions();

        m_accumulator -= m_fixedTimeStep;
    }
}

void PhysicsWorld::integrateRigidbody(InternalRigidbody& body, float dt) {
    if (!body.state.awake) return;

    auto& state = body.state;
    auto& desc = body.desc;

    // Apply gravity
    if (desc.useGravity) {
        state.linearVelocity += m_gravity * dt;
    }

    // Apply damping
    state.linearVelocity = state.linearVelocity * (1.0f - desc.linearDamping * dt);
    state.angularVelocity = state.angularVelocity * (1.0f - desc.angularDamping * dt);

    // Integrate position
    state.position += state.linearVelocity * dt;

    // Integrate rotation (simplified - should use quaternion integration)
    if (state.angularVelocity.lengthSquared() > 0.0001f) {
        float angularSpeed = state.angularVelocity.length();
        Vec3Physics axis = state.angularVelocity.normalized();
        QuatPhysics deltaRotation = QuatPhysics::fromAxisAngle(axis, angularSpeed * dt);
        state.rotation = (deltaRotation * state.rotation).normalized();
    }

    // Apply rotation constraints
    if (desc.freezeRotationX) state.angularVelocity.x = 0;
    if (desc.freezeRotationY) state.angularVelocity.y = 0;
    if (desc.freezeRotationZ) state.angularVelocity.z = 0;

    // Sleep check
    float linearThreshold = 0.01f;
    float angularThreshold = 0.01f;
    if (state.linearVelocity.lengthSquared() < linearThreshold * linearThreshold &&
        state.angularVelocity.lengthSquared() < angularThreshold * angularThreshold) {
        // Could implement sleep counter here
    }
}

void PhysicsWorld::detectCollisions() {
    // Simple O(n^2) broad phase - would use spatial hashing/BVH in production
    std::vector<std::pair<RigidbodyHandle, RigidbodyHandle>> pairs;

    std::vector<RigidbodyHandle> handles;
    for (const auto& [handle, body] : m_rigidbodies) {
        handles.push_back(handle);
    }

    for (size_t i = 0; i < handles.size(); ++i) {
        for (size_t j = i + 1; j < handles.size(); ++j) {
            RigidbodyHandle a = handles[i];
            RigidbodyHandle b = handles[j];

            const auto& bodyA = m_rigidbodies[a];
            const auto& bodyB = m_rigidbodies[b];

            // Skip if both are static
            if (bodyA.desc.type == RigidbodyType::Static &&
                bodyB.desc.type == RigidbodyType::Static) {
                continue;
            }

            // Check all collider pairs
            for (ColliderHandle colA : bodyA.colliders) {
                for (ColliderHandle colB : bodyB.colliders) {
                    const auto& colliderA = m_colliders[colA];
                    const auto& colliderB = m_colliders[colB];

                    // Check layer masks
                    uint32_t layerA = static_cast<uint32_t>(colliderA.desc.layer);
                    uint32_t layerB = static_cast<uint32_t>(colliderB.desc.layer);
                    uint32_t maskA = static_cast<uint32_t>(colliderA.desc.mask);
                    uint32_t maskB = static_cast<uint32_t>(colliderB.desc.mask);

                    if (!(layerA & maskB) || !(layerB & maskA)) {
                        continue;
                    }

                    // Narrow phase collision test
                    PhysicsTransform transformA(bodyA.state.position, bodyA.state.rotation);
                    PhysicsTransform transformB(bodyB.state.position, bodyB.state.rotation);

                    CollisionInfo info;
                    info.colliderA = colA;
                    info.colliderB = colB;
                    info.bodyA = a;
                    info.bodyB = b;
                    info.entityA = bodyA.entity;
                    info.entityB = bodyB.entity;
                    info.isTrigger = colliderA.desc.isTrigger || colliderB.desc.isTrigger;

                    bool collision = false;

                    // Test based on shape types
                    if (colliderA.desc.type == ColliderType::Sphere &&
                        colliderB.desc.type == ColliderType::Sphere) {
                        collision = testSphereSphere(colliderA.desc, transformA,
                                                     colliderB.desc, transformB, info);
                    } else if (colliderA.desc.type == ColliderType::Box &&
                               colliderB.desc.type == ColliderType::Box) {
                        collision = testBoxBox(colliderA.desc, transformA,
                                               colliderB.desc, transformB, info);
                    } else if (colliderA.desc.type == ColliderType::Box &&
                               colliderB.desc.type == ColliderType::Sphere) {
                        collision = testBoxSphere(colliderA.desc, transformA,
                                                  colliderB.desc, transformB, info);
                    } else if (colliderA.desc.type == ColliderType::Sphere &&
                               colliderB.desc.type == ColliderType::Box) {
                        collision = testBoxSphere(colliderB.desc, transformB,
                                                  colliderA.desc, transformA, info);
                        std::swap(info.colliderA, info.colliderB);
                        std::swap(info.bodyA, info.bodyB);
                        std::swap(info.entityA, info.entityB);
                        info.contactNormal = info.contactNormal * -1.0f;
                    }

                    if (collision) {
                        if (info.isTrigger) {
                            if (m_triggerCallback) {
                                m_triggerCallback(info.colliderA, info.colliderB, true);
                            }
                        } else {
                            if (m_collisionCallback) {
                                m_collisionCallback(info);
                            }
                        }
                    }
                }
            }
        }
    }
}

void PhysicsWorld::resolveCollisions() {
    // Collision resolution would go here
    // For now, just a placeholder
}

void PhysicsWorld::updateVehicles(float dt) {
    for (auto& [handle, vehicle] : m_vehicles) {
        if (vehicle.desc.chassis == InvalidRigidbody) continue;

        auto bodyIt = m_rigidbodies.find(vehicle.desc.chassis);
        if (bodyIt == m_rigidbodies.end()) continue;

        auto& chassisBody = bodyIt->second;

        // Apply engine force
        float engineForce = vehicle.currentThrottle * vehicle.desc.maxEngineForce;
        float brakeForce = vehicle.currentBrake * vehicle.desc.maxBrakeForce;

        // Calculate forward vector
        Vec3Physics forward = chassisBody.state.rotation.rotate(Vec3Physics::forward());

        // Apply forces to chassis
        if (engineForce > 0) {
            chassisBody.state.linearVelocity += forward * (engineForce / chassisBody.desc.mass * dt);
        }

        // Apply braking (reduce velocity)
        if (brakeForce > 0) {
            float speed = chassisBody.state.linearVelocity.length();
            if (speed > 0.01f) {
                float reduction = std::min(speed, brakeForce / chassisBody.desc.mass * dt);
                chassisBody.state.linearVelocity = chassisBody.state.linearVelocity *
                    ((speed - reduction) / speed);
            }
        }

        // Apply steering (simplified)
        if (std::abs(vehicle.currentSteering) > 0.001f) {
            float steerAngle = vehicle.currentSteering * vehicle.desc.maxSteeringAngle;
            float speed = chassisBody.state.linearVelocity.length();
            if (speed > 0.5f) {
                float turnRate = steerAngle * speed * 0.1f;
                chassisBody.state.angularVelocity.y += turnRate * dt;
            }
        }

        // Update wheel rotations
        float speed = chassisBody.state.linearVelocity.length();
        for (size_t i = 0; i < vehicle.desc.wheels.size(); ++i) {
            if (i >= vehicle.wheelRotation.size()) {
                vehicle.wheelRotation.resize(vehicle.desc.wheels.size(), 0);
                vehicle.suspensionCompression.resize(vehicle.desc.wheels.size(), 0);
            }

            const auto& wheel = vehicle.desc.wheels[i];
            vehicle.wheelRotation[i] += speed / wheel.wheelRadius * dt;
        }
    }
}

bool PhysicsWorld::testSphereSphere(const ColliderDesc& a, const PhysicsTransform& ta,
                                     const ColliderDesc& b, const PhysicsTransform& tb,
                                     CollisionInfo& info) const {
    Vec3Physics posA = ta.transformPoint(a.offset);
    Vec3Physics posB = tb.transformPoint(b.offset);

    Vec3Physics diff = posB - posA;
    float dist = diff.length();
    float combinedRadius = a.sphereRadius + b.sphereRadius;

    if (dist < combinedRadius) {
        info.penetrationDepth = combinedRadius - dist;
        info.contactNormal = dist > 0.0001f ? diff.normalized() : Vec3Physics::up();
        info.contactPoint = posA + info.contactNormal * a.sphereRadius;
        return true;
    }

    return false;
}

bool PhysicsWorld::testBoxBox(const ColliderDesc& a, const PhysicsTransform& ta,
                               const ColliderDesc& b, const PhysicsTransform& tb,
                               CollisionInfo& info) const {
    // Simplified AABB test (ignores rotation)
    Vec3Physics posA = ta.transformPoint(a.offset);
    Vec3Physics posB = tb.transformPoint(b.offset);

    Vec3Physics diff = posB - posA;

    float overlapX = a.boxHalfExtents.x + b.boxHalfExtents.x - std::abs(diff.x);
    float overlapY = a.boxHalfExtents.y + b.boxHalfExtents.y - std::abs(diff.y);
    float overlapZ = a.boxHalfExtents.z + b.boxHalfExtents.z - std::abs(diff.z);

    if (overlapX > 0 && overlapY > 0 && overlapZ > 0) {
        // Find minimum penetration axis
        if (overlapX < overlapY && overlapX < overlapZ) {
            info.penetrationDepth = overlapX;
            info.contactNormal = {diff.x > 0 ? 1.0f : -1.0f, 0, 0};
        } else if (overlapY < overlapZ) {
            info.penetrationDepth = overlapY;
            info.contactNormal = {0, diff.y > 0 ? 1.0f : -1.0f, 0};
        } else {
            info.penetrationDepth = overlapZ;
            info.contactNormal = {0, 0, diff.z > 0 ? 1.0f : -1.0f};
        }

        info.contactPoint = posA + diff * 0.5f;
        return true;
    }

    return false;
}

bool PhysicsWorld::testBoxSphere(const ColliderDesc& box, const PhysicsTransform& tbox,
                                  const ColliderDesc& sphere, const PhysicsTransform& tsphere,
                                  CollisionInfo& info) const {
    // Transform sphere center to box local space
    Vec3Physics boxPos = tbox.transformPoint(box.offset);
    Vec3Physics spherePos = tsphere.transformPoint(sphere.offset);
    Vec3Physics local = spherePos - boxPos;

    // Find closest point on box to sphere center
    Vec3Physics closest;
    closest.x = std::clamp(local.x, -box.boxHalfExtents.x, box.boxHalfExtents.x);
    closest.y = std::clamp(local.y, -box.boxHalfExtents.y, box.boxHalfExtents.y);
    closest.z = std::clamp(local.z, -box.boxHalfExtents.z, box.boxHalfExtents.z);

    Vec3Physics diff = local - closest;
    float distSq = diff.lengthSquared();

    if (distSq < sphere.sphereRadius * sphere.sphereRadius) {
        float dist = std::sqrt(distSq);
        info.penetrationDepth = sphere.sphereRadius - dist;
        info.contactNormal = dist > 0.0001f ? diff.normalized() : Vec3Physics::up();
        info.contactPoint = boxPos + closest;
        return true;
    }

    return false;
}

RigidbodyHandle PhysicsWorld::createRigidbody(const RigidbodyDesc& desc) {
    RigidbodyHandle handle = m_nextRigidbodyHandle++;

    InternalRigidbody body;
    body.desc = desc;
    body.state.position = desc.position;
    body.state.rotation = desc.rotation;
    body.state.linearVelocity = Vec3Physics::zero();
    body.state.angularVelocity = Vec3Physics::zero();
    body.state.awake = true;

    // Create colliders
    for (const auto& colDesc : desc.colliders) {
        ColliderHandle colHandle = addCollider(handle, colDesc);
        body.colliders.push_back(colHandle);
    }

    m_rigidbodies[handle] = body;
    return handle;
}

void PhysicsWorld::destroyRigidbody(RigidbodyHandle body) {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end()) {
        // Remove all colliders
        for (ColliderHandle col : it->second.colliders) {
            m_colliders.erase(col);
        }
        m_rigidbodies.erase(it);
    }
}

bool PhysicsWorld::rigidbodyExists(RigidbodyHandle body) const {
    return m_rigidbodies.find(body) != m_rigidbodies.end();
}

RigidbodyState PhysicsWorld::getRigidbodyState(RigidbodyHandle body) const {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end()) {
        return it->second.state;
    }
    return RigidbodyState{};
}

void PhysicsWorld::setRigidbodyState(RigidbodyHandle body, const RigidbodyState& state) {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end()) {
        it->second.state = state;
    }
}

void PhysicsWorld::setPosition(RigidbodyHandle body, const Vec3Physics& position) {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end()) {
        it->second.state.position = position;
    }
}

void PhysicsWorld::setRotation(RigidbodyHandle body, const QuatPhysics& rotation) {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end()) {
        it->second.state.rotation = rotation;
    }
}

void PhysicsWorld::setLinearVelocity(RigidbodyHandle body, const Vec3Physics& velocity) {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end()) {
        it->second.state.linearVelocity = velocity;
    }
}

void PhysicsWorld::setAngularVelocity(RigidbodyHandle body, const Vec3Physics& velocity) {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end()) {
        it->second.state.angularVelocity = velocity;
    }
}

Vec3Physics PhysicsWorld::getPosition(RigidbodyHandle body) const {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end()) {
        return it->second.state.position;
    }
    return Vec3Physics::zero();
}

QuatPhysics PhysicsWorld::getRotation(RigidbodyHandle body) const {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end()) {
        return it->second.state.rotation;
    }
    return QuatPhysics::identity();
}

Vec3Physics PhysicsWorld::getLinearVelocity(RigidbodyHandle body) const {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end()) {
        return it->second.state.linearVelocity;
    }
    return Vec3Physics::zero();
}

Vec3Physics PhysicsWorld::getAngularVelocity(RigidbodyHandle body) const {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end()) {
        return it->second.state.angularVelocity;
    }
    return Vec3Physics::zero();
}

void PhysicsWorld::applyForce(RigidbodyHandle body, const Vec3Physics& force) {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end() && it->second.desc.type == RigidbodyType::Dynamic) {
        float invMass = 1.0f / it->second.desc.mass;
        it->second.state.linearVelocity += force * (invMass * m_fixedTimeStep);
        it->second.state.awake = true;
    }
}

void PhysicsWorld::applyForceAtPosition(RigidbodyHandle body, const Vec3Physics& force,
                                         const Vec3Physics& position) {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end() && it->second.desc.type == RigidbodyType::Dynamic) {
        // Apply linear force
        applyForce(body, force);

        // Apply torque
        Vec3Physics relPos = position - it->second.state.position;
        Vec3Physics torque = relPos.cross(force);
        applyTorque(body, torque);
    }
}

void PhysicsWorld::applyTorque(RigidbodyHandle body, const Vec3Physics& torque) {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end() && it->second.desc.type == RigidbodyType::Dynamic) {
        // Simplified - should use inertia tensor
        float invMass = 1.0f / it->second.desc.mass;
        it->second.state.angularVelocity += torque * (invMass * m_fixedTimeStep);
        it->second.state.awake = true;
    }
}

void PhysicsWorld::applyImpulse(RigidbodyHandle body, const Vec3Physics& impulse) {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end() && it->second.desc.type == RigidbodyType::Dynamic) {
        float invMass = 1.0f / it->second.desc.mass;
        it->second.state.linearVelocity += impulse * invMass;
        it->second.state.awake = true;
    }
}

void PhysicsWorld::applyImpulseAtPosition(RigidbodyHandle body, const Vec3Physics& impulse,
                                           const Vec3Physics& position) {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end() && it->second.desc.type == RigidbodyType::Dynamic) {
        applyImpulse(body, impulse);

        Vec3Physics relPos = position - it->second.state.position;
        Vec3Physics angularImpulse = relPos.cross(impulse);
        float invMass = 1.0f / it->second.desc.mass;
        it->second.state.angularVelocity += angularImpulse * invMass;
    }
}

ColliderHandle PhysicsWorld::addCollider(RigidbodyHandle body, const ColliderDesc& desc) {
    ColliderHandle handle = m_nextColliderHandle++;

    InternalCollider collider;
    collider.desc = desc;
    collider.body = body;

    m_colliders[handle] = collider;

    auto bodyIt = m_rigidbodies.find(body);
    if (bodyIt != m_rigidbodies.end()) {
        bodyIt->second.colliders.push_back(handle);
    }

    return handle;
}

void PhysicsWorld::removeCollider(ColliderHandle collider) {
    auto it = m_colliders.find(collider);
    if (it != m_colliders.end()) {
        // Remove from body's collider list
        auto bodyIt = m_rigidbodies.find(it->second.body);
        if (bodyIt != m_rigidbodies.end()) {
            auto& colliders = bodyIt->second.colliders;
            colliders.erase(std::remove(colliders.begin(), colliders.end(), collider),
                           colliders.end());
        }
        m_colliders.erase(it);
    }
}

void PhysicsWorld::setEntityForRigidbody(RigidbodyHandle body, Entity* entity) {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end()) {
        it->second.entity = entity;
    }
}

Entity* PhysicsWorld::getEntityForRigidbody(RigidbodyHandle body) const {
    auto it = m_rigidbodies.find(body);
    if (it != m_rigidbodies.end()) {
        return it->second.entity;
    }
    return nullptr;
}

RaycastHit PhysicsWorld::raycast(const Vec3Physics& origin, const Vec3Physics& direction,
                                  float maxDistance, CollisionLayer mask) const {
    RaycastHit closest;
    closest.distance = maxDistance;

    Vec3Physics dir = direction.normalized();

    for (const auto& [handle, body] : m_rigidbodies) {
        for (ColliderHandle colHandle : body.colliders) {
            auto colIt = m_colliders.find(colHandle);
            if (colIt == m_colliders.end()) continue;

            const auto& collider = colIt->second;

            // Check layer mask
            if (!(static_cast<uint32_t>(collider.desc.layer) & static_cast<uint32_t>(mask))) {
                continue;
            }

            // Raycast against shape
            PhysicsTransform transform(body.state.position, body.state.rotation);
            Vec3Physics center = transform.transformPoint(collider.desc.offset);

            float hitDist = -1;
            Vec3Physics hitNormal;

            if (collider.desc.type == ColliderType::Sphere) {
                // Ray-sphere intersection
                Vec3Physics oc = origin - center;
                float a = dir.dot(dir);
                float b = 2.0f * oc.dot(dir);
                float c = oc.dot(oc) - collider.desc.sphereRadius * collider.desc.sphereRadius;
                float discriminant = b * b - 4 * a * c;

                if (discriminant >= 0) {
                    hitDist = (-b - std::sqrt(discriminant)) / (2.0f * a);
                    if (hitDist >= 0 && hitDist < closest.distance) {
                        Vec3Physics hitPoint = origin + dir * hitDist;
                        hitNormal = (hitPoint - center).normalized();
                    }
                }
            } else if (collider.desc.type == ColliderType::Box) {
                // Ray-AABB intersection (simplified, ignores rotation)
                Vec3Physics invDir = {1.0f / dir.x, 1.0f / dir.y, 1.0f / dir.z};
                Vec3Physics t1 = {
                    (center.x - collider.desc.boxHalfExtents.x - origin.x) * invDir.x,
                    (center.y - collider.desc.boxHalfExtents.y - origin.y) * invDir.y,
                    (center.z - collider.desc.boxHalfExtents.z - origin.z) * invDir.z
                };
                Vec3Physics t2 = {
                    (center.x + collider.desc.boxHalfExtents.x - origin.x) * invDir.x,
                    (center.y + collider.desc.boxHalfExtents.y - origin.y) * invDir.y,
                    (center.z + collider.desc.boxHalfExtents.z - origin.z) * invDir.z
                };

                float tmin = std::max({std::min(t1.x, t2.x), std::min(t1.y, t2.y), std::min(t1.z, t2.z)});
                float tmax = std::min({std::max(t1.x, t2.x), std::max(t1.y, t2.y), std::max(t1.z, t2.z)});

                if (tmax >= 0 && tmin <= tmax && tmin < closest.distance) {
                    hitDist = tmin >= 0 ? tmin : tmax;
                    // Approximate normal
                    Vec3Physics hitPoint = origin + dir * hitDist;
                    Vec3Physics local = hitPoint - center;
                    if (std::abs(local.x) > std::abs(local.y) && std::abs(local.x) > std::abs(local.z)) {
                        hitNormal = {local.x > 0 ? 1.0f : -1.0f, 0, 0};
                    } else if (std::abs(local.y) > std::abs(local.z)) {
                        hitNormal = {0, local.y > 0 ? 1.0f : -1.0f, 0};
                    } else {
                        hitNormal = {0, 0, local.z > 0 ? 1.0f : -1.0f};
                    }
                }
            }

            if (hitDist >= 0 && hitDist < closest.distance) {
                closest.hit = true;
                closest.distance = hitDist;
                closest.point = origin + dir * hitDist;
                closest.normal = hitNormal;
                closest.collider = colHandle;
                closest.rigidbody = handle;
                closest.entity = body.entity;
            }
        }
    }

    return closest;
}

std::vector<RaycastHit> PhysicsWorld::raycastAll(const Vec3Physics& origin, const Vec3Physics& direction,
                                                   float maxDistance, CollisionLayer mask) const {
    std::vector<RaycastHit> hits;
    // Would implement similarly to raycast but collect all hits
    return hits;
}

RaycastHit PhysicsWorld::sphereCast(const Vec3Physics& origin, float radius, const Vec3Physics& direction,
                                     float maxDistance, CollisionLayer mask) const {
    // Simplified - use raycast with expanded shapes
    return raycast(origin, direction, maxDistance, mask);
}

std::vector<ColliderHandle> PhysicsWorld::overlapSphere(const Vec3Physics& center, float radius,
                                                         CollisionLayer mask) const {
    std::vector<ColliderHandle> result;

    for (const auto& [handle, body] : m_rigidbodies) {
        for (ColliderHandle colHandle : body.colliders) {
            auto colIt = m_colliders.find(colHandle);
            if (colIt == m_colliders.end()) continue;

            const auto& collider = colIt->second;

            if (!(static_cast<uint32_t>(collider.desc.layer) & static_cast<uint32_t>(mask))) {
                continue;
            }

            PhysicsTransform transform(body.state.position, body.state.rotation);
            Vec3Physics colCenter = transform.transformPoint(collider.desc.offset);

            float dist = (colCenter - center).length();
            float combinedRadius = radius;

            if (collider.desc.type == ColliderType::Sphere) {
                combinedRadius += collider.desc.sphereRadius;
            } else if (collider.desc.type == ColliderType::Box) {
                // Approximate with bounding sphere
                combinedRadius += collider.desc.boxHalfExtents.length();
            }

            if (dist < combinedRadius) {
                result.push_back(colHandle);
            }
        }
    }

    return result;
}

std::vector<ColliderHandle> PhysicsWorld::overlapBox(const Vec3Physics& center, const Vec3Physics& halfExtents,
                                                       const QuatPhysics& rotation, CollisionLayer mask) const {
    std::vector<ColliderHandle> result;
    // Would implement box overlap test
    return result;
}

VehicleHandle PhysicsWorld::createVehicle(const VehicleDesc& desc) {
    VehicleHandle handle = m_nextVehicleHandle++;

    InternalVehicle vehicle;
    vehicle.desc = desc;
    vehicle.wheelRotation.resize(desc.wheels.size(), 0);
    vehicle.suspensionCompression.resize(desc.wheels.size(), 0);

    m_vehicles[handle] = vehicle;
    return handle;
}

void PhysicsWorld::destroyVehicle(VehicleHandle vehicle) {
    m_vehicles.erase(vehicle);
}

void PhysicsWorld::setVehicleInput(VehicleHandle vehicle, float steering, float throttle, float brake) {
    auto it = m_vehicles.find(vehicle);
    if (it != m_vehicles.end()) {
        it->second.currentSteering = std::clamp(steering, -1.0f, 1.0f);
        it->second.currentThrottle = std::clamp(throttle, 0.0f, 1.0f);
        it->second.currentBrake = std::clamp(brake, 0.0f, 1.0f);
    }
}

void PhysicsWorld::getVehicleWheelTransform(VehicleHandle vehicle, int wheelIndex,
                                             Vec3Physics& position, QuatPhysics& rotation) const {
    auto it = m_vehicles.find(vehicle);
    if (it != m_vehicles.end() && wheelIndex >= 0 &&
        wheelIndex < static_cast<int>(it->second.desc.wheels.size())) {

        const auto& wheel = it->second.desc.wheels[wheelIndex];
        const auto& veh = it->second;

        // Get chassis transform
        auto bodyIt = m_rigidbodies.find(veh.desc.chassis);
        if (bodyIt != m_rigidbodies.end()) {
            PhysicsTransform chassisTransform(bodyIt->second.state.position,
                                               bodyIt->second.state.rotation);

            // Calculate wheel position with suspension
            float compression = wheelIndex < static_cast<int>(veh.suspensionCompression.size())
                ? veh.suspensionCompression[wheelIndex] : 0;
            Vec3Physics localPos = wheel.connectionPoint +
                wheel.direction * (wheel.suspensionRestLength - compression);
            position = chassisTransform.transformPoint(localPos);

            // Calculate wheel rotation
            float wheelRot = wheelIndex < static_cast<int>(veh.wheelRotation.size())
                ? veh.wheelRotation[wheelIndex] : 0;
            QuatPhysics spinRotation = QuatPhysics::fromAxisAngle(wheel.axle, wheelRot);

            // Add steering rotation for front wheels
            QuatPhysics steerRotation = QuatPhysics::identity();
            if (wheel.steerable) {
                steerRotation = QuatPhysics::fromAxisAngle(Vec3Physics::up(),
                    veh.currentSteering * veh.desc.maxSteeringAngle);
            }

            rotation = (bodyIt->second.state.rotation * steerRotation * spinRotation).normalized();
        }
    }
}

// PhysicsSystem implementation

PhysicsSystem::PhysicsSystem() = default;

PhysicsSystem::~PhysicsSystem() {
    shutdown();
}

bool PhysicsSystem::initialize() {
    if (m_initialized) return true;

    m_defaultWorld = std::make_unique<PhysicsWorld>();
    m_defaultWorld->setGravity(m_defaultGravity);

    m_initialized = true;
    std::cout << "Physics system initialized\n";
    return true;
}

void PhysicsSystem::shutdown() {
    m_defaultWorld.reset();
    m_worlds.clear();
    m_initialized = false;
}

PhysicsWorld* PhysicsSystem::createWorld() {
    auto world = std::make_unique<PhysicsWorld>();
    world->setGravity(m_defaultGravity);
    PhysicsWorld* ptr = world.get();
    m_worlds.push_back(std::move(world));
    return ptr;
}

void PhysicsSystem::destroyWorld(PhysicsWorld* world) {
    m_worlds.erase(
        std::remove_if(m_worlds.begin(), m_worlds.end(),
            [world](const auto& ptr) { return ptr.get() == world; }),
        m_worlds.end()
    );
}

void PhysicsSystem::update(float deltaTime) {
    if (m_defaultWorld) {
        m_defaultWorld->step(deltaTime);
    }
    for (auto& world : m_worlds) {
        world->step(deltaTime);
    }
}

// Global physics system singleton
PhysicsSystem& getPhysicsSystem() {
    static PhysicsSystem instance;
    return instance;
}

} // namespace opensaints
