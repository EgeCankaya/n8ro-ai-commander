#pragma once

#include <scripting/IScriptingApiModule.h>

#include <string>

namespace arkheon::aicommander {

class CommanderRuntime;

// Registers the `aiCommander` Lua namespace (AIC-API-1).
//
// The namespace is new, not an extension of a built-in one: `navigation`, `weapon`, `sensor`,
// `entityControl`, `mission`, `simulation`, `scenarioControl`, and `animation` are engine
// contracts, and adding to them would make the generated stubs ambiguous about ownership.
//
// Every callback captures the ScriptingApiContext BY VALUE, per the IScriptingApiModule contract:
// the registrar keeps the callback, and the Lua runtime invokes it with only the script arguments,
// so a function that needs engine state must own its context rather than be handed one at call time.
class AiCommanderApiModule final : public n8ro::sim::IScriptingApiModule {
public:
    // `runtime` is owned by the plugin and outlives the registrar. Callbacks capture it as a raw
    // pointer alongside the by-value context; it is only ever touched on the simulation thread,
    // which is where the Lua runtime invokes them.
    explicit AiCommanderApiModule(CommanderRuntime& runtime) : runtime_(&runtime) {}

    [[nodiscard]] const char* moduleId() const override;
    [[nodiscard]] bool registerWith(
        n8ro::sim::MissionRegistrar& registrar,
        const n8ro::sim::ScriptingApiContext& context) override;

private:
    CommanderRuntime* runtime_ = nullptr;
};

} // namespace arkheon::aicommander
