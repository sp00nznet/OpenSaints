#include "script_system.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>

// Conditional Lua support
#ifdef HAVE_LUA
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#endif

namespace opensaints {

// ActionNode implementation

ActionNode::ActionNode(ActionNodeType type) : m_type(type) {}

void ActionNode::start(ScriptContext* context) {
    m_state = ActionState::Running;
}

ActionState ActionNode::update(ScriptContext* context, float deltaTime) {
    return m_state;
}

void ActionNode::stop(ScriptContext* context) {
    if (m_state == ActionState::Running) {
        m_state = ActionState::Cancelled;
    }
    for (auto& child : m_children) {
        child->stop(context);
    }
}

void ActionNode::reset() {
    m_state = ActionState::Ready;
    for (auto& child : m_children) {
        child->reset();
    }
}

void ActionNode::addChild(std::unique_ptr<ActionNode> child) {
    child->setEntity(m_entity);
    m_children.push_back(std::move(child));
}

void ActionNode::setProperty(const std::string& name, const ScriptValue& value) {
    m_properties[name] = value;
}

ScriptValue ActionNode::getProperty(const std::string& name) const {
    auto it = m_properties.find(name);
    return (it != m_properties.end()) ? it->second : ScriptValue{};
}

// SequenceNode implementation

void SequenceNode::start(ScriptContext* context) {
    ActionNode::start(context);
    m_currentChild = 0;
    if (!m_children.empty()) {
        m_children[0]->start(context);
    }
}

ActionState SequenceNode::update(ScriptContext* context, float deltaTime) {
    if (m_children.empty()) {
        m_state = ActionState::Succeeded;
        return m_state;
    }

    while (m_currentChild < m_children.size()) {
        auto& child = m_children[m_currentChild];
        ActionState childState = child->update(context, deltaTime);

        if (childState == ActionState::Running) {
            return ActionState::Running;
        }

        if (childState == ActionState::Failed) {
            m_state = ActionState::Failed;
            return m_state;
        }

        // Child succeeded, move to next
        m_currentChild++;
        if (m_currentChild < m_children.size()) {
            m_children[m_currentChild]->start(context);
        }
    }

    m_state = ActionState::Succeeded;
    return m_state;
}

void SequenceNode::reset() {
    ActionNode::reset();
    m_currentChild = 0;
}

// SelectorNode implementation

void SelectorNode::start(ScriptContext* context) {
    ActionNode::start(context);
    m_currentChild = 0;
    if (!m_children.empty()) {
        m_children[0]->start(context);
    }
}

ActionState SelectorNode::update(ScriptContext* context, float deltaTime) {
    if (m_children.empty()) {
        m_state = ActionState::Failed;
        return m_state;
    }

    while (m_currentChild < m_children.size()) {
        auto& child = m_children[m_currentChild];
        ActionState childState = child->update(context, deltaTime);

        if (childState == ActionState::Running) {
            return ActionState::Running;
        }

        if (childState == ActionState::Succeeded) {
            m_state = ActionState::Succeeded;
            return m_state;
        }

        // Child failed, try next
        m_currentChild++;
        if (m_currentChild < m_children.size()) {
            m_children[m_currentChild]->start(context);
        }
    }

    m_state = ActionState::Failed;
    return m_state;
}

void SelectorNode::reset() {
    ActionNode::reset();
    m_currentChild = 0;
}

// ParallelNode implementation

void ParallelNode::start(ScriptContext* context) {
    ActionNode::start(context);
    m_childComplete.assign(m_children.size(), false);
    for (auto& child : m_children) {
        child->start(context);
    }
}

ActionState ParallelNode::update(ScriptContext* context, float deltaTime) {
    if (m_children.empty()) {
        m_state = ActionState::Succeeded;
        return m_state;
    }

    int succeededCount = 0;
    int failedCount = 0;
    int runningCount = 0;

    for (size_t i = 0; i < m_children.size(); ++i) {
        if (m_childComplete[i]) {
            succeededCount++;
            continue;
        }

        auto& child = m_children[i];
        ActionState childState = child->update(context, deltaTime);

        switch (childState) {
            case ActionState::Succeeded:
                m_childComplete[i] = true;
                succeededCount++;
                if (m_policy == Policy::RequireOne) {
                    m_state = ActionState::Succeeded;
                    return m_state;
                }
                break;
            case ActionState::Failed:
                failedCount++;
                if (m_policy == Policy::RequireAll) {
                    m_state = ActionState::Failed;
                    return m_state;
                }
                break;
            case ActionState::Running:
                runningCount++;
                break;
            default:
                break;
        }
    }

    if (runningCount > 0) {
        return ActionState::Running;
    }

    if (m_policy == Policy::RequireAll) {
        m_state = (failedCount == 0) ? ActionState::Succeeded : ActionState::Failed;
    } else {
        m_state = (succeededCount > 0) ? ActionState::Succeeded : ActionState::Failed;
    }

    return m_state;
}

void ParallelNode::reset() {
    ActionNode::reset();
    m_childComplete.clear();
}

// WaitNode implementation

void WaitNode::start(ScriptContext* context) {
    ActionNode::start(context);
    m_elapsed = 0;
}

ActionState WaitNode::update(ScriptContext* context, float deltaTime) {
    m_elapsed += deltaTime;

    // Check condition if set
    if (m_condition && m_condition()) {
        m_state = ActionState::Succeeded;
        return m_state;
    }

    // Check duration
    if (m_elapsed >= m_duration) {
        m_state = ActionState::Succeeded;
        return m_state;
    }

    return ActionState::Running;
}

void WaitNode::reset() {
    ActionNode::reset();
    m_elapsed = 0;
}

// ConditionNode implementation

void ConditionNode::start(ScriptContext* context) {
    ActionNode::start(context);

    // Evaluate condition to select child
    m_selectedChild = nullptr;
    if (m_children.size() >= 2) {
        bool result = m_condition ? m_condition() : true;
        m_selectedChild = m_children[result ? 0 : 1].get();
    } else if (m_children.size() == 1) {
        bool result = m_condition ? m_condition() : true;
        if (result) {
            m_selectedChild = m_children[0].get();
        }
    }

    if (m_selectedChild) {
        m_selectedChild->start(context);
    }
}

ActionState ConditionNode::update(ScriptContext* context, float deltaTime) {
    if (m_selectedChild) {
        m_state = m_selectedChild->update(context, deltaTime);
    } else {
        m_state = ActionState::Succeeded;  // No child to run
    }
    return m_state;
}

// MoveToNode implementation

void MoveToNode::start(ScriptContext* context) {
    ActionNode::start(context);

    // Get target from properties
    if (auto x = getProperty("target_x"); isNumber(x)) m_targetX = static_cast<float>(toNumber(x));
    if (auto y = getProperty("target_y"); isNumber(y)) m_targetY = static_cast<float>(toNumber(y));
    if (auto z = getProperty("target_z"); isNumber(z)) m_targetZ = static_cast<float>(toNumber(z));
    if (auto s = getProperty("speed"); isNumber(s)) m_speed = static_cast<float>(toNumber(s));
    if (auto t = getProperty("tolerance"); isNumber(t)) m_tolerance = static_cast<float>(toNumber(t));
}

ActionState MoveToNode::update(ScriptContext* context, float deltaTime) {
    // This would integrate with the entity/physics system
    // For now, just succeed immediately
    m_state = ActionState::Succeeded;
    return m_state;
}

// PlayAnimationNode implementation

void PlayAnimationNode::start(ScriptContext* context) {
    ActionNode::start(context);

    if (auto name = getProperty("animation"); isString(name)) {
        m_animationName = toString(name);
    }
    if (auto wait = getProperty("wait"); isBool(wait)) {
        m_waitForComplete = toBool(wait);
    }

    // This would trigger the animation on the entity
    // For now, just succeed immediately if not waiting
    if (!m_waitForComplete) {
        m_state = ActionState::Succeeded;
    }
}

ActionState PlayAnimationNode::update(ScriptContext* context, float deltaTime) {
    // This would check if the animation has finished
    // For now, just succeed
    m_state = ActionState::Succeeded;
    return m_state;
}

// ScriptContext implementation

ScriptContext::ScriptContext() = default;

ScriptContext::~ScriptContext() = default;

void ScriptContext::setVariable(const std::string& name, const ScriptValue& value) {
    m_variables[name] = value;
}

ScriptValue ScriptContext::getVariable(const std::string& name) const {
    auto it = m_variables.find(name);
    if (it != m_variables.end()) {
        return it->second;
    }
    // Check parent context
    if (m_parent) {
        return m_parent->getVariable(name);
    }
    return ScriptValue{};
}

bool ScriptContext::hasVariable(const std::string& name) const {
    if (m_variables.find(name) != m_variables.end()) {
        return true;
    }
    if (m_parent) {
        return m_parent->hasVariable(name);
    }
    return false;
}

void ScriptContext::clearVariables() {
    m_variables.clear();
}

void ScriptContext::registerFunction(const std::string& name, ScriptFunction func) {
    m_functions[name] = func;
}

ScriptValue ScriptContext::callFunction(const std::string& name, const std::vector<ScriptValue>& args) {
    auto it = m_functions.find(name);
    if (it != m_functions.end()) {
        return it->second(args);
    }
    if (m_parent) {
        return m_parent->callFunction(name, args);
    }
    return ScriptValue{};
}

// ActionTree implementation

ActionTree::ActionTree() = default;

bool ActionTree::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open action tree: " << path << "\n";
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadFromString(buffer.str());
}

