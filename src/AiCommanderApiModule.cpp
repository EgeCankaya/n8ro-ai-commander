#include "AiCommanderApiModule.h"

#include "CommanderRuntime.h"

#include <scripting/LuaApiHelpers.h>
#include <scripting/MissionRegistrar.h>

#include <string_view>
#include <utility>

namespace arkheon::aicommander {

namespace {

constexpr std::string_view kNamespace = "aiCommander";

} // namespace

const char* AiCommanderApiModule::moduleId() const {
    return "arkheon.aiCommander.api";
}

bool AiCommanderApiModule::registerWith(
    n8ro::sim::MissionRegistrar& registrar,
    const n8ro::sim::ScriptingApiContext& context) {
    bool allOk = true;
    CommanderRuntime* runtime = runtime_;

    // -- aiCommander.getStats --
    {
        n8ro::sim::LuaApiFunctionMeta meta;
        meta.description =
            "Returns the commander's health and counters as a JSON string.\n"
            "Input: getStats()\n"
            "Returns: string (JSON) with enabled, backend, operational, runtimeColumnProbe, orders.\n"
            "Never raises; returns \"{}\" if the commander runtime is unavailable.";
        meta.signatures.push_back(n8ro::sim::LuaApiSignatureMeta{
            {},
            {n8ro::sim::LuaApiType::String}
        });

        // The context is captured by value even though this particular function does not read
        // entity state — the capture rule is a property of how the registrar stores the callback,
        // not of what any one function happens to need today.
        allOk = registrar.registerFunctionWithMetadata(
                    kNamespace,
                    "getStats",
                    [context, runtime](const n8ro::core::LuaVariadicArgs&) -> n8ro::core::LuaValue {
                        (void)context;
                        if (runtime == nullptr) {
                            return std::string("{}");
                        }
                        return runtime->statsJson();
                    },
                    std::move(meta))
            && allOk;
    }

    return allOk;
}

} // namespace arkheon::aicommander
