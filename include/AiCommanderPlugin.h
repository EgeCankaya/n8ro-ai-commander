#pragma once

#include <plugin/IPlugin.h>

#include "AiCommanderApiModule.h"
#include "CommanderRuntime.h"
#include "OrderValidatorStageB.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace n8ro::core {
class IThreadRunner;
}
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
    ~AiCommanderPlugin() override;

    [[nodiscard]] int getInterfaceVersion() const override;
    [[nodiscard]] n8ro::core::PluginMetadata getMetadata() const override;
    void initialize(n8ro::core::PluginContext& context) override;
    void onTickFrame(double dt) override;
    void onStop() override;
    void shutdown() override;

    [[nodiscard]] std::vector<n8ro::core::PluginConfigField> getConfigFields() const override;
    bool applyConfigFields(const std::vector<n8ro::core::PluginConfigField>& fields) override;

private:
    // Runs the AIC-ARCH-4 probe once entities exist. Deferred out of initialize() because no
    // scenario is loaded there — the entity manager is empty until a scenario spawns.
    void runProbeIfPending();

    // Constructs the adapter `commander.backend` names. Called at initialize() and whenever the
    // backend changes, so switching takes effect without restarting the process (AIC-ARCH-3).
    void rebuildBackend();

    // The per-frame pipeline, in the order the design requires:
    //   drain completed orders -> Stage B -> publish  (newest information first)
    //   advance the fallback ladder
    //   take snapshots and dispatch new requests
    void drainCompletedOrders(double simTimeS, std::int64_t frame);
    void advanceLadders(double simTimeS, std::int64_t frame);
    void dispatchRequests(double simTimeS, std::int64_t frame);

    // Builds a snapshot from live state, or returns false when a required field is unavailable —
    // in which case this entity is skipped for the tick rather than snapshotted with a hole in it.
    [[nodiscard]] bool buildSnapshot(
        const std::string& entityId, double simTimeS, std::int64_t serial, OrderSnapshot& out) const;

    [[nodiscard]] EntityPosition positionOf(const std::string& entityId) const;

    // Writes the frame-cost percentiles and pipeline counters to the log and the order record at
    // run end (Observability).
    void writeRunEndStats();

    CommanderRuntime runtime_;
    AiCommanderApiModule apiModule_;

    // Non-owning, host-supplied, valid for the lifetime of the scripting service. Dereferenced on
    // the update thread only, and never handed to a worker.
    const n8ro::sim::IEntityManager* entityManager_ = nullptr;
    n8ro::core::IThreadRunner* threadRunner_ = nullptr;

    // The Stage-B world view over entityManager_. Owned here so its lifetime matches the plugin's.
    std::unique_ptr<StageBWorldView> world_;

    std::int64_t frameCounter_ = 0;
    double simTimeS_ = 0.0;
    bool scriptingApiRegistered_ = false;
    bool probeAttemptedThisRun_ = false;
    bool shutdown_ = false;
    bool loggedNoThreadRunner_ = false;
};

} // namespace arkheon::aicommander

// C-style factory functions for DLL export.
extern "C" {
    N8RO_CORE_API n8ro::core::IPlugin* create_plugin();
    N8RO_CORE_API void destroy_plugin(n8ro::core::IPlugin* plugin);
    N8RO_CORE_API const char* get_plugin_signature();
}
