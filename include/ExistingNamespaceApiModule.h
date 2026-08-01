// Arkheon Technologies
// Proprietary and Confidential.
// Unauthorized copying of this file, via any medium, is strictly prohibited.
// © Arkheon Technologies. All rights reserved.

#pragma once

#include <scripting/IScriptingApiModule.h>

namespace n8ro::sample::simscripting {

// Demonstrates ADDING a new function to an EXISTING namespace.
//
// n8ro-sim's built-in NavigationSystem registers the `navigation` namespace at engine start; this
// module registers one more function under that same namespace name ("navigation"). Because the
// MissionRegistrar keys on (namespace, function), passing the existing namespace string simply
// appends the function to it — script authors then call navigation.bearingDegrees(...) alongside
// the built-in navigation.* functions.
class ExistingNamespaceApiModule final : public n8ro::sim::IScriptingApiModule {
public:
    [[nodiscard]] const char* moduleId() const override;
    [[nodiscard]] bool registerWith(
        n8ro::sim::MissionRegistrar& registrar,
        const n8ro::sim::ScriptingApiContext& context) override;
};

} // namespace n8ro::sample::simscripting
