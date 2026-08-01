// Arkheon Technologies
// Proprietary and Confidential.
// Unauthorized copying of this file, via any medium, is strictly prohibited.
// © Arkheon Technologies. All rights reserved.

#pragma once

#include <scripting/IScriptingApiModule.h>

namespace n8ro::sample::simscripting {

// Demonstrates CREATING a brand-new namespace.
//
// There is no registry of allowed namespaces: a namespace is created implicitly the first time a
// function is registered under a new namespace name. This module registers functions under
// "tutorial", which no built-in system owns, so the engine exposes a fresh `tutorial` global table
// to mission/workbook scripts and emits a tutorial.lua stub for editor tooling.
class NewNamespaceApiModule final : public n8ro::sim::IScriptingApiModule {
public:
    [[nodiscard]] const char* moduleId() const override;
    [[nodiscard]] bool registerWith(
        n8ro::sim::MissionRegistrar& registrar,
        const n8ro::sim::ScriptingApiContext& context) override;
};

} // namespace n8ro::sample::simscripting
