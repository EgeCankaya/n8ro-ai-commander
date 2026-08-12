#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace n8ro::sim {
class IEntityManager;
}

namespace arkheon::aicommander {

// The damage tiers of `componentLifecycle/health`, in the schema's declaration order.
//
// The schema states the enum is "ordinal and monotonic - the tier only escalates, never recovers,
// and destroyed is terminal", so the ordinal IS the tier and a later one is strictly worse. The
// names are taken from `schema-reference.json`'s `enumValues` for
// /datablocks/componentLifecycle/health rather than from a header: there is no typed lifecycle
// interface, and these names are the schema's rather than ours to invent.
enum class HealthTier : std::size_t {
    Nominal = 0,
    Degraded = 1,
    Disabled = 2,
    Wrecked = 3,
    Destroyed = 4,
};

// The schema's own spelling of a tier, for the log line an operator will read.
[[nodiscard]] std::string_view toString(HealthTier tier);

// The tier at which an airframe stops being worth commanding: `wrecked` and `destroyed` only.
//
// `disabled` deliberately keeps receiving orders. It is a mission kill rather than a lost airframe -
// BlueF16_01 was disabled at t+64 s of the 2026-08-12 run and went on flying and fighting - and
// refusing to command an aircraft that is still flying would be a worse error than the one this
// guard exists to fix.
[[nodiscard]] bool isUncommandable(HealthTier tier);

// The two seams. `componentLifecycle/health` is an ENUM leaf and nothing specifies how an enum
// column is stored, so both accessors have to be tried - see readHealthTierWith.
using HealthOrdinalReader = std::function<std::optional<std::int64_t>(const std::string& entityId)>;
using HealthNameReader = std::function<std::optional<std::string>(const std::string& entityId)>;

// The pure form. Returns nullopt when the entity, the component, or the field is unavailable, and
// when the value resolves to something outside the schema's declared set.
//
// It tries the ordinal column and then the text column, and that is not indecision.
// `ComponentFieldAccess.h` documents each reader as returning nullopt "when the field's value type
// does not match the accessor" and says nothing about how an ENUM column is typed; the schema
// reference supplies the ordered value set and is equally silent on storage. Committing to one
// accessor on a guess would not fail loudly - it would return nullopt on every call for the life of
// the process, leaving a guard that is installed, green, and never fires. That is the muted-guard
// failure this project already has a name for (AIC-BE-3, §Corrections item 35(b)), and it is not
// worth re-earning here.
[[nodiscard]] std::optional<HealthTier> readHealthTierWith(
    const std::string& entityId,
    const HealthOrdinalReader& readOrdinal,
    const HealthNameReader& readName);

// The engine-facing form: binds both seams to ComponentFieldAccess against the live entity manager.
[[nodiscard]] std::optional<HealthTier> readHealthTier(
    const n8ro::sim::IEntityManager& manager, const std::string& entityId);

} // namespace arkheon::aicommander
