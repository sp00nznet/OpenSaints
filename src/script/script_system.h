#pragma once
// Script System for OpenSaints
// Provides Lua scripting and action node execution

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <variant>

namespace opensaints {

// Forward declarations
class ScriptContext;
class ActionNode;
class Entity;

// Script value types
using ScriptValue = std::variant<
    std::monostate,     // nil
    bool,               // boolean
    double,             // number
    std::string,        // string
    void*               // userdata (entity, etc.)
>;

// Script function signature
using ScriptFunction = std::function<ScriptValue(const std::vector<ScriptValue>&)>;

// Script error callback
using ScriptErrorCallback = std::function<void(const std::string& error)>;

// Script variable for scripting contexts
struct ScriptVariable {
    std::string name;
    ScriptValue value;
    bool readonly = false;
};

// Action node types (from Saints Row 2 action system)
enum class ActionNodeType {
    // Flow control
    Sequence,       // Execute children in order
    Selector,       // Execute until one succeeds
    Parallel,       // Execute all simultaneously
    Condition,      // Branch based on condition
    Loop,           // Repeat children
    Wait,           // Wait for duration/condition

    // Actions
    MoveTo,         // Move entity to position
    LookAt,         // Turn to face target
    PlayAnimation,  // Play animation clip
    PlaySound,      // Play audio
    Say,            // Show dialog
    SetVariable,    // Set script variable
    CallFunction,   // Call script function
    TriggerEvent,   // Trigger game event

    // AI-specific
    Patrol,         // Patrol between waypoints
    Chase,          // Chase target
    Attack,         // Attack target
    Flee,           // Flee from target
    Cover,          // Take cover

    // Vehicle
    EnterVehicle,
    ExitVehicle,
    DriveToPosition,
    FollowPath,

    // Custom
    Custom
};

// Action node execution state
enum class ActionState {
    Ready,
    Running,
    Succeeded,
    Failed,
    Cancelled
};

// Base action node
class ActionNode {
public:
    ActionNode(ActionNodeType type = ActionNodeType::Custom);
    virtual ~ActionNode() = default;

    // Node identification
    ActionNodeType type() const { return m_type; }
    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    // Execution
    virtual void start(ScriptContext* context);
    virtual ActionState update(ScriptContext* context, float deltaTime);
    virtual void stop(ScriptContext* context);
    virtual void reset();

    ActionState state() const { return m_state; }
    bool isRunning() const { return m_state == ActionState::Running; }

    // Tree structure
    void addChild(std::unique_ptr<ActionNode> child);
    const std::vector<std::unique_ptr<ActionNode>>& children() const { return m_children; }

    // Properties
    void setProperty(const std::string& name, const ScriptValue& value);
    ScriptValue getProperty(const std::string& name) const;

    // Entity association
    void setEntity(Entity* entity) { m_entity = entity; }
    Entity* entity() const { return m_entity; }

protected:
    ActionNodeType m_type;
    std::string m_name;
    ActionState m_state = ActionState::Ready;
    std::vector<std::unique_ptr<ActionNode>> m_children;
    std::unordered_map<std::string, ScriptValue> m_properties;
    Entity* m_entity = nullptr;
};

// Sequence node - executes children in order
class SequenceNode : public ActionNode {
public:
    SequenceNode() : ActionNode(ActionNodeType::Sequence) {}
    void start(ScriptContext* context) override;
    ActionState update(ScriptContext* context, float deltaTime) override;
    void reset() override;

private:
    size_t m_currentChild = 0;
};

// Selector node - executes until one succeeds
class SelectorNode : public ActionNode {
public:
    SelectorNode() : ActionNode(ActionNodeType::Selector) {}
    void start(ScriptContext* context) override;
    ActionState update(ScriptContext* context, float deltaTime) override;
    void reset() override;

private:
    size_t m_currentChild = 0;
};

// Parallel node - executes all children simultaneously
class ParallelNode : public ActionNode {
public:
    enum class Policy {
        RequireAll,     // Success when all succeed
        RequireOne      // Success when any succeeds
    };

    ParallelNode(Policy policy = Policy::RequireAll)
        : ActionNode(ActionNodeType::Parallel), m_policy(policy) {}
    void start(ScriptContext* context) override;
    ActionState update(ScriptContext* context, float deltaTime) override;
    void reset() override;

private:
    Policy m_policy;
    std::vector<bool> m_childComplete;
};

// Wait node - waits for duration or condition
class WaitNode : public ActionNode {
public:
    WaitNode(float duration = 1.0f)
        : ActionNode(ActionNodeType::Wait), m_duration(duration) {}
    void start(ScriptContext* context) override;
    ActionState update(ScriptContext* context, float deltaTime) override;
    void reset() override;

    void setCondition(std::function<bool()> condition) { m_condition = condition; }

private:
    float m_duration;
    float m_elapsed = 0;
    std::function<bool()> m_condition;
};

// Condition node - branches based on condition
class ConditionNode : public ActionNode {
public:
    ConditionNode() : ActionNode(ActionNodeType::Condition) {}
    void start(ScriptContext* context) override;
    ActionState update(ScriptContext* context, float deltaTime) override;

    void setCondition(std::function<bool()> condition) { m_condition = condition; }

private:
    std::function<bool()> m_condition;
    ActionNode* m_selectedChild = nullptr;
};

// Move to position node
class MoveToNode : public ActionNode {
public:
    MoveToNode() : ActionNode(ActionNodeType::MoveTo) {}
    void start(ScriptContext* context) override;
    ActionState update(ScriptContext* context, float deltaTime) override;

private:
    float m_targetX = 0, m_targetY = 0, m_targetZ = 0;
    float m_speed = 5.0f;
    float m_tolerance = 0.5f;
};

// Play animation node
class PlayAnimationNode : public ActionNode {
public:
    PlayAnimationNode() : ActionNode(ActionNodeType::PlayAnimation) {}
    void start(ScriptContext* context) override;
    ActionState update(ScriptContext* context, float deltaTime) override;

private:
    std::string m_animationName;
    bool m_waitForComplete = true;
};

// Script execution context
class ScriptContext {
public:
    ScriptContext();
    ~ScriptContext();

