#pragma once

#include "Order.h"

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
// One definition, three consumers: Stage-A validation validates against it, the Claude adapter
// sends it as output_config.format.schema, and the local adapter sends it as Ollama's `format`
// parameter. Any drift between prompt, validator, and consumer is therefore impossible by
// construction rather than by discipline.
//
// SHAPE: `oneOf` over two posture-discriminated branches (PRD v1.7, §Corrections item 13).
//
//   waypoint branch  posture in {ingress, hold, rtb}   waypoint REQUIRED
//   geometry branch  posture in {engage, crank, defend} no `waypoint` property at all
//
// Both branches require targetEntityId and orbitRadiusM, and both set additionalProperties:false.
//
// This is not decoration. A constrained decoder compels exactly what a schema's `required` array
// names, so a flat document with optional conditional fields compels none of them — and the model
// measured for Phase 1b omits every field it is not compelled to emit, producing 10/12 Stage-A
// `shape` rejections. The same orders under this shape measured 0/12 with the prompt untouched.
//
// Stage-A check A6 still enforces the same rules independently. The decoder is a second line, not
// a substitute: a backend that stops honouring `format` has to fail as a rejection, not as an
// accepted order.
//
// `additionalProperties: false` is what makes "the model is structurally forbidden to emit
// kinematics" true: there is no property for heading, pitch, roll, velocity, acceleration, turn
// rate, load factor, hardpoint selection, or a fire command, and an unknown property is a rejection
// rather than something quietly ignored.
[[nodiscard]] const n8ro::core::JsonValue& orderJsonSchema();

// The same document as canonical text, for embedding in a prompt's stable prefix. Cached, so the
// prefix bytes are identical across every render in a run (AIC-BE-3).
[[nodiscard]] const std::string& orderJsonSchemaText();

// The single branch of that document which applies to `posture`.
//
// Returned by reference into the same built document, so there is no second definition to drift.
//
// It exists because Stage-A check A4 uses JsonValue::validateAgainstSchema as a structural
// backstop, and the SDK does not document which JSON Schema keywords that validator implements.
// A4 runs after the posture has been parsed, so selecting the branch is free and does not depend
// on `oneOf` support the validator may not have.
[[nodiscard]] const n8ro::core::JsonValue& orderSchemaBranch(Posture posture);

} // namespace arkheon::aicommander
