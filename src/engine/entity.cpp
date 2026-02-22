#include "entity.h"
#include "render/render_queue.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

namespace opensaints {

// Transform implementation

void Transform::setPosition(float x, float y, float z) {
    position[0] = x;
    position[1] = y;
    position[2] = z;
}

void Transform::setRotation(float x, float y, float z, float w) {
    rotation[0] = x;
    rotation[1] = y;
    rotation[2] = z;
    rotation[3] = w;
}

void Transform::setEulerAngles(float pitch, float yaw, float roll) {
    // Convert Euler angles to quaternion
    float cy = std::cos(yaw * 0.5f);
    float sy = std::sin(yaw * 0.5f);
    float cp = std::cos(pitch * 0.5f);
    float sp = std::sin(pitch * 0.5f);
    float cr = std::cos(roll * 0.5f);
    float sr = std::sin(roll * 0.5f);

    rotation[3] = cr * cp * cy + sr * sp * sy; // w
    rotation[0] = sr * cp * cy - cr * sp * sy; // x
    rotation[1] = cr * sp * cy + sr * cp * sy; // y
    rotation[2] = cr * cp * sy - sr * sp * cy; // z
}

void Transform::setScale(float x, float y, float z) {
    scale[0] = x;
    scale[1] = y;
    scale[2] = z;
}

void Transform::setUniformScale(float s) {
    scale[0] = scale[1] = scale[2] = s;
}

void Transform::translate(float dx, float dy, float dz) {
    position[0] += dx;
    position[1] += dy;
    position[2] += dz;
}

void Transform::rotate(float angle, float axisX, float axisY, float axisZ) {
    // Normalize axis
    float len = std::sqrt(axisX * axisX + axisY * axisY + axisZ * axisZ);
    if (len < 0.0001f) return;
    axisX /= len;
    axisY /= len;
    axisZ /= len;

    // Create rotation quaternion
    float halfAngle = angle * 0.5f;
    float s = std::sin(halfAngle);
    float qx = axisX * s;
    float qy = axisY * s;
    float qz = axisZ * s;
    float qw = std::cos(halfAngle);

    // Multiply quaternions (q * rotation)
    float newW = rotation[3] * qw - rotation[0] * qx - rotation[1] * qy - rotation[2] * qz;
    float newX = rotation[3] * qx + rotation[0] * qw + rotation[1] * qz - rotation[2] * qy;
    float newY = rotation[3] * qy - rotation[0] * qz + rotation[1] * qw + rotation[2] * qx;
    float newZ = rotation[3] * qz + rotation[0] * qy - rotation[1] * qx + rotation[2] * qw;

    rotation[0] = newX;
    rotation[1] = newY;
    rotation[2] = newZ;
    rotation[3] = newW;

    // Normalize
    len = std::sqrt(newX * newX + newY * newY + newZ * newZ + newW * newW);
    rotation[0] /= len;
    rotation[1] /= len;
    rotation[2] /= len;
    rotation[3] /= len;
}

void Transform::getMatrix(float* matrix) const {
    // Convert quaternion to rotation matrix and combine with scale/position
    float x = rotation[0], y = rotation[1], z = rotation[2], w = rotation[3];
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    matrix[0] = (1.0f - (yy + zz)) * scale[0];
    matrix[1] = (xy + wz) * scale[0];
    matrix[2] = (xz - wy) * scale[0];
    matrix[3] = 0.0f;

    matrix[4] = (xy - wz) * scale[1];
    matrix[5] = (1.0f - (xx + zz)) * scale[1];
    matrix[6] = (yz + wx) * scale[1];
    matrix[7] = 0.0f;

    matrix[8] = (xz + wy) * scale[2];
    matrix[9] = (yz - wx) * scale[2];
    matrix[10] = (1.0f - (xx + yy)) * scale[2];
    matrix[11] = 0.0f;

    matrix[12] = position[0];
    matrix[13] = position[1];
    matrix[14] = position[2];
    matrix[15] = 1.0f;
}

void Transform::getForward(float* dir) const {
    // Forward is -Z in our coordinate system
    float x = rotation[0], y = rotation[1], z = rotation[2], w = rotation[3];
    dir[0] = 2.0f * (x * z + w * y);
    dir[1] = 2.0f * (y * z - w * x);
    dir[2] = 1.0f - 2.0f * (x * x + y * y);
    // Negate for forward
    dir[0] = -dir[0];
    dir[1] = -dir[1];
    dir[2] = -dir[2];
}

void Transform::getRight(float* dir) const {
    float x = rotation[0], y = rotation[1], z = rotation[2], w = rotation[3];
    dir[0] = 1.0f - 2.0f * (y * y + z * z);
    dir[1] = 2.0f * (x * y + w * z);
    dir[2] = 2.0f * (x * z - w * y);
}

void Transform::getUp(float* dir) const {
    float x = rotation[0], y = rotation[1], z = rotation[2], w = rotation[3];
    dir[0] = 2.0f * (x * y - w * z);
    dir[1] = 1.0f - 2.0f * (x * x + z * z);
    dir[2] = 2.0f * (y * z + w * x);
}

// Component implementation

EntityId Component::getEntityId() const {
    return m_entity ? m_entity->getId() : InvalidEntityId;
}

// Entity implementation

Entity::Entity(EntityId id, EntityManager* manager)
    : m_id(id), m_manager(manager) {}

Entity::~Entity() {
    // Detach from parent
    if (m_parent) {
        m_parent->removeChild(this);
    }

    // Detach children
    for (Entity* child : m_children) {
        child->m_parent = nullptr;
    }

    // Detach all components
    for (auto& [type, component] : m_components) {
        component->onDetach();
    }
}

void Entity::addTag(const std::string& tag) {
    if (std::find(m_tags.begin(), m_tags.end(), tag) == m_tags.end()) {
        m_tags.push_back(tag);
    }
}

void Entity::removeTag(const std::string& tag) {
    m_tags.erase(std::remove(m_tags.begin(), m_tags.end(), tag), m_tags.end());
}

bool Entity::hasTag(const std::string& tag) const {
    return std::find(m_tags.begin(), m_tags.end(), tag) != m_tags.end();
}

void Entity::addComponentInternal(std::unique_ptr<Component> component) {
    ComponentTypeId typeId = component->getTypeId();
    component->m_entity = this;
    m_components[typeId] = std::move(component);
    m_components[typeId]->onAttach();
}

std::vector<Component*> Entity::getComponents() {
    std::vector<Component*> result;
    for (auto& [type, component] : m_components) {
        result.push_back(component.get());
    }
    return result;
}

void Entity::setParent(Entity* parent) {
    if (m_parent == parent) return;

    if (m_parent) {
        m_parent->removeChild(this);
    }

    m_parent = parent;

    if (m_parent) {
        m_parent->m_children.push_back(this);
    }
}

void Entity::addChild(Entity* child) {
    if (child && child->m_parent != this) {
        child->setParent(this);
    }
}

void Entity::removeChild(Entity* child) {
    if (child && child->m_parent == this) {
        child->m_parent = nullptr;
        m_children.erase(
            std::remove(m_children.begin(), m_children.end(), child),
            m_children.end()
        );
    }
}

void Entity::update(float deltaTime) {
    if (!m_enabled) return;

    for (auto& [type, component] : m_components) {
        if (component->isEnabled()) {
            component->onUpdate(deltaTime);
        }
    }

    // Update children
    for (Entity* child : m_children) {
        child->update(deltaTime);
    }
}

// EntityManager implementation

EntityManager::EntityManager() = default;

EntityManager::~EntityManager() {
    destroyAllEntities();
}

Entity* EntityManager::createEntity(const std::string& name) {
    EntityId id = m_nextId++;
    auto entity = std::make_unique<Entity>(id, this);
    entity->setName(name.empty() ? "Entity_" + std::to_string(id) : name);

    Entity* ptr = entity.get();
    m_entities[id] = std::move(entity);

    return ptr;
}

void EntityManager::destroyEntity(EntityId id) {
    m_pendingDestroy.push_back(id);
}

void EntityManager::destroyEntity(Entity* entity) {
    if (entity) {
        destroyEntity(entity->getId());
    }
}

void EntityManager::destroyAllEntities() {
    m_entities.clear();
    m_pendingDestroy.clear();
}

Entity* EntityManager::getEntity(EntityId id) {
    auto it = m_entities.find(id);
    return (it != m_entities.end()) ? it->second.get() : nullptr;
}

Entity* EntityManager::findByName(const std::string& name) {
    for (auto& [id, entity] : m_entities) {
        if (entity->getName() == name) {
            return entity.get();
        }
    }
    return nullptr;
}

std::vector<Entity*> EntityManager::findByTag(const std::string& tag) {
    std::vector<Entity*> result;
    for (auto& [id, entity] : m_entities) {
        if (entity->hasTag(tag)) {
            result.push_back(entity.get());
        }
    }
    return result;
}

void EntityManager::update(float deltaTime) {
    // Process pending destroys
    processPendingDestroy();

    // Update all root entities (those without parents)
    for (auto& [id, entity] : m_entities) {
        if (!entity->getParent()) {
            entity->update(deltaTime);
        }
    }
}

void EntityManager::processPendingDestroy() {
    for (EntityId id : m_pendingDestroy) {
        m_entities.erase(id);
    }
    m_pendingDestroy.clear();
}

// CameraComponent implementation

void CameraComponent::getProjectionMatrix(float* matrix, float aspect) const {
    std::memset(matrix, 0, 16 * sizeof(float));

    if (orthographic) {
        float halfHeight = orthoSize;
        float halfWidth = halfHeight * aspect;

        matrix[0] = 1.0f / halfWidth;
        matrix[5] = 1.0f / halfHeight;
        matrix[10] = -2.0f / (farPlane - nearPlane);
        matrix[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);
        matrix[15] = 1.0f;
    } else {
        float fovRad = fov * 3.14159265f / 180.0f;
        float tanHalfFov = std::tan(fovRad * 0.5f);

        matrix[0] = 1.0f / (aspect * tanHalfFov);
        matrix[5] = 1.0f / tanHalfFov;
        matrix[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
        matrix[11] = -1.0f;
        matrix[14] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
    }
}

void CameraComponent::getViewMatrix(float* matrix) const {
    if (!m_entity) {
        std::memset(matrix, 0, 16 * sizeof(float));
        matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
        return;
    }

    const Transform& t = m_entity->transform();

    // Get basis vectors
    float forward[3], right[3], up[3];
    t.getForward(forward);
    t.getRight(right);
    t.getUp(up);

    // Build view matrix (inverse of camera transform)
    matrix[0] = right[0];
    matrix[1] = up[0];
    matrix[2] = -forward[0];
    matrix[3] = 0.0f;

    matrix[4] = right[1];
    matrix[5] = up[1];
    matrix[6] = -forward[1];
    matrix[7] = 0.0f;

    matrix[8] = right[2];
    matrix[9] = up[2];
    matrix[10] = -forward[2];
    matrix[11] = 0.0f;

    // Translation
    matrix[12] = -(right[0] * t.position[0] + right[1] * t.position[1] + right[2] * t.position[2]);
    matrix[13] = -(up[0] * t.position[0] + up[1] * t.position[1] + up[2] * t.position[2]);
    matrix[14] = forward[0] * t.position[0] + forward[1] * t.position[1] + forward[2] * t.position[2];
    matrix[15] = 1.0f;
}

// Collect render items from all visible entities with MeshRendererComponent
void collectRenderItems(EntityManager& manager, RenderQueue& queue) {
    auto entities = manager.getEntitiesWithComponent<MeshRendererComponent>();

    for (Entity* entity : entities) {
        if (!entity->isEnabled()) continue;

        auto* mesh = entity->getComponent<MeshRendererComponent>();
        if (!mesh || !mesh->visible || mesh->vertexBuffer == InvalidBuffer) continue;

        // Compute model matrix from entity transform
        float modelMatrix[16];
        entity->transform().getMatrix(modelMatrix);

        if (!mesh->submeshes.empty()) {
            for (const auto& sub : mesh->submeshes) {
                if (sub.indexCount == 0 && sub.vertexCount == 0) continue;

                RenderItem item;
                item.vertexBuffer = mesh->vertexBuffer;
                item.indexBuffer = mesh->indexBuffer;
                item.texture = sub.texture;
                item.indexCount = sub.indexCount;
                item.indexStart = sub.indexStart;
                item.vertexOffset = sub.vertexOffset;
                item.vertexCount = sub.vertexCount;
                item.vertexStart = sub.vertexStart;
                std::memcpy(item.modelMatrix, modelMatrix, sizeof(float) * 16);

                queue.submit(item);
            }
        } else {
            // Single draw for the whole mesh (no submeshes)
            RenderItem item;
            item.vertexBuffer = mesh->vertexBuffer;
            item.indexBuffer = mesh->indexBuffer;
            item.texture = InvalidTexture;
            std::memcpy(item.modelMatrix, modelMatrix, sizeof(float) * 16);

            queue.submit(item);
        }
    }
}

} // namespace opensaints
