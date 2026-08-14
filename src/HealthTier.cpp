#include "HealthTier.h"

#include <component/ComponentFieldAccess.h>
#include <component/ComponentTypeNames.h>

#include <array>

namespace arkheon::aicommander {

namespace {

// The schema's declared value set, in declaration order. The index into this array is the ordinal.
constexpr std::array<std::string_view, 5> kHealthTierNames{
    "nominal", "degraded", "disabled", "wrecked", "destroyed"};

// The field path is the schema leaf path relative to the component: take the record's path
// (/datablocks/componentLifecycle/health) and drop the /datablocks/<componentType>/ prefix.
constexpr std::string_view kHealthFieldPath = "health";

constexpr std::size_t kFirstUncommandableOrdinal = static_cast<std::size_t>(HealthTier::Wrecked);

} // namespace

std::string_view toString(HealthTier tier) {
    const std::size_t ordinal = static_cast<std::size_t>(tier);
    if (ordinal >= kHealthTierNames.size()) {
        return "unknown";
    }
    return kHealthTierNames[ordinal];
}

bool isUncommandable(HealthTier tier) {
    return static_cast<std::size_t>(tier) >= kFirstUncommandableOrdinal;
}

std::optional<HealthTier> readHealthTierWith(
    const std::string& entityId,
    const HealthOrdinalReader& readOrdinal,
    const HealthNameReader& readName) {
    if (entityId.empty()) {
        return std::nullopt;
    }

    // TEXT FIRST, and the order is empirical rather than a preference.
    //
    // Both columns are still consulted, because nothing in ComponentFieldAccess.h or the schema
    // reference specifies how an enum column is stored — that is why this seam exists. But on the
    // validated release (n8ro@2.1.144, §Dependencies) the column is TEXT, and asking for the int
    // column first makes the SDK emit
    //
    //     [ERROR] (DynamicStore) DynamicStore::getInt: handle is not an int field
    //
    // on EVERY read before the fallback succeeds — once per commanded entity per cadence tick, at
    // ERROR, in the same log an operator reads to decide whether a run is healthy. Measured on the
    // 2026-08-12 22:27Z headless run. The guard worked the whole time; it simply announced a failure
    // each time it did, which is the muted-warning failure this project argues against wearing the
    // opposite costume — a real ERROR line that means nothing, teaching the reader to skip ERROR.
    //
    // Reversing the order costs a tree whose column IS ordinal nothing: that path is still tried,
    // one lookup later. It buys silence on the tree this plugin is pinned to and tested against.
    if (readName) {
        if (const std::optional<std::string> name = readName(entityId)) {
            for (std::size_t i = 0; i < kHealthTierNames.size(); ++i) {
                if (*name == kHealthTierNames[i]) {
                    return static_cast<HealthTier>(i);
                }
            }
            // A name the schema does not declare. The column RESOLVED, so it exists and this build
            // cannot interpret its value; consulting the other column would report a tier for a
            // value the schema does not declare. Unknown, and deliberately NOT read as wrecked.
            return std::nullopt;
        }
    }

    if (readOrdinal) {
        if (const std::optional<std::int64_t> ordinal = readOrdinal(entityId)) {
            if (*ordinal < 0 || static_cast<std::size_t>(*ordinal) >= kHealthTierNames.size()) {
                // Outside the schema's declared range. Unknown, and deliberately NOT read as
                // wrecked: a tier this code cannot interpret must never be the reason an intact
                // aircraft goes uncommanded.
                return std::nullopt;
            }
            return static_cast<HealthTier>(static_cast<std::size_t>(*ordinal));
        }
    }

    return std::nullopt;
}

std::optional<HealthTier> readHealthTier(
    const n8ro::sim::IEntityManager& manager, const std::string& entityId) {
    return readHealthTierWith(
        entityId,
        [&manager](const std::string& id) {
            return n8ro::sim::readComponentFieldInt(
                manager, id, n8ro::sim::kComponentLifecycle, kHealthFieldPath);
        },
        [&manager](const std::string& id) {
            return n8ro::sim::readComponentFieldText(
                manager, id, n8ro::sim::kComponentLifecycle, kHealthFieldPath);
        });
}

} // namespace arkheon::aicommander