bool ActionTree::loadFromString(const std::string& data) {
    // Placeholder - would parse action tree format
    // For now, just create an empty root
    m_root = std::make_unique<SequenceNode>();
    m_root->setName("root");
    return true;
}

void ActionTree::setRoot(std::unique_ptr<ActionNode> root) {
    m_root = std::move(root);
}

void ActionTree::start(ScriptContext* context) {
    if (m_root) {
        m_root->start(context);
        m_running = true;
    }
}

ActionState ActionTree::update(ScriptContext* context, float deltaTime) {
    if (!m_root || !m_running) {
        return ActionState::Ready;
    }

    ActionState state = m_root->update(context, deltaTime);
    if (state != ActionState::Running) {
        m_running = false;
    }
    return state;
}

void ActionTree::stop(ScriptContext* context) {
    if (m_root) {
        m_root->stop(context);
    }
    m_running = false;
}

void ActionTree::reset() {
    if (m_root) {
        m_root->reset();
    }
    m_running = false;
}

// ScriptSystem implementation

ScriptSystem::ScriptSystem() = default;

ScriptSystem::~ScriptSystem() {
    shutdown();
}

bool ScriptSystem::initialize() {
    if (m_initialized) return true;

    // Try to initialize Lua
    initLua();

    // Register built-in game API
    registerGameAPI();

    m_initialized = true;
    std::cout << "Script system initialized";
    if (luaAvailable()) {
        std::cout << " (Lua enabled)";
    }
    std::cout << "\n";

    return true;
}

