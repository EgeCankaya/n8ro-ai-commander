#include "PromptRenderer.h"

#include "Order.h"
// OrderSchema.h is deliberately NOT included. It was, until C3 removed the prose rendering of the
// order schema from the prefix (v1.8.14, and the WHAT IS NOT HERE block in build() below), and
// nothing in this file has referred to a symbol from it since. `orderJsonSchemaText()` itself stays
// very much alive - LocalLlmClient sends it as Ollama's `format` document, and the hosted adapter
// sends a projection of the same schema - it simply is not this file's business any more.

#include <core/json/JsonValue.h>

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace arkheon::aicommander {

namespace {

using n8ro::core::JsonValue;

// Fixed-precision formatting. std::to_string's locale-independence is not guaranteed for doubles
// across every runtime, and a prompt whose numbers reformat between runs would break both prefix
// stability and snapshotHash comparison.
[[nodiscard]] std::string fixed(double value, int decimals) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(decimals) << value;
    return stream.str();
}

constexpr const char* kSystemPrompt =
    "You are a tactical commander for a single simulated aircraft.\n"
    "You issue INTENT only: a posture, an optional target, an optional waypoint, a cruise speed, "
    "and rules of engagement.\n"
    "You do NOT fly the aircraft. Heading, pitch, roll, throttle, turn geometry, launch timing, "
    "and weapon release are decided by deterministic logic you cannot address.\n"
    "Reply with exactly one JSON object conforming to the schema below, and nothing else. No "
    "prose, no markdown fence, no explanation outside the 'reason' field.\n"
    "If the situation does not warrant a change, reply with the posture that keeps the aircraft "
    "safe. Never invent a target that is not in the provided track list.\n";

constexpr const char* kPostureVocabulary =
    "POSTURES:\n"
    "  ingress - fly to the ordered waypoint at the ordered speed. Requires waypoint.\n"
    "  engage  - pursue the ordered target. Requires targetEntityId. No waypoint: the aircraft "
    "computes its own pursuit geometry.\n"
    "  crank   - fly an offset from the ordered target while a shot is assessed. Requires "
    "targetEntityId. No waypoint.\n"
    "  defend  - turn cold from the nearest threat and descend. No target, no waypoint: the "
    "aircraft selects the threat and computes the maneuver.\n"
    "  hold    - orbit the ordered waypoint at the ordered radius. Requires waypoint and "
    "orbitRadiusM > 0.\n"
    "  rtb     - egress to the ordered waypoint. Requires waypoint.\n"
    "\n"
    "RULES OF ENGAGEMENT (independent of posture):\n"
    "  weaponsFree  - the aircraft may select and engage targets within its own rules.\n"
    "  weaponsTight - the aircraft may fire only at the ordered targetEntityId.\n"
    "  weaponsHold  - the aircraft must cease fire and not engage, whatever the posture.\n";

} // namespace

void PromptRenderer::build(const CommanderConfig& config, const std::string& doctrineText) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());

    // WHAT IS NOT HERE, AND WHY (v1.8.14, C3). The order schema used to be rendered into the prefix
    // here as prose, immediately after the posture vocabulary. It is gone.
    //
    // It was never redundant when it was written: on the local backend `format` was the only
    // structural constraint, and the prose copy was what a model read for field semantics. On the
    // hosted path the adapter ALSO sends the schema as `output_config.format.schema`, so the
    // document went out twice per request - ~71 % of the cached prefix in two renderings of one
    // thing (§Corrections item 22).
    //
    // The removal was held unmade across four revisions on purpose, because the cost half was
    // measured and the QUALITY half was not, and this project's §Corrections is largely a record of
    // what happens when a measured-good artifact is changed on the strength of a plausible story.
    // It is made now on a pre-registered paired arm: 120 orders each, the prose copy the only
    // difference, decision rule fixed in the document BEFORE the run (§Corrections item 30).
    // Both arms accepted 120/120 with reject.schema and reject.shape at 0.00 %.
    //
    // TWO CONSEQUENCES A LATER EDITOR NEEDS. (1) The structured-output projection is now the ONLY
    // description of the order shape that reaches the model on the hosted path - if a backend is
    // ever added that does not constrain decoding, it does not inherit this and the prose copy has
    // to come back for it. (2) The cached block falls to ~5,075 tokens against Haiku 4.5's 4,096
    // minimum. The margin was 86 %; it is now 24 %. Roughly 980 tokens of further prefix reduction
    // stops the prefix caching at all, which costs far more than it saves (§Corrections item 31).
    // The bare '\n' below is deliberate and is NOT a leftover. It is the blank line that used to
    // separate the schema block from DOCTRINE, and it is kept so that the prefix this ships is
    // BYTE-IDENTICAL to the arm that measured 120/120. Dropping it as tidy-up made the shipped
    // prefix 8,749 bytes against the measured 8,750 - a one-byte divergence between the artifact
    // under test and the artifact in production, which is a small instance of exactly the class of
    // error §Corrections is a record of. Kept, and the reason written down so it survives the next
    // person who notices it looks redundant.
    stream << kSystemPrompt << '\n'
           << kPostureVocabulary << '\n'
           << '\n'
           << "DOCTRINE:\n"
           << (doctrineText.empty() ? std::string("(none provided)") : doctrineText) << '\n'
           << '\n'
           << "You command at most " << config.maxCommandedEntities
           << " aircraft, and you are asked for a new order roughly every "
           << fixed(config.cadenceS, 0) << " seconds. An order you issue may still be in force "
           << "when the situation has moved on, so prefer intent that stays sensible for that long.\n";

    // Only the model name and cadence are permitted config values here, per the transmitted-field
    // allowlist. No endpoint, no path, no key name, no safety bound.
    prefix_ = stream.str();
    built_ = true;
}

