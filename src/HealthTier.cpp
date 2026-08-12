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

    if (readName) {
        if (const std::optional<std::string> name = readName(entityId)) {
            for (std::size_t i = 0; i < kHealthTierNames.size(); ++i) {
                if (*name == kHealthTierNames[i]) {
                    return static_cast<HealthTier>(i);
                }
            }
            // A name the schema does not declare. Same reasoning as an out-of-range ordinal.
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
