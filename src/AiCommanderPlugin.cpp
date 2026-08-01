#include "AiCommanderPlugin.h"

#include <core/logging/GlobalLogger.h>
#include <entity/IEntityManager.h>
#include <plugin/IPluginServices.h>
#include <plugin/IScriptingPluginService.h>
#include <plugin/PluginContext.h>
#include <scripting/ScriptingApiContext.h>

#include <string>
#include <string_view>

namespace arkheon::aicommander {

namespace {

constexpr std::string_view kLogCategory = "arkheon.aiCommander";
constexpr std::string_view kPluginId = "ai-commander";

[[nodiscard]] const char* yesNo(bool value) {
    return value ? "non-null" : "NULL";
}

} // namespace

int AiCommanderPlugin::getInterfaceVersion() const {
    return 1;
}

n8ro::core::PluginMetadata AiCommanderPlugin::getMetadata() const {
    n8ro::core::PluginMetadata metadata;
    metadata.setPluginId(std::string(kPluginId));
    metadata.setVersion("0.1.0");
    metadata.setAuthor("Arkheon Technologies");
    return metadata;
}

void AiCommanderPlugin::initialize(n8ro::core::PluginContext& context) {
    shutdown_ = false;
    scriptingApiRegistered_ = false;
    probeAttemptedThisRun_ = false;

    // OQ-7: PluginContext documents both `services` and `threadRunner` as nullable, and nobody has
    // observed what the host actually passes to a sim plugin. Log both on every load — this is the
    // whole evidence base for whether the worker can use IThreadRunner::submitBackgroundTask or
    // must own a std::thread. The fallback is specified either way, so this is a simplification
    // question rather than a blocker; the log line is what closes it.
    {
        const std::string line = std::string("ai-commander: [OQ-7] PluginContext.services is ")
            + yesNo(context.services != nullptr) + ", PluginContext.threadRunner is "
            + yesNo(context.threadRunner != nullptr) + ".";
        N8RO_LOG_INFO(line, kLogCategory);
    }

    if (context.services == nullptr) {
        N8RO_LOG_WARNING(
            "ai-commander: plugin services unavailable; aiCommander namespace not registered.",
            kLogCategory);
        return;
    }

    void* rawService =
        context.services->getService(n8ro::sim::IScriptingPluginService::kPluginServiceId);
    if (rawService == nullptr) {
        N8RO_LOG_WARNING(
            "ai-commander: scripting plugin service not provided by host; "
            "aiCommander namespace not registered.",
            kLogCategory);
        return;
    }

    auto* scriptingService = static_cast<n8ro::sim::IScriptingPluginService*>(rawService);
    const n8ro::sim::ScriptingApiContext& scriptingContext = scriptingService->scriptingContext();

    // Held for the AIC-ARCH-4 probe and, later, for building snapshots on the update thread.
    // Never handed to a worker: IEntityManager is single-thread-only, and the worker's callable
    // captures only a POD snapshot and the ILlmClient (AIC-ARCH-2).
    entityManager_ = scriptingContext.entityManager;

    scriptingApiRegistered_ =
        apiModule_.registerWith(scriptingService->missionRegistrar(), scriptingContext);

    if (scriptingApiRegistered_) {
        const CommanderConfig& config = runtime_.config();
        const std::string line = std::string("ai-commander: registered the aiCommander namespace. ")
            + "backend=" + toString(config.backend)
            + " enabled=" + (config.enabled ? "true" : "false")
            + " cadenceS=" + std::to_string(static_cast<int>(config.cadenceS))
            + " maxCommandedEntities=" + std::to_string(config.maxCommandedEntities)
            + " recordPath=" + config.recordPath;
        N8RO_LOG_INFO(line, kLogCategory);
    } else {
        N8RO_LOG_WARNING(
            "ai-commander: one or more aiCommander functions failed to register.", kLogCategory);
    }
}

void AiCommanderPlugin::runProbeIfPending() {
    if (probeAttemptedThisRun_ || entityManager_ == nullptr) {
        return;
    }

    ProbeReport report = probeRuntimeColumns(*entityManager_);
    if (report.result == ProbeResult::NotRun) {
        // No entity with a transform yet. Stay pending and retry on a later frame rather than
        // recording a verdict the scenario has not had a chance to earn.
        return;
    }

    probeAttemptedThisRun_ = true;

    // OQ-9 evidence, logged whatever the verdict: whether readComponentFieldReal can address a
    // runtime column at all, whether the schema's slash form also works, and — the load-bearing
    // one — whether a path that is not a column returns a value (silent) or nullopt (loud).
    {
        const std::string line = std::string("ai-commander: [OQ-9] runtime-column probe on '")
            + report.probedEntityId + "': dot-path 'velocityNed.x' "
            + (report.dotPathResolved ? "RESOLVED" : "nullopt")
            + ", slash-path 'velocityNed/x' "
            + (report.slashPathResolved ? "RESOLVED" : "nullopt")
            + ", bogus-path 'velocityNed.q' "
            + (report.bogusPathResolved ? "RESOLVED (bad paths are SILENT)"
                                        : "nullopt (bad paths are LOUD)")
            + ".";
        N8RO_LOG_INFO(line, kLogCategory);
    }

    const std::string verdict = std::string("ai-commander: runtime-column probe ")
        + toString(report.result) + " - " + report.detail;
    if (report.result == ProbeResult::Pass) {
        N8RO_LOG_INFO(verdict, kLogCategory);
    } else {
        // A failed probe disables the commander. It does NOT fall back to a zero velocity: a
        // fabricated stationary own-ship would degrade every order downstream with no failing test
        // and no log line, which is the exact failure mode AIC-ARCH-4 exists to prevent.
        N8RO_LOG_ERROR(verdict, kLogCategory);
        N8RO_LOG_ERROR(
            "ai-commander: commander will not issue orders while the runtime-column probe is "
            "failing.",
            kLogCategory);
    }

    runtime_.setProbeReport(std::move(report));
}

void AiCommanderPlugin::onTickFrame(double dt) {
    (void)dt;
    if (shutdown_) {
        return;
    }
    runProbeIfPending();
}

void AiCommanderPlugin::onStop() {
    // A new scenario brings a new entity set, so the probe verdict from the previous run does not
    // carry over — re-earn it rather than trusting a stale pass.
    probeAttemptedThisRun_ = false;
    runtime_.setProbeReport(ProbeReport{});
}

void AiCommanderPlugin::shutdown() {
    shutdown_ = true;
    entityManager_ = nullptr;
}

std::vector<n8ro::core::PluginConfigField> AiCommanderPlugin::getConfigFields() const {
    return toConfigFields(runtime_.config());
}

bool AiCommanderPlugin::applyConfigFields(
    const std::vector<n8ro::core::PluginConfigField>& fields) {
    CommanderConfig parsed;
    std::string error;
    if (!tryParseConfigFields(fields, runtime_.config(), parsed, error)) {
        // All-or-nothing: nothing has been written, so every prior value still stands. The reason
        // is logged because a config rejection the operator cannot diagnose is indistinguishable
        // from the plugin ignoring them.
        N8RO_LOG_WARNING(
            std::string("ai-commander: rejected config - ") + error
                + ". No field was applied; previous configuration retained.",
            kLogCategory);
        return false;
    }

    const bool wasEnabled = runtime_.config().enabled;
    runtime_.setConfig(parsed);

    if (parsed.enabled != wasEnabled) {
        N8RO_LOG_INFO(
            std::string("ai-commander: commander.enabled -> ") + (parsed.enabled ? "true" : "false")
                + " (backend=" + toString(parsed.backend) + ").",
            kLogCategory);
    }
    return true;
}

} // namespace arkheon::aicommander

extern "C" {

N8RO_CORE_API n8ro::core::IPlugin* create_plugin() {
    return new arkheon::aicommander::AiCommanderPlugin();
}

N8RO_CORE_API void destroy_plugin(n8ro::core::IPlugin* plugin) {
    delete plugin;
}

N8RO_CORE_API const char* get_plugin_signature() {
    return "N8RO_PLUGIN_V1";
}

} // extern "C"