void ScriptSystem::shutdown() {
    m_scripts.clear();
    m_actionTrees.clear();
    m_runningTrees.clear();
    m_contexts.clear();
    shutdownLua();
    m_initialized = false;
}

bool ScriptSystem::initLua() {
#ifdef HAVE_LUA
    m_luaState = luaL_newstate();
    if (m_luaState) {
        luaL_openlibs(static_cast<lua_State*>(m_luaState));
        return true;
    }
#endif
    return false;
}

void ScriptSystem::shutdownLua() {
#ifdef HAVE_LUA
    if (m_luaState) {
        lua_close(static_cast<lua_State*>(m_luaState));
        m_luaState = nullptr;
    }
#endif
}

bool ScriptSystem::luaAvailable() const {
#ifdef HAVE_LUA
    return m_luaState != nullptr;
#else
    return false;
#endif
}

bool ScriptSystem::loadScript(const std::string& name, const std::string& code) {
    m_scripts[name] = code;

#ifdef HAVE_LUA
    if (m_luaState) {
        lua_State* L = static_cast<lua_State*>(m_luaState);
        if (luaL_loadstring(L, code.c_str()) != 0) {
            std::string error = lua_tostring(L, -1);
            lua_pop(L, 1);
            reportError("Failed to load script '" + name + "': " + error);
            return false;
        }
        lua_setglobal(L, name.c_str());
        return true;
    }
#endif

    return true;  // Script stored for later
}

