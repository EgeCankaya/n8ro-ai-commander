#include "RejectReason.h"

namespace arkheon::aicommander {

const char* toString(RejectReason reason) {
    switch (reason) {
        case RejectReason::None:       return "none";
        case RejectReason::Transport:  return "transport";
        case RejectReason::Refusal:    return "refusal";
        case RejectReason::Envelope:   return "envelope";
        case RejectReason::Parse:      return "parse";
        case RejectReason::Schema:     return "schema";
        case RejectReason::Version:    return "version";
        case RejectReason::Enum:       return "enum";
        case RejectReason::Shape:      return "shape";
        case RejectReason::Range:      return "range";
        case RejectReason::Roster:     return "roster";
        case RejectReason::Stale:      return "stale";
        case RejectReason::Track:      return "track";
        case RejectReason::Fratricide: return "fratricide";
        case RejectReason::Geofence:   return "geofence";
        case RejectReason::Clamp:      return "clamp";
        case RejectReason::Superseded: return "superseded";
        case RejectReason::Loadout:    return "loadout";
    }
    return "none";
}

bool isStageBReason(RejectReason reason) {
    switch (reason) {
        case RejectReason::Roster:
        case RejectReason::Stale:
        case RejectReason::Track:
        case RejectReason::Fratricide:
        case RejectReason::Geofence:
        case RejectReason::Clamp:
        case RejectReason::Superseded:
        case RejectReason::Loadout:
            return true;
        default:
            return false;
    }
}

} // namespace arkheon::aicommander
