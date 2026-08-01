// Arkheon Technologies
// Proprietary and Confidential.
// Unauthorized copying of this file, via any medium, is strictly prohibited.
// © Arkheon Technologies. All rights reserved.

#pragma once

#include <scripting/IScriptingApiModule.h>

namespace n8ro::sample::simscripting {

// Demonstrates the STATEFUL pattern: reading live entity state from a scripting function.
//
// Unlike the pure-math modules in this sample, these functions need runtime state, so the
// ScriptingApiContext is captured BY VALUE into the registered callback (the Lua runtime invokes
// the callback with only the script arguments — it cannot hand the context back at call time).
// Each function then reads a transform field from an entity's typed column through the generic,
// schema-driven read seam (component/ComponentFieldAccess.h) — components are generic schema-driven
// records, reached by (componentType, fieldPath), not a typed component interface:
//
//   readComponentFieldReal(*context.entityManager, id, kComponentTransform, "positionGeodetic/latitudeDeg")
//     -> std::optional<double>   (nullopt if the entity / component / field is unavailable)
//
// The field path is the schema leaf path (see the SDK schema reference for shape / units / frame).
//
// Registers a new `entityState` namespace with:
//   - entityState.getAltitude(entityId)            -> number (meters)
//   - entityState.rangeMeters(entityIdA, entityIdB) -> number (meters)
class EntityStateApiModule final : public n8ro::sim::IScriptingApiModule {
public:
    [[nodiscard]] const char* moduleId() const override;
    [[nodiscard]] bool registerWith(
        n8ro::sim::MissionRegistrar& registrar,
        const n8ro::sim::ScriptingApiContext& context) override;
};

} // namespace n8ro::sample::simscripting
