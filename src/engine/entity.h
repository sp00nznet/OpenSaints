#pragma once
// Entity Component System for OpenSaints
// Provides flexible game object management

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <typeindex>
#include <functional>

#include "render/renderer.h"

namespace opensaints {

// Forward declarations
class Entity;
class EntityManager;
class Component;

// Entity ID type
using EntityId = uint64_t;
constexpr EntityId InvalidEntityId = 0;

// Component type ID
using ComponentTypeId = std::type_index;

// Transform component data
struct Transform {
    float position[3] = {0, 0, 0};
    float rotation[4] = {0, 0, 0, 1}; // Quaternion (x, y, z, w)
    float scale[3] = {1, 1, 1};

    // Helpers
    void setPosition(float x, float y, float z);
    void setRotation(float x, float y, float z, float w);
    void setEulerAngles(float pitch, float yaw, float roll);
    void setScale(float x, float y, float z);
    void setUniformScale(float s);

    void translate(float dx, float dy, float dz);
    void rotate(float angle, float axisX, float axisY, float axisZ);

    void getMatrix(float* matrix) const;
    void getForward(float* dir) const;
    void getRight(float* dir) const;
    void getUp(float* dir) const;
};

// Base component class
class Component {
public:
    virtual ~Component() = default;

    // Get owning entity
    Entity* getEntity() const { return m_entity; }
    EntityId getEntityId() const;

    // Component lifecycle
    virtual void onAttach() {}
    virtual void onDetach() {}
    virtual void onUpdate(float deltaTime) {}

    // Enable/disable
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    // Component type
    virtual ComponentTypeId getTypeId() const = 0;

protected:
    friend class Entity;
    Entity* m_entity = nullptr;
    bool m_enabled = true;
};

// Template for getting component type ID
template<typename T>
ComponentTypeId getComponentTypeId() {
    return std::type_index(typeid(T));
}

// Entity class
class Entity {
public:
    Entity(EntityId id, EntityManager* manager);
    ~Entity();

    // Identity
    EntityId getId() const { return m_id; }
    const std::string& getName() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    // Tags
    void addTag(const std::string& tag);
    void removeTag(const std::string& tag);
    bool hasTag(const std::string& tag) const;
    const std::vector<std::string>& getTags() const { return m_tags; }

    // Transform (always present)
    Transform& transform() { return m_transform; }
    const Transform& transform() const { return m_transform; }

    // Component management
    template<typename T, typename... Args>
    T* addComponent(Args&&... args) {
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = component.get();
        addComponentInternal(std::move(component));
        return ptr;
    }

    template<typename T>
    T* getComponent() {
        auto it = m_components.find(getComponentTypeId<T>());
        if (it != m_components.end()) {
            return static_cast<T*>(it->second.get());
        }
        return nullptr;
    }

    template<typename T>
    const T* getComponent() const {
        auto it = m_components.find(getComponentTypeId<T>());
        if (it != m_components.end()) {
            return static_cast<const T*>(it->second.get());
        }
        return nullptr;
    }

    template<typename T>
    bool hasComponent() const {
        return m_components.find(getComponentTypeId<T>()) != m_components.end();
    }

    template<typename T>
    void removeComponent() {
        auto it = m_components.find(getComponentTypeId<T>());
        if (it != m_components.end()) {
            it->second->onDetach();
            m_components.erase(it);
        }
    }

    // Get all components
    std::vector<Component*> getComponents();

    // Enable/disable entity
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    // Parent/child hierarchy
    Entity* getParent() const { return m_parent; }
    void setParent(Entity* parent);
    const std::vector<Entity*>& getChildren() const { return m_children; }
    void addChild(Entity* child);
    void removeChild(Entity* child);

    // Lifecycle
    void update(float deltaTime);

    // Get manager
    EntityManager* getManager() const { return m_manager; }

private:
    void addComponentInternal(std::unique_ptr<Component> component);

    EntityId m_id;
    std::string m_name;
    std::vector<std::string> m_tags;
    Transform m_transform;
    bool m_enabled = true;

    std::unordered_map<ComponentTypeId, std::unique_ptr<Component>> m_components;

    Entity* m_parent = nullptr;
    std::vector<Entity*> m_children;

    EntityManager* m_manager;
};

// Entity Manager
class EntityManager {
public:
    EntityManager();
    ~EntityManager();

    // Entity creation/destruction
    Entity* createEntity(const std::string& name = "");
    void destroyEntity(EntityId id);
    void destroyEntity(Entity* entity);
    void destroyAllEntities();

