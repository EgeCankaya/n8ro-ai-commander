#pragma once

#include <plugin/IPlugin.h>

#include "AiCommanderApiModule.h"
#include "CommanderRuntime.h"

#include <string>
#include <vector>

namespace n8ro::sim {
class IEntityManager;
}

namespace arkheon::aicommander {

// The N8RO AI Entity Commander plugin.
//
// A Tier-2 commander: it observes a bounded snapshot of an entity's tactical situation, asks a
// language model for a structured order, validates that order hard, and publishes it where the
// entity's existing deterministic Lua behaviour can read it. It never moves an entity, never
// writes component state, and never blocks the simulation update thread (AIC-ARCH-1/2).
class AiCommanderPlugin final : public n8ro::core::IPlugin {
public:
    AiCommanderPlugin() : apiModule_(runtime_) {}
    ~AiCommanderPlugin() override = default;

    [[nodiscard]] int getInterfaceVersion() const override;
    [[nodiscard]] n8ro::core::PluginMetadata getMetadata() const override;
    void initialize(n8ro::core::PluginContext& context) override;
    void onTickFrame(double dt) override;
    void onStop() override;
    void shutdown() override;

    [[nodiscard]] std::vector<n8ro::core::PluginConfigField> getConfigFields() const override;
    bool applyConfigFields(const std::vector<n8ro::core::PluginConfigField>& fields) override;

private:
    // Runs the AIC-ARCH-4 probe once entities exist, and logs the verdict. Deferred out of
    // initialize() because no scenario is loaded there — the entity manager is empty until a
    // scenario spawns, so a probe at initialize() would always report "nothing to probe".
    void runProbeIfPending();

    CommanderRuntime runtime_;
    AiCommanderApiModule apiModule_;

    // Non-owning; supplied by the host and valid for the lifetime of the scripting service. Only
    // ever dereferenced on the update thread.
    const n8ro::sim::IEntityManager* entityManager_ = nullptr;

    bool scriptingApiRegistered_ = false;
    bool probeAttemptedThisRun_ = false;
    bool shutdown_ = false;
};

} // namespace arkheon::aicommander

// C-style factory functions for DLL export.
extern "C" {
    N8RO_CORE_API n8ro::core::IPlugin* create_plugin();
    N8RO_CORE_API void destroy_plugin(n8ro::core::IPlugin* plugin);
    N8RO_CORE_API const char* get_plugin_signature();
}
