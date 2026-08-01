// Arkheon Technologies
// Proprietary and Confidential.
// Unauthorized copying of this file, via any medium, is strictly prohibited.
// © Arkheon Technologies. All rights reserved.

#pragma once

#include <plugin/IPlugin.h>

#include "ExistingNamespaceApiModule.h"
#include "NewNamespaceApiModule.h"
#include "EntityStateApiModule.h"

#include <string>

namespace n8ro::sample::simscripting {

// Sample n8ro-sim plugin that injects mission-scripting functions into the running engine.
//
// On initialize() it resolves the host-provided IScriptingPluginService through
// PluginContext.services, then hands its three IScriptingApiModule members the host MissionRegistrar
// and scripting context so they can register their functions — one into the existing `navigation`
// namespace, one into a brand-new `tutorial` namespace, and one into an `entityState` namespace that
// reads live entity state through the captured context.
class SimScriptingPlugin final : public n8ro::core::IPlugin {
public:
    SimScriptingPlugin() = default;
    ~SimScriptingPlugin() override = default;

    [[nodiscard]] int getInterfaceVersion() const override;
    [[nodiscard]] n8ro::core::PluginMetadata getMetadata() const override;
    void initialize(n8ro::core::PluginContext& context) override;
    void onTickFrame(double dt) override;
    void shutdown() override;

private:
    ExistingNamespaceApiModule existingNamespaceModule_;
    NewNamespaceApiModule newNamespaceModule_;
    EntityStateApiModule entityStateModule_;
    bool initialized_ = false;
    bool shutdown_ = false;
    bool scriptingApiRegistered_ = false;
    std::string loadedPluginId_;
};

} // namespace n8ro::sample::simscripting

// C-style factory functions for DLL export
extern "C" {
    N8RO_CORE_API n8ro::core::IPlugin* create_plugin();
    N8RO_CORE_API void destroy_plugin(n8ro::core::IPlugin* plugin);
    N8RO_CORE_API const char* get_plugin_signature();
}
