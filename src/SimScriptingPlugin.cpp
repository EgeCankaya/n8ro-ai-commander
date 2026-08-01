// Arkheon Technologies
// Proprietary and Confidential.
// Unauthorized copying of this file, via any medium, is strictly prohibited.
// © Arkheon Technologies. All rights reserved.

#include "SimScriptingPlugin.h"

#include <core/logging/GlobalLogger.h>
#include <plugin/IPluginServices.h>
#include <plugin/IScriptingPluginService.h>
#include <plugin/PluginContext.h>

#include <string_view>

namespace n8ro::sample::simscripting {

namespace {
constexpr std::string_view kLogCategory = "sample.simScripting";
}

int SimScriptingPlugin::getInterfaceVersion() const {
    return 1;
}

n8ro::core::PluginMetadata SimScriptingPlugin::getMetadata() const {
    n8ro::core::PluginMetadata metadata;
    metadata.setPluginId("ai-commander");
    metadata.setVersion("1.0.0");
    metadata.setAuthor("N8RO Sample");
    return metadata;
}

void SimScriptingPlugin::initialize(n8ro::core::PluginContext& context) {
    initialized_ = true;
    shutdown_ = false;
    scriptingApiRegistered_ = false;
    loadedPluginId_ = context.metadata.pluginId();
    if (loadedPluginId_.empty()) {
        loadedPluginId_ = "ai-commander";
    }

    if (!context.services) {
        N8RO_LOG_WARNING(
            "ai-commander: plugin services unavailable; scripting API not registered.",
            kLogCategory);
        return;
    }

    // Resolve the host-owned scripting registration surface. The engine registers it under
    // IScriptingPluginService::kPluginServiceId during plugin loading.
    void* rawService = context.services->getService(n8ro::sim::IScriptingPluginService::kPluginServiceId);
    if (!rawService) {
        N8RO_LOG_WARNING(
            "ai-commander: scripting plugin service not provided by host; scripting API not registered.",
            kLogCategory);
        return;
    }
    auto* scriptingService = static_cast<n8ro::sim::IScriptingPluginService*>(rawService);

    // Delegate to each module: one extends an existing namespace, one creates a new namespace,
    // one reads live entity state through the captured context.
    const bool existingOk = existingNamespaceModule_.registerWith(
        scriptingService->missionRegistrar(),
        scriptingService->scriptingContext());
    const bool newOk = newNamespaceModule_.registerWith(
        scriptingService->missionRegistrar(),
        scriptingService->scriptingContext());
    const bool stateOk = entityStateModule_.registerWith(
        scriptingService->missionRegistrar(),
        scriptingService->scriptingContext());

    scriptingApiRegistered_ = existingOk && newOk && stateOk;
    if (scriptingApiRegistered_) {
        N8RO_LOG_INFO(
            "ai-commander: registered navigation.bearingDegrees, tutorial.add / tutorial.greet, "
            "and entityState.getAltitude / entityState.rangeMeters.",
            kLogCategory);
    } else {
        N8RO_LOG_WARNING(
            "ai-commander: one or more scripting functions failed to register.",
            kLogCategory);
    }
}

void SimScriptingPlugin::onTickFrame(double dt) {
    // Registered functions are invoked by the engine's Lua runtime when mission scripts call them,
    // not from the plugin frame callback. Nothing to do per frame.
    (void)dt;
}

void SimScriptingPlugin::shutdown() {
    shutdown_ = true;
}

} // namespace n8ro::sample::simscripting

extern "C" {

N8RO_CORE_API n8ro::core::IPlugin* create_plugin() {
    return new n8ro::sample::simscripting::SimScriptingPlugin();
}

N8RO_CORE_API void destroy_plugin(n8ro::core::IPlugin* plugin) {
    if (plugin) {
        delete plugin;
    }
}

N8RO_CORE_API const char* get_plugin_signature() {
    return "N8RO_PLUGIN_V1";
}

} // extern "C"