std::string PromptRenderer::renderSuffix(const OrderSnapshot& snapshot) const {
    // Built as JSON so a value carrying an awkward character is escaped rather than able to break
    // out of its field. The suffix is the only place scenario state enters a prompt, and part of
    // it (track ids) originates outside the plugin.
    JsonValue root = JsonValue::object();
    (void)root.setDouble("simTimeS", snapshot.simTimeS);

    JsonValue own = JsonValue::object();
    (void)own.setString("entityId", snapshot.entityId);
    (void)own.setDouble("latitudeDeg", snapshot.latitudeDeg);
    (void)own.setDouble("longitudeDeg", snapshot.longitudeDeg);
    (void)own.setDouble("altitudeHaeM", snapshot.altitudeHaeM);
    (void)own.setDouble("courseDeg", snapshot.courseDeg);
    // Emitted BEFORE the three signed components. C15's original defect was a model reaching past
    // the field it wanted because that field was not there; it is there now, and it is there first.
    //
    // The velocity components STAY, and the plan to delete them is dropped (v1.8.28, item 44(d)).
    // v1.8.26 read a cluster of copied float values as the model "selecting among" speedMps, velE
    // and velN, and proposed removing velN/velE as decoys. The first run carrying snapshot content
    // refuted that outright: all nine accepted orders matched own.speedMps EXACTLY, and the clusters
    // were simply own.speedMps at different times. Removing fields on a premise the measurement
    // contradicted would be the same mistake the removal was meant to fix.
    (void)own.setDouble("speedMps", snapshot.speedMps);
    (void)own.setDouble("velN", snapshot.velNMps);
    (void)own.setDouble("velE", snapshot.velEMps);
    (void)own.setDouble("velD", snapshot.velDMps);
    (void)own.setString("team", snapshot.team);

    JsonValue loadout = JsonValue::array();
    for (const LoadoutReport& row : snapshot.loadout) {
        JsonValue entry = JsonValue::object();
        (void)entry.setString("hardpointName", row.hardpointName);
        (void)entry.setString("weaponProfileName", row.weaponProfileName);
        (void)entry.setInt64("ammoCount", row.ammoCount);
        (void)entry.setInt64("ammoMax", row.ammoMax);
        (void)loadout.pushBack(entry);
    }
    (void)own.set("loadout", loadout);
    (void)root.set("own", own);

    // Tracks carry ONLY the three scalars aiCommander.reportTrack accepts. No team, no kind, no
    // domain: the ingress verb does not carry them and the plugin will not infer them (PRD v1.2,
    // §Exactly what is transmitted). Every componentTrackIdentity free-text field is excluded
    // entirely — those are documented as ingested from an external feed and are the injection
    // channel the threat model names.
    JsonValue tracks = JsonValue::array();
    for (const TrackReport& row : snapshot.tracks) {
        JsonValue entry = JsonValue::object();
        (void)entry.setString("targetEntityId", row.targetEntityId);
        (void)entry.setDouble("rangeM", row.rangeM);
        (void)entry.setDouble("snrDb", row.snrDb);
        (void)tracks.pushBack(entry);
    }
    (void)root.set("tracks", tracks);

    if (!snapshot.situationNote.empty()) {
        (void)root.setString("situationNote", snapshot.situationNote);
    }

    return root.toString();
}

std::string PromptRenderer::render(const OrderSnapshot& snapshot) const {
    return prefix_ + "\nSITUATION:\n" + renderSuffix(snapshot) + "\n\nYour order:\n";
}

std::string PromptRenderer::stableHash(const std::string& text) {
    // FNV-1a, 64-bit. Not a cryptographic hash and not claimed to be one: its job is drift
    // detection across runs of the same build, which a fast non-cryptographic hash does as well as
    // SHA-256 while adding no dependency. The record field is prefixed so nobody reading the log
    // mistakes it for a digest with collision guarantees.
    std::uint64_t hash = 1469598103934665603ULL;
    for (const char c : text) {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
        hash *= 1099511628211ULL;
    }
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return stream.str();
}

} // namespace arkheon::aicommander
