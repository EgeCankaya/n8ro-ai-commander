#pragma once

#include <core/json/JsonValue.h>

#include <string>

namespace arkheon::aicommander {

// The order schema's version. The document is an external contract with a third party (the model),
// so it is versioned explicitly rather than implicitly by shape.
inline constexpr int kOrderSchemaVersion = 1;

// The bounds the schema itself enforces, independent of any `safety.*` configuration. These are
// the limits of the coordinate systems, not of the airframe: a latitude outside [-90, 90] is not a
// risky order, it is a meaningless one. Envelope limits live in `safety.*` and are checked in
// Stage B, where the live configuration is available.
inline constexpr double kMinLatitudeDeg = -90.0;
inline constexpr double kMaxLatitudeDeg = 90.0;
inline constexpr double kMinLongitudeDeg = -180.0;
inline constexpr double kMaxLongitudeDeg = 180.0;
inline constexpr double kMaxOrbitRadiusM = 50000.0;
// A static ceiling, not an airframe limit. The configured envelope is `safety.maxSpeedMps` and is
// enforced in Stage B; this only catches values no configuration could mean.
inline constexpr double kMaxCruiseSpeedMps = 1000.0;

inline constexpr std::size_t kMinReasonChars = 1;
inline constexpr std::size_t kMaxReasonChars = 200;
inline constexpr std::size_t kMaxEntityIdChars = 128;

// The single embedded JSON Schema document for an order (AIC-ORD-1).
//
// One definition, three consumers: Stage-A validation calls
// JsonValue::validateAgainstSchema against it, the Claude adapter sends it as
// output_config.format.schema, and the local adapter derives its GBNF grammar from it. Any drift
// between prompt, validator, and consumer is therefore impossible by construction rather than by
// discipline.
//
// `additionalProperties: false` is what makes "the model is structurally forbidden to emit
// kinematics" true: there is no property for heading, pitch, roll, velocity, acceleration, turn
// rate, load factor, hardpoint selection, or a fire command, and an unknown property is a rejection
// rather than something quietly ignored.
[[nodiscard]] const n8ro::core::JsonValue& orderJsonSchema();

// The same document as canonical text, for embedding in a prompt's stable prefix. Cached, so the
// prefix bytes are identical across every render in a run (AIC-BE-3).
[[nodiscard]] const std::string& orderJsonSchemaText();

} // namespace arkheon::aicommander