    // Variable management
    void setVariable(const std::string& name, const ScriptValue& value);
    ScriptValue getVariable(const std::string& name) const;
    bool hasVariable(const std::string& name) const;
    void clearVariables();

    // Function registration
    void registerFunction(const std::string& name, ScriptFunction func);
    ScriptValue callFunction(const std::string& name, const std::vector<ScriptValue>& args);

    // Entity context
    void setEntity(Entity* entity) { m_entity = entity; }
    Entity* entity() const { return m_entity; }

    // Parent context (for nested scopes)
    void setParent(ScriptContext* parent) { m_parent = parent; }
    ScriptContext* parent() const { return m_parent; }

private:
    std::unordered_map<std::string, ScriptValue> m_variables;
    std::unordered_map<std::string, ScriptFunction> m_functions;
    Entity* m_entity = nullptr;
    ScriptContext* m_parent = nullptr;
};

// Action node tree (complete behavior tree)
class ActionTree {
public:
    ActionTree();
    ~ActionTree() = default;

    // Load from file/data
    bool loadFromFile(const std::string& path);
    bool loadFromString(const std::string& data);

    // Root node
    void setRoot(std::unique_ptr<ActionNode> root);
    ActionNode* root() { return m_root.get(); }

    // Execution
    void start(ScriptContext* context);
    ActionState update(ScriptContext* context, float deltaTime);
    void stop(ScriptContext* context);
    void reset();

    // State
    bool isRunning() const { return m_running; }
    ActionState state() const { return m_root ? m_root->state() : ActionState::Ready; }

    // Metadata
    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

private:
    std::string m_name;
    std::unique_ptr<ActionNode> m_root;
    bool m_running = false;
};

// Script System (manages all scripting)
class ScriptSystem {
public:
    ScriptSystem();
    ~ScriptSystem();

    // Initialize/shutdown
    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // Lua integration (when available)
    bool loadScript(const std::string& name, const std::string& code);
    bool loadScriptFile(const std::string& name, const std::string& path);
    bool executeScript(const std::string& name);
    ScriptValue callScriptFunction(const std::string& script, const std::string& function,
                                    const std::vector<ScriptValue>& args);

    // Global functions (exposed to Lua)
    void registerGlobalFunction(const std::string& name, ScriptFunction func);
    ScriptValue callGlobalFunction(const std::string& name, const std::vector<ScriptValue>& args);

    // Global variables
    void setGlobalVariable(const std::string& name, const ScriptValue& value);
    ScriptValue getGlobalVariable(const std::string& name) const;

    // Action trees
    void registerActionTree(const std::string& name, std::shared_ptr<ActionTree> tree);
    std::shared_ptr<ActionTree> getActionTree(const std::string& name) const;
    ActionTree* createActionTreeInstance(const std::string& templateName);

    // Context management
    ScriptContext* createContext();
    void destroyContext(ScriptContext* context);
    ScriptContext* globalContext() { return &m_globalContext; }

    // Update all running scripts
    void update(float deltaTime);

    // Error handling
    void setErrorCallback(ScriptErrorCallback callback) { m_errorCallback = callback; }
    void reportError(const std::string& error);

    // Built-in game functions
    void registerGameAPI();

private:
    bool m_initialized = false;

    // Lua state (optional)
    void* m_luaState = nullptr;  // lua_State*

    // Script storage
    std::unordered_map<std::string, std::string> m_scripts;
    std::unordered_map<std::string, std::shared_ptr<ActionTree>> m_actionTrees;
    std::vector<std::unique_ptr<ActionTree>> m_runningTrees;

    // Global context
    ScriptContext m_globalContext;
    std::vector<std::unique_ptr<ScriptContext>> m_contexts;

    ScriptErrorCallback m_errorCallback;

    // Lua helpers
    bool initLua();
    void shutdownLua();
    bool luaAvailable() const;
};

// Global script system access
ScriptSystem& getScriptSystem();

// Script value helpers
inline bool isNil(const ScriptValue& v) { return std::holds_alternative<std::monostate>(v); }
inline bool isBool(const ScriptValue& v) { return std::holds_alternative<bool>(v); }
inline bool isNumber(const ScriptValue& v) { return std::holds_alternative<double>(v); }
inline bool isString(const ScriptValue& v) { return std::holds_alternative<std::string>(v); }
inline bool isUserdata(const ScriptValue& v) { return std::holds_alternative<void*>(v); }

inline bool toBool(const ScriptValue& v) {
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v);
    if (std::holds_alternative<double>(v)) return std::get<double>(v) != 0;
    if (std::holds_alternative<std::string>(v)) return !std::get<std::string>(v).empty();
    return false;
}

inline double toNumber(const ScriptValue& v) {
    if (std::holds_alternative<double>(v)) return std::get<double>(v);
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1.0 : 0.0;
    if (std::holds_alternative<std::string>(v)) {
        try { return std::stod(std::get<std::string>(v)); }
        catch (...) { return 0; }
    }
    return 0;
}

inline std::string toString(const ScriptValue& v) {
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? "true" : "false";
    if (std::holds_alternative<double>(v)) return std::to_string(std::get<double>(v));
    return "";
}

} // namespace opensaints