bool ScriptSystem::loadScriptFile(const std::string& name, const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        reportError("Failed to open script file: " + path);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadScript(name, buffer.str());
}

bool ScriptSystem::executeScript(const std::string& name) {
#ifdef HAVE_LUA
    if (m_luaState) {
        lua_State* L = static_cast<lua_State*>(m_luaState);

        auto it = m_scripts.find(name);
        if (it == m_scripts.end()) {
            reportError("Script not found: " + name);
            return false;
        }

        if (luaL_dostring(L, it->second.c_str()) != 0) {
            std::string error = lua_tostring(L, -1);
            lua_pop(L, 1);
            reportError("Script error in '" + name + "': " + error);
            return false;
        }
        return true;
    }
#endif

    // No Lua, just report that scripts aren't executable
    std::cout << "Script execution not available (Lua not linked)\n";
    return false;
}

ScriptValue ScriptSystem::callScriptFunction(const std::string& script, const std::string& function,
                                              const std::vector<ScriptValue>& args) {
#ifdef HAVE_LUA
    if (m_luaState) {
        lua_State* L = static_cast<lua_State*>(m_luaState);

        lua_getglobal(L, function.c_str());
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            return ScriptValue{};
        }

        // Push arguments
        for (const auto& arg : args) {
            if (std::holds_alternative<bool>(arg)) {
                lua_pushboolean(L, std::get<bool>(arg));
            } else if (std::holds_alternative<double>(arg)) {
                lua_pushnumber(L, std::get<double>(arg));
            } else if (std::holds_alternative<std::string>(arg)) {
                lua_pushstring(L, std::get<std::string>(arg).c_str());
            } else {
                lua_pushnil(L);
            }
        }

        // Call function
        if (lua_pcall(L, static_cast<int>(args.size()), 1, 0) != 0) {
            std::string error = lua_tostring(L, -1);
            lua_pop(L, 1);
            reportError("Script function error: " + error);
            return ScriptValue{};
        }

        // Get return value
        ScriptValue result;
        if (lua_isboolean(L, -1)) {
            result = static_cast<bool>(lua_toboolean(L, -1));
        } else if (lua_isnumber(L, -1)) {
            result = lua_tonumber(L, -1);
        } else if (lua_isstring(L, -1)) {
            result = std::string(lua_tostring(L, -1));
        }
        lua_pop(L, 1);
        return result;
    }
#endif

    return ScriptValue{};
}

void ScriptSystem::registerGlobalFunction(const std::string& name, ScriptFunction func) {
    m_globalContext.registerFunction(name, func);

#ifdef HAVE_LUA
    // Would also register in Lua state
#endif
}

ScriptValue ScriptSystem::callGlobalFunction(const std::string& name, const std::vector<ScriptValue>& args) {
    return m_globalContext.callFunction(name, args);
}

void ScriptSystem::setGlobalVariable(const std::string& name, const ScriptValue& value) {
    m_globalContext.setVariable(name, value);

#ifdef HAVE_LUA
    if (m_luaState) {
        lua_State* L = static_cast<lua_State*>(m_luaState);

        if (std::holds_alternative<bool>(value)) {
            lua_pushboolean(L, std::get<bool>(value));
        } else if (std::holds_alternative<double>(value)) {
            lua_pushnumber(L, std::get<double>(value));
        } else if (std::holds_alternative<std::string>(value)) {
            lua_pushstring(L, std::get<std::string>(value).c_str());
        } else {
            lua_pushnil(L);
        }
        lua_setglobal(L, name.c_str());
    }
#endif
}

