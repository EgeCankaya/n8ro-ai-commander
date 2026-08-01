#include "Order.h"

namespace arkheon::aicommander {

const char* toString(Posture posture) {
    switch (posture) {
        case Posture::Ingress: return "ingress";
        case Posture::Engage:  return "engage";
        case Posture::Crank:   return "crank";
        case Posture::Defend:  return "defend";
        case Posture::Hold:    return "hold";
        case Posture::Rtb:     return "rtb";
    }
    return "hold";
}

const char* toString(Roe roe) {
    switch (roe) {
        case Roe::WeaponsFree:  return "weaponsFree";
        case Roe::WeaponsTight: return "weaponsTight";
        case Roe::WeaponsHold:  return "weaponsHold";
    }
    return "weaponsHold";
}

bool tryParsePosture(const std::string& text, Posture& out) {
    if (text == "ingress") { out = Posture::Ingress; return true; }
    if (text == "engage")  { out = Posture::Engage;  return true; }
    if (text == "crank")   { out = Posture::Crank;   return true; }
    if (text == "defend")  { out = Posture::Defend;  return true; }
    if (text == "hold")    { out = Posture::Hold;    return true; }
    if (text == "rtb")     { out = Posture::Rtb;     return true; }
    return false;
}

bool tryParseRoe(const std::string& text, Roe& out) {
    if (text == "weaponsFree")  { out = Roe::WeaponsFree;  return true; }
    if (text == "weaponsTight") { out = Roe::WeaponsTight; return true; }
    if (text == "weaponsHold")  { out = Roe::WeaponsHold;  return true; }
    return false;
}

bool postureRequiresTarget(Posture posture) {
    // engage and crank are the two postures that act against a specific contact. defend turns cold
    // from the nearest munition track, which the script identifies itself — the model is not asked
    // to name a missile.
    return posture == Posture::Engage || posture == Posture::Crank;
}

bool postureRequiresWaypoint(Posture posture) {
    // ingress / hold / rtb fly to a point the model chooses. engage / crank / defend fly geometry
    // the script computes, so a model-supplied waypoint would be ignored — and the schema forbids
    // it rather than accepting a field nothing reads.
    return posture == Posture::Ingress || posture == Posture::Hold || posture == Posture::Rtb;
}

} // namespace arkheon::aicommander
