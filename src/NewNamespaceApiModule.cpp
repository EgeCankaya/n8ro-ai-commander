// Arkheon Technologies
// Proprietary and Confidential.
// Unauthorized copying of this file, via any medium, is strictly prohibited.
// © Arkheon Technologies. All rights reserved.

#include "NewNamespaceApiModule.h"

#include <scripting/LuaApiHelpers.h>
#include <scripting/MissionRegistrar.h>

#include <string>
#include <string_view>

namespace n8ro::sample::simscripting {

namespace {

constexpr std::string_view kTutorialNamespace = "tutorial";

// tutorial.add(a, b) -> number. Returns nil on invalid input.
n8ro::core::LuaValue tutorialAdd(const n8ro::core::LuaVariadicArgs& args) {
    if (args.size() < 2) {
        return std::monostate{};
    }
    double a = 0.0;
    double b = 0.0;
    if (!n8ro::core::tryReadLuaNumber(args[0], a)
        || !n8ro::core::tryReadLuaNumber(args[1], b)) {
        return std::monostate{};
    }
    return a + b;
}

// tutorial.greet(name) -> string. Falls back to "world" when no string argument is supplied.
n8ro::core::LuaValue tutorialGreet(const n8ro::core::LuaVariadicArgs& args) {
    std::string name = "world";
    if (!args.empty()) {
        std::string provided;
        if (n8ro::core::tryReadLuaString(args[0], provided) && !provided.empty()) {
            name = provided;
        }
    }
    return std::string("Hello, ") + name + "!";
}

} // namespace

const char* NewNamespaceApiModule::moduleId() const {
    return "sample.tutorial";
}

bool NewNamespaceApiModule::registerWith(
    n8ro::sim::MissionRegistrar& registrar,
    const n8ro::sim::ScriptingApiContext& context) {
    (void)context;
    bool allOk = true;

    // -- tutorial.add --
    {
        n8ro::sim::LuaApiFunctionMeta meta;
        meta.description =
            "Adds two numbers.\n"
            "Input: add(a, b)\n"
            "Returns: number (a + b), or nil on invalid input.\n"
            "Registered by the ai-commander sample plugin under the new 'tutorial' namespace.";
        meta.signatures.push_back(n8ro::sim::LuaApiSignatureMeta{
            {
                n8ro::sim::LuaApiParamMeta{"a", n8ro::sim::LuaApiType::Number, false},
                n8ro::sim::LuaApiParamMeta{"b", n8ro::sim::LuaApiType::Number, false}
            },
            {n8ro::sim::LuaApiType::Number}
        });
        allOk = registrar.registerFunctionWithMetadata(
                    kTutorialNamespace,
                    "add",
                    [](const n8ro::core::LuaVariadicArgs& args) -> n8ro::core::LuaValue {
                        return tutorialAdd(args);
                    },
                    std::move(meta))
            && allOk;
    }

    // -- tutorial.greet --
    {
        n8ro::sim::LuaApiFunctionMeta meta;
        meta.description =
            "Returns a greeting string.\n"
            "Input: greet(name)\n"
            "- name: optional string (defaults to \"world\")\n"
            "Returns: string.\n"
            "Registered by the ai-commander sample plugin under the new 'tutorial' namespace.";
        meta.signatures.push_back(n8ro::sim::LuaApiSignatureMeta{
            {n8ro::sim::LuaApiParamMeta{"name", n8ro::sim::LuaApiType::String, true}},
            {n8ro::sim::LuaApiType::String}
        });
        allOk = registrar.registerFunctionWithMetadata(
                    kTutorialNamespace,
                    "greet",
                    [](const n8ro::core::LuaVariadicArgs& args) -> n8ro::core::LuaValue {
                        return tutorialGreet(args);
                    },
                    std::move(meta))
            && allOk;
    }

    return allOk;
}

} // namespace n8ro::sample::simscripting