ScriptValue ScriptSystem::getGlobalVariable(const std::string& name) const {
    return m_globalContext.getVariable(name);
}

void ScriptSystem::registerActionTree(const std::string& name, std::shared_ptr<ActionTree> tree) {
    m_actionTrees[name] = tree;
}

std::shared_ptr<ActionTree> ScriptSystem::getActionTree(const std::string& name) const {
    auto it = m_actionTrees.find(name);
    return (it != m_actionTrees.end()) ? it->second : nullptr;
}

ActionTree* ScriptSystem::createActionTreeInstance(const std::string& templateName) {
    // For now, just create a new tree - would clone from template
    auto tree = std::make_unique<ActionTree>();
    tree->setName(templateName);
    m_runningTrees.push_back(std::move(tree));
    return m_runningTrees.back().get();
}

ScriptContext* ScriptSystem::createContext() {
    auto context = std::make_unique<ScriptContext>();
    context->setParent(&m_globalContext);
    ScriptContext* ptr = context.get();
    m_contexts.push_back(std::move(context));
    return ptr;
}

void ScriptSystem::destroyContext(ScriptContext* context) {
    m_contexts.erase(
        std::remove_if(m_contexts.begin(), m_contexts.end(),
            [context](const auto& ptr) { return ptr.get() == context; }),
        m_contexts.end()
    );
}

void ScriptSystem::update(float deltaTime) {
    // Update running action trees
    for (auto it = m_runningTrees.begin(); it != m_runningTrees.end(); ) {
        auto& tree = *it;
        if (tree->isRunning()) {
            tree->update(&m_globalContext, deltaTime);
        }
        if (!tree->isRunning()) {
            it = m_runningTrees.erase(it);
        } else {
            ++it;
        }
    }
}

void ScriptSystem::reportError(const std::string& error) {
    std::cerr << "[Script] " << error << "\n";
    if (m_errorCallback) {
        m_errorCallback(error);
    }
}

void ScriptSystem::registerGameAPI() {
    // Register built-in functions for game interaction

    registerGlobalFunction("print", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        for (const auto& arg : args) {
            std::cout << toString(arg);
        }
        std::cout << "\n";
        return ScriptValue{};
    });

    registerGlobalFunction("wait", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        // Would integrate with coroutines/async
        return ScriptValue{};
    });

    registerGlobalFunction("random", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        double min = 0, max = 1;
        if (args.size() >= 1) min = toNumber(args[0]);
        if (args.size() >= 2) max = toNumber(args[1]);
        return min + (static_cast<double>(rand()) / RAND_MAX) * (max - min);
    });

    registerGlobalFunction("abs", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.empty()) return 0.0;
        return std::abs(toNumber(args[0]));
    });

    registerGlobalFunction("floor", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.empty()) return 0.0;
        return std::floor(toNumber(args[0]));
    });

    registerGlobalFunction("ceil", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.empty()) return 0.0;
        return std::ceil(toNumber(args[0]));
    });

    registerGlobalFunction("sqrt", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.empty()) return 0.0;
        return std::sqrt(toNumber(args[0]));
    });

    registerGlobalFunction("sin", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.empty()) return 0.0;
        return std::sin(toNumber(args[0]));
    });

    registerGlobalFunction("cos", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.empty()) return 0.0;
        return std::cos(toNumber(args[0]));
    });

    // Game-specific functions would go here
    // spawn_entity, destroy_entity, play_sound, set_mission_state, etc.
}

// Global script system singleton
ScriptSystem& getScriptSystem() {
    static ScriptSystem instance;
    return instance;
}

} // namespace opensaints