    // Entity lookup
    Entity* getEntity(EntityId id);
    Entity* findByName(const std::string& name);
    std::vector<Entity*> findByTag(const std::string& tag);

    // Get all entities
    const std::unordered_map<EntityId, std::unique_ptr<Entity>>& getEntities() const {
        return m_entities;
    }

    // Query entities with specific component
    template<typename T>
    std::vector<Entity*> getEntitiesWithComponent() {
        std::vector<Entity*> result;
        for (auto& [id, entity] : m_entities) {
            if (entity->hasComponent<T>()) {
                result.push_back(entity.get());
            }
        }
        return result;
    }

    // Update all entities
    void update(float deltaTime);

    // Statistics
    size_t getEntityCount() const { return m_entities.size(); }

private:
    std::unordered_map<EntityId, std::unique_ptr<Entity>> m_entities;
    EntityId m_nextId = 1;
    std::vector<EntityId> m_pendingDestroy;

    void processPendingDestroy();
};

// Common component types

// Per-submesh rendering data
struct SubmeshGPU {
    uint32_t indexCount = 0;
    uint32_t indexStart = 0;
    int32_t vertexOffset = 0;
    uint32_t vertexCount = 0;
    uint32_t vertexStart = 0;
    TextureHandle texture = InvalidTexture;
};

// Mesh renderer component — holds GPU resource handles for rendering
class MeshRendererComponent : public Component {
public:
    ComponentTypeId getTypeId() const override { return getComponentTypeId<MeshRendererComponent>(); }

    std::string meshName;
    bool visible = true;

    // GPU resources
    BufferHandle vertexBuffer = InvalidBuffer;
    BufferHandle indexBuffer = InvalidBuffer;
    std::vector<SubmeshGPU> submeshes;
};

// Collider component (placeholder)
class ColliderComponent : public Component {
public:
    ComponentTypeId getTypeId() const override { return getComponentTypeId<ColliderComponent>(); }

    enum class Shape { Box, Sphere, Capsule, Mesh };
    Shape shape = Shape::Box;
    float size[3] = {1, 1, 1};
    float radius = 0.5f;
    bool isTrigger = false;
};

// Rigidbody component (placeholder)
class RigidbodyComponent : public Component {
public:
    ComponentTypeId getTypeId() const override { return getComponentTypeId<RigidbodyComponent>(); }

    float mass = 1.0f;
    float drag = 0.0f;
    float angularDrag = 0.05f;
    bool useGravity = true;
    bool isKinematic = false;

    float velocity[3] = {0, 0, 0};
    float angularVelocity[3] = {0, 0, 0};
};

// Audio source component (placeholder)
class AudioSourceComponent : public Component {
public:
    ComponentTypeId getTypeId() const override { return getComponentTypeId<AudioSourceComponent>(); }

    std::string audioClip;
    float volume = 1.0f;
    float pitch = 1.0f;
    float minDistance = 1.0f;
    float maxDistance = 500.0f;
    bool loop = false;
    bool playOnAwake = false;
    bool spatial = true;
};

// Light component
class LightComponent : public Component {
public:
    ComponentTypeId getTypeId() const override { return getComponentTypeId<LightComponent>(); }

    enum class Type { Directional, Point, Spot };
    Type type = Type::Point;
    float color[3] = {1, 1, 1};
    float intensity = 1.0f;
    float range = 10.0f;
    float spotAngle = 45.0f;
    bool castShadows = false;
};

// Camera component
class CameraComponent : public Component {
public:
    ComponentTypeId getTypeId() const override { return getComponentTypeId<CameraComponent>(); }

    float fov = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    bool orthographic = false;
    float orthoSize = 5.0f;
    int renderOrder = 0;

    void getProjectionMatrix(float* matrix, float aspect) const;
    void getViewMatrix(float* matrix) const;
};

// Script component base (for game logic)
class ScriptComponent : public Component {
public:
    ComponentTypeId getTypeId() const override { return getComponentTypeId<ScriptComponent>(); }

    virtual void onStart() {}
    virtual void onUpdate(float deltaTime) override {}
    virtual void onFixedUpdate(float fixedDeltaTime) {}
    virtual void onDestroy() {}

    virtual void onCollisionEnter(Entity* other) {}
    virtual void onCollisionExit(Entity* other) {}
    virtual void onTriggerEnter(Entity* other) {}
    virtual void onTriggerExit(Entity* other) {}
};

// Forward declaration
class RenderQueue;

// Collect all visible MeshRendererComponents into a RenderQueue
// Uses each entity's Transform to produce per-object model matrices
void collectRenderItems(EntityManager& manager, RenderQueue& queue);

} // namespace opensaints
