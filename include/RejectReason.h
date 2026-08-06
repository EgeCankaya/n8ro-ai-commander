#pragma once

namespace arkheon::aicommander {

// Why an order was refused. Every rejection increments a named counter and writes exactly one
// `order.rejected` record carrying this reason (AIC-VAL-1).
//
// The reasons are deliberately fine-grained. A validator that only reported "rejected" would make
// the runbook useless: `reject.schema` climbing means the model is emitting malformed documents
// and the grammar is not engaged, while `reject.track` climbing means the model is hallucinating
// target ids or Tier 1 has stopped reporting. Those need different responses, so they need
// different counters — and the adversarial corpus asserts the reason, not merely the rejection,
// because a test that only asserts rejection cannot tell a schema failure from a range failure.
enum class RejectReason {
    None,

    // -- Stage A: worker thread, no SDK access ---------------------------------------------------
    Transport,   // A1. The request did not complete, or completed with a non-2xx status.
    Refusal,     // A2. The model declined (Claude `stop_reason == "refusal"`).
    Envelope,    // A2. The backend's response envelope did not have the expected shape.
    Parse,       // A3. The body is not one well-formed JSON object.
    Schema,      // A4. Structure, types, required fields, or an unknown property.
    Version,     // A5. `schemaVersion` is present and well-formed but is not 1.
    Enum,        // A5. `posture` or `roe` is a string outside its vocabulary.
    Shape,       // A6. Conditional-presence rules violated for the given posture.
    Range,       // A7. A numeric is non-finite or out of static range, or a string is too long.

    // -- Stage B: simulation thread, SDK access permitted ---------------------------------------
    Roster,      // B1. Not a commanded entity, or the entity no longer exists.
    Stale,       // B2. The snapshot the order derives from is older than maxOrderAgeS.
    Track,       // B3. The target was never reported by Tier 1 for this window.
    Fratricide,  // B4. The target is on the commanded entity's own team.
    Geofence,    // B5. The waypoint is outside safety.geofenceRadiusM of the entity.
    Clamp,       // B6. Altitude or speed outside the configured safety envelope.
    Superseded,  // B7. A newer order has already been published for this entity.
    // B8. engage/crank ordered for an aircraft whose reported stores are entirely dry. Note the
    // asymmetry with Track: an EMPTY reported loadout does not produce this reason, because
    // "Tier 1 reported nothing" is not "Tier 1 reported nothing left" (AIC-VAL-1, v1.7.3).
    Loadout,
    // B9. engage/crank ordered against a target whose SISO-REF-010 entity kind is 2 (Munition).
    // Note the asymmetry with Fratricide: an UNKNOWN kind does not produce this reason. B4 refuses
    // an unknown team because that makes the shot more dangerous; an unknown kind leaves it merely
    // unclassified, and refusing it would take every scenario whose profiles omit `entityType`
    // offline (AIC-VAL-1, v1.8.23).
    TargetClass,
};

// Stable lowercase identifiers. These are a wire contract, not a display string: they appear in
// the JSONL order log (AIC-DET-1), in the `aicmd.reject.<reason>` counter names, and in the
// runbook. Renaming one breaks every saved log and every alert.
[[nodiscard]] const char* toString(RejectReason reason);

// True when the reason can only be produced by Stage B. Used to assert the split holds: a Stage-A
// validator that returned a Stage-B reason would mean a semantic check had leaked onto the worker,
// where the SDK collaborators it needs are not legal to touch.
[[nodiscard]] bool isStageBReason(RejectReason reason);

} // namespace arkheon::aicommander
