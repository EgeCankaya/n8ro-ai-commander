#include "TestSupport.h"

#include <core/scripting/ILuaEvalRuntime.h>
#include <core/scripting/LuaScriptRuntimeFactory.h>

#include <fstream>
#include <memory>
#include <sstream>
#include <string>

// THE REFERENCE TIER-1 SCRIPT, EXERCISED IN A REAL LUA VM (AIC-ORD-2, PRD v1.8.30, C21).
//
// WHY THIS FILE EXISTS, AND WHY IT DID NOT BEFORE. `lua/ai_commander_interceptor.lua` is a shipped
// deliverable that AIC-ORD-2 states acceptance criteria about, and until this file NOTHING checked
// it - not its syntax, not its behaviour, not even that it parses. It was validated by being run
// inside a 22-minute engine scenario and read afterwards in an order log.
//
// That is how C21 survived for the project's whole life. `considerFiring` sat inside the `engage`
// branch and `fallBackToWaypoints` returned before any fire logic, so a commanded aircraft with no
// order in force never shot at all - and the cost, measured over twelve paired runs, was 4 launches
// against the shipped script's 32 and 22 of 24 commanded aircraft destroyed against 0 of 24. Every
// published metric stayed green throughout, because none of them was an outcome and none of them
// could see a Lua file.
//
// WHAT MAKES THIS TESTABLE WITHOUT AN ENGINE. The script talks to the world through six global
// tables - sensor, navigation, weapon, entityControl, aiCommander, mission - and nothing else. So a
// Lua chunk that defines those six as recording stubs, then calls onTick, drives the real script
// through the real VM and observes exactly which verbs it invoked. No engine, no scenario, no
// inference server, and it runs in milliseconds inside the offline suite.
//
// The engine's own Lua runtime is used rather than a bundled interpreter, so the sandbox, the
// deterministic math and the instruction budget are the ones the script will actually run under.

using arkheon::aicommander::testing::repoRoot;

namespace {

std::string readReferenceScript(std::string& error) {
    if (repoRoot().empty()) {
        error = "repository root not found - the suite could not locate ai-commander.slnx by "
                "walking up from argv[0], so it cannot read the reference script. This is a "
                "FAILURE and not a skip: a guard that quietly does nothing still reports green.";
        return {};
    }
    const std::string path = repoRoot() + "/lua/ai_commander_interceptor.lua";
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "could not open the reference script at " + path;
        return {};
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

// One session per test, so a chunk that leaves globals behind cannot make the next test pass.
struct LuaHarness {
    std::unique_ptr<n8ro::core::ILuaEvalRuntime> runtime;
    std::string sessionId;

    explicit LuaHarness(std::string id)
        : runtime(n8ro::core::createLuaEvalRuntime()), sessionId(std::move(id)) {}

    [[nodiscard]] bool ok() const { return runtime != nullptr; }

    [[nodiscard]] n8ro::core::EvalResult eval(const std::string& code) {
        return runtime->evaluateString(sessionId, code);
    }
};

// The six global tables the script talks to, as recording stubs.
//
// `calls` accumulates one line per verb invocation, so a test asserts on WHAT THE SCRIPT DID rather
// than on what its source text looks like. A grep-based test would pass on a script that had been
// refactored into a shape that no longer works.
//
// The defaults here describe an aircraft with a full loadout, no contacts, and no commander. Each
// test overrides only the part it is about, which keeps what it is testing visible.
constexpr const char* kStubWorld = R"LUA(
calls = {}
local function note(fmt, ...) calls[#calls + 1] = string.format(fmt, ...) end
function called(needle)
    for _, line in ipairs(calls) do
        if string.find(line, needle, 1, true) ~= nil then return true end
    end
    return false
end

-- Positions are degrees; 1 degree of latitude is ~111 km, which is the only conversion these
-- tests need to place a contact at a chosen range.
world = {
    positions = { OWN = {13.50, 144.80, 10000.0} },
    tracks = {},                 -- { {id=, rangeM=, snrDb=}, ... }
    info = {},                   -- id -> { team=, entityTypeCode = { kind=, domain= } }
    closestByKind = {},          -- kind -> { id, rangeM }
    ammo = { { hardpointName = "R77_BVR", weaponProfileName = "P", ammoCount = 4, ammoMax = 4 },
             { hardpointName = "R73_IR",  weaponProfileName = "P", ammoCount = 2, ammoMax = 2 } },
}

mission = { log = function(m) note("mission.log %s", m) end,
            markRunning = function() end, markCompleted = function() note("markCompleted") end }

entityControl = {
    getPositionGeodetic = function(id)
        local p = world.positions[id]
        if p == nil then return nil end
        return p[1], p[2], p[3]
    end,
    getEntityInfo = function(id) return world.info[id] end,
}

sensor = {
    getTrackNr = function() return #world.tracks end,
    getTrackById = function(_, i)
        local t = world.tracks[i]
        if t == nil then return nil end
        return t.id, t.rangeM, t.snrDb
    end,
    getClosestHostileTrackById = function(_, kind)
        local hit = world.closestByKind[kind or "any"]
        if hit == nil then return nil end
        return hit[1], hit[2]
    end,
}

navigation = {
    requestGoTo = function(id, lat, lon, alt, spd)
        note("navigation.requestGoTo %s alt=%.0f spd=%.0f", id, alt or -1, spd or -1); return true
    end,
    requestTrackTarget = function(id, tgt, spd)
        note("navigation.requestTrackTarget %s -> %s", id, tgt); return true
    end,
    requestHoldPosition = function(id, lat, lon, alt, r, spd)
        note("navigation.requestHoldPosition %s r=%.0f", id, r); return true
    end,
    resumeWaypointFollowing = function(id) note("navigation.resumeWaypointFollowing %s", id); return true end,
}

weapon = {
    getWeaponLoadout = function() return world.ammo end,
    requestFire = function(id, tgt, hp) note("weapon.requestFire %s -> %s from %s", id, tgt, hp); return true end,
    requestCeaseFire = function(id) note("weapon.requestCeaseFire %s", id); return true end,
    canFire = function() error("canFire must never be called - AIC-ORD-2 v1.8.21") end,
}

-- Placed at 25 km due east of OWN: inside R77_BVR's disciplined 36 km launch range and outside
-- R73_IR's 9.6 km, so a correct envelope-based hardpoint selection picks the BVR rail.
function placeContact(id, kmEast, team, kind, domain)
    world.positions[id] = {13.50, 144.80 + kmEast / 111.32 / math.cos(math.rad(13.5)), 10000.0}
    world.tracks[#world.tracks + 1] = { id = id, rangeM = kmEast * 1000.0, snrDb = 20.0 }
    world.info[id] = { team = team, entityTypeCode = { kind = kind, domain = domain } }
end

world.info.OWN = { team = "Red", entityTypeCode = { kind = 1, domain = 2 } }
)LUA";

// EvalResult::returnValue is the SERIALIZED first return value, so a Lua string comes back JSON-
// quoted. Stripping the quotes here keeps every assertion below comparing the value the script
// produced rather than the serializer's rendering of it.
std::string unquote(const std::string& serialized) {
    if (serialized.size() >= 2 && serialized.front() == '"' && serialized.back() == '"') {
        return serialized.substr(1, serialized.size() - 2);
    }
    return serialized;
}

// Fails the calling test with the Lua error when a chunk did not run.
#define AIC_LUA_OK(result, what)                                                                   \
    do {                                                                                           \
        const auto& aicRes = (result);                                                             \
        AIC_EXPECT_TRUE(aicRes.success, what << ": " << aicRes.errorMessage);                      \
    } while (0)

} // namespace

// The floor beneath every other test in this file, and it is worth having on its own: until
// v1.8.30 nothing in this repository would have noticed a syntax error in a shipped Lua file. It
// would have surfaced as an entity that silently did nothing in a 22-minute scenario run.
AIC_TEST(ReferenceScriptCompilesAndDefinesItsEntryPoints) {
    std::string error;
    const std::string source = readReferenceScript(error);
    AIC_EXPECT_TRUE(error.empty(), error);

    LuaHarness lua("ref-compiles");
    AIC_EXPECT_TRUE(lua.ok(), "the SDK's Lua eval runtime could not be created");
    AIC_LUA_OK(lua.eval(source), "the reference script must compile and load");

    const auto shape = lua.eval(
        "return type(onInit) .. ',' .. type(onTick) .. ',' .. type(onShutdown)");
    AIC_LUA_OK(shape, "probing the entry points");
    AIC_EXPECT_EQ(unquote(shape.returnValue), std::string("function,function,function"),
                  "the script must define onInit, onTick and onShutdown as functions - the engine "
                  "calls these by name and a missing one is an entity that never ticks");
    return true;
}

// C21, MECHANISM 4, AND THE ONE MOST LIKELY TO HAVE KILLED THE AIRCRAFT.
//
// The shipped oppint_red_interceptor.lua's FIRST action every tick is a munition-defeat check
// inside 15 km. The commander-aware script had none: it flew `defend` only when the model ordered
// it, on a 20 s cadence plus a measured p50 of 3.5 s inference. An inbound AMRAAM crosses 15 km in
// roughly fifteen seconds. §Non-goals assigns missile defeat to Tier 0/1 "where it already works"
// and Tier 1 had not implemented it.
//
// The commander is ABSENT here - aiCommander is nil, exactly as it is after a rollback - because
// the requirement is that no order and no ABSENCE of an order can suppress survival.
AIC_TEST(ReferenceScriptDefendsAgainstAnInboundMissileWithNoCommanderAtAll) {
    std::string error;
    const std::string source = readReferenceScript(error);
    AIC_EXPECT_TRUE(error.empty(), error);

    LuaHarness lua("ref-defends");
    AIC_EXPECT_TRUE(lua.ok(), "the SDK's Lua eval runtime could not be created");
    AIC_LUA_OK(lua.eval(kStubWorld), "the stub world must load");
    AIC_LUA_OK(lua.eval(source), "the reference script must compile");

    // No commander at all, and a hostile munition at 8 km - inside the 15 km threat range. The
    // threat needs a POSITION as well as a track: the cold vector is computed from the geometry,
    // so a threat the script can see but cannot locate is one it cannot turn away from.
    AIC_LUA_OK(lua.eval(
        "aiCommander = nil\n"
        "placeContact('BLUE_wpn_1', 8.0, 'Blue', 2, 9)\n"
        "world.closestByKind[2] = { 'BLUE_wpn_1', 8000.0 }\n"
        "onInit('OWN'); onTick('OWN', 100.0, 0.05)"),
        "driving one tick with a missile inbound and no commander");

    const auto defended = lua.eval("return tostring(called('navigation.requestGoTo'))");
    AIC_LUA_OK(defended, "checking the defensive reflex");
    AIC_EXPECT_EQ(unquote(defended.returnValue), std::string("true"),
                  "with a munition inside the threat range the script MUST manoeuvre on the tick it "
                  "is detected, whether or not a commander exists. This is Tier-0 behaviour that "
                  "§Non-goals already assigns to Tier 0/1, and its absence is the most plausible "
                  "mechanism behind 22 of 24 commanded aircraft being destroyed");

    // It must descend, not merely move: the defensive pump is a descent to kDefendAltM = 3000.
    const auto descended = lua.eval("return tostring(called('alt=3000'))");
    AIC_LUA_OK(descended, "checking the descent");
    AIC_EXPECT_EQ(unquote(descended.returnValue), std::string("true"),
                  "the pump must descend to the defend altitude, not just turn");

    // And nothing else runs on the tick it survives.
    const auto fired = lua.eval("return tostring(called('weapon.requestFire'))");
    AIC_LUA_OK(fired, "checking that survival preempts the rest of the tick");
    AIC_EXPECT_EQ(unquote(fired.returnValue), std::string("false"),
                  "the defensive pump returns; the rest of the tick must not run");
    return true;
}

// C21, MECHANISMS 1 AND 2, ASSERTED TOGETHER BECAUSE THEY FAILED TOGETHER.
//
// With no order in force, the previous version called navigation.resumeWaypointFollowing and
// RETURNED - before any fire logic. Every gap between orders, every rejection, every timeout and
// the whole run if the backend was down, the aircraft flew in a straight line and did not shoot.
// An isolating third arm reproduced it with no model in the loop at all: 0 launches, 2 of 2 lost.
AIC_TEST(ReferenceScriptStillFightsWhenNoOrderIsInForce) {
    std::string error;
    const std::string source = readReferenceScript(error);
    AIC_EXPECT_TRUE(error.empty(), error);

    LuaHarness lua("ref-fights");
    AIC_EXPECT_TRUE(lua.ok(), "the SDK's Lua eval runtime could not be created");
    AIC_LUA_OK(lua.eval(kStubWorld), "the stub world must load");
    AIC_LUA_OK(lua.eval(source), "the reference script must compile");

    // Commander gone entirely; one hostile air platform at 25 km. Kind 1 / domain 2 is a platform
    // in the air domain, per SISO-REF-010 as the shipped script reads it.
    AIC_LUA_OK(lua.eval(
        "aiCommander = nil\n"
        "placeContact('BANDIT_01', 25.0, 'Blue', 1, 2)\n"
        "onInit('OWN'); onTick('OWN', 100.0, 0.05)"),
        "driving one tick with a bandit in range and no commander");

    const auto pursued = lua.eval("return tostring(called('navigation.requestTrackTarget'))");
    AIC_LUA_OK(pursued, "checking pursuit");
    AIC_EXPECT_EQ(unquote(pursued.returnValue), std::string("true"),
                  "with no order in force the script must select its own target and pursue it, not "
                  "resume waypoint following and stop");

    const auto fired = lua.eval("return tostring(called('weapon.requestFire'))");
    AIC_LUA_OK(fired, "checking the launch");
    AIC_EXPECT_EQ(unquote(fired.returnValue), std::string("true"),
                  "AND IT MUST SHOOT. This is C21 in one assertion: the previous version returned "
                  "before any fire logic whenever no order was in force, which cost 22 of 24 "
                  "commanded aircraft across twelve paired runs");

    // The rail is chosen by ENVELOPE (v1.8.30), not by table order. At 25 km only R77_BVR's
    // disciplined range (45,000 * 0.8 = 36,000 m) covers the target; R73_IR's is 9,600 m. The
    // previous firstLoadedHardpoint would have taken whichever rail came first.
    const auto rail = lua.eval("return tostring(called('from R77_BVR'))");
    AIC_LUA_OK(rail, "checking hardpoint selection");
    AIC_EXPECT_EQ(unquote(rail.returnValue), std::string("true"),
                  "the hardpoint must be selected by its own engagement envelope - a short-range "
                  "IR missile launched at 25 km is a wasted round, and the previous version could "
                  "launch one at four times its kinematic reach");
    return true;
}

// C21, MECHANISM 2, ON THE POSTURE THAT MADE IT EXPENSIVE.
//
// `considerFiring` used to sit inside the `engage` branch, and `engage` is 13 of 158 accepted
// orders - 8.2 % - across every archived run. So the aircraft could not shoot during 92 % of the
// time it WAS under a valid order, not merely during the gaps. `crank` is the sharpest case: the
// posture exists to support a shot already in the air, so an engage -> crank sequence meant one
// shot and then silence.
AIC_TEST(ReferenceScriptFiresUnderCrankAndUnderHoldButNeverUnderRtb) {
    std::string error;
    const std::string source = readReferenceScript(error);
    AIC_EXPECT_TRUE(error.empty(), error);

    struct Case {
        const char* posture;
        const char* roe;
        const char* target;      // "" means the order carries none.
        bool expectFire;
        const char* why;
    };
    const Case cases[] = {
        {"crank", "weaponsTight", "BANDIT_01", true,
         "crank exists to support a shot already in the air; a script that cannot fire in it turns "
         "engage -> crank into one shot and then silence"},
        {"hold", "weaponsFree", "", true,
         "hold under weaponsFree must let Tier 1 select and engage - this is exactly the authority "
         "an aircraft with NO commander already has, and AIC-VAL-2 rung 2 publishes hold, so a "
         "non-firing hold is the state 22 of 24 aircraft died in"},
        {"hold", "weaponsTight", "", false,
         "weaponsTight permits fire ONLY against the ordered target, and hold carries none"},
        {"ingress", "weaponsFree", "", true,
         "ingress under weaponsFree is a transit through contested air, not a ceasefire"},
        {"rtb", "weaponsFree", "", false,
         "rtb is the ONE posture that keeps its cease-fire - egressing while shooting is not "
         "egressing"},
        {"engage", "weaponsHold", "BANDIT_01", false,
         "weaponsHold forbids firing outright, whatever the posture"},
    };

    for (const Case& c : cases) {
        LuaHarness lua(std::string("ref-posture-") + c.posture + "-" + c.roe + "-"
                       + (c.target[0] == '\0' ? "notarget" : "target"));
        AIC_EXPECT_TRUE(lua.ok(), "the SDK's Lua eval runtime could not be created");
        AIC_LUA_OK(lua.eval(kStubWorld), "the stub world must load");
        AIC_LUA_OK(lua.eval(source), "the reference script must compile");

        std::ostringstream setup;
        setup << "placeContact('BANDIT_01', 25.0, 'Blue', 1, 2)\n"
                 "aiCommander = {\n"
                 "  requestCommand = function() return true end,\n"
                 "  isValid = function() return true end,\n"
                 "  getPosture = function() return '" << c.posture << "', '" << c.target
              << "', 300.0 end,\n"
                 "  getRoe = function() return '" << c.roe << "' end,\n"
                 "  getWaypoint = function() return 13.5, 144.8, 9000.0 end,\n"
                 "  getOrbitRadiusM = function() return 8000.0 end,\n"
                 "  getOrderSerial = function() return 1 end,\n"
                 "  reportTrack = function() return true end,\n"
                 "  reportLoadout = function() return true end,\n"
                 "  setSituationNote = function() return true end,\n"
                 "}\n"
                 "onInit('OWN'); onTick('OWN', 100.0, 0.05)";
        AIC_LUA_OK(lua.eval(setup.str()),
                   "driving one tick under posture " << c.posture << " / " << c.roe);

        const auto fired = lua.eval("return tostring(called('weapon.requestFire'))");
        AIC_LUA_OK(fired, "checking the launch under " << c.posture);
        AIC_EXPECT_EQ(unquote(fired.returnValue), std::string(c.expectFire ? "true" : "false"),
                      "posture '" << c.posture << "' under '" << c.roe << "': " << c.why);
    }
    return true;
}

// PRD v1.8.30, §Corrections item 46(c). Tier 1 must not report its own side.
//
// All five archived `fratricide` rejections named RedSAM_FireControlRadar, and because Stage B runs
// B3 (was it reported?) BEFORE B4 (is it friendly?), every one of them PASSED the reported-track
// check. That is a proof rather than an inference: the contact was in Tier 1's reported list. The
// team was available in Lua the whole time - the shipped script's own listHostileAirPlatforms reads
// it from entityControl.getEntityInfo - and this script fetched it and threw it away.
//
// This does NOT reopen C13, which refused filtering because it "would blind `defend`". That is true
// of MUNITIONS and does not apply to friendlies: nothing in `defend` requires visibility of a
// friendly SAM radar. The munition below must still be reported, and now carries kind='munition'
// so the model can tell it apart instead of being rejected for not knowing.
AIC_TEST(ReferenceScriptReportsHostilesAndMunitionsButNeverItsOwnSide) {
    std::string error;
    const std::string source = readReferenceScript(error);
    AIC_EXPECT_TRUE(error.empty(), error);

    LuaHarness lua("ref-reporting");
    AIC_EXPECT_TRUE(lua.ok(), "the SDK's Lua eval runtime could not be created");
    AIC_LUA_OK(lua.eval(kStubWorld), "the stub world must load");
    AIC_LUA_OK(lua.eval(source), "the reference script must compile");

    AIC_LUA_OK(lua.eval(
        "reported = {}\n"
        "placeContact('BANDIT_01', 25.0, 'Blue', 1, 2)\n"        // hostile aircraft
        "placeContact('BANDIT_wpn_9', 30.0, 'Blue', 2, 9)\n"     // hostile munition
        "placeContact('RedSAM_Radar', 40.0, 'Red', 1, 1)\n"      // OWN SIDE
        "aiCommander = {\n"
        "  requestCommand = function() return true end,\n"
        "  isValid = function() return false end,\n"
        "  reportLoadout = function() return true end,\n"
        "  setSituationNote = function() return true end,\n"
        "  reportTrack = function(_, id, r, s, kind, team)\n"
        "    reported[id] = (kind or '?') .. '/' .. (team or '?'); return true\n"
        "  end,\n"
        "}\n"
        "onInit('OWN'); onTick('OWN', 100.0, 0.05)"),
        "driving one tick with a hostile, a munition and a friendly in the picture");

    const auto bandit = lua.eval("return tostring(reported['BANDIT_01'])");
    AIC_LUA_OK(bandit, "checking the hostile aircraft");
    AIC_EXPECT_EQ(unquote(bandit.returnValue), std::string("air/hostile"),
                  "a hostile aircraft must be reported with both attributes populated");

    const auto munition = lua.eval("return tostring(reported['BANDIT_wpn_9'])");
    AIC_LUA_OK(munition, "checking the munition");
    AIC_EXPECT_EQ(unquote(munition.returnValue), std::string("munition/hostile"),
                  "the inbound munition must STAY in the picture - C13 established that filtering "
                  "it would blind `defend` - and must now be DISTINGUISHABLE, which is what 4 "
                  "targetClass and 5 track rejections were the model failing to do without it");

    const auto friendly = lua.eval("return tostring(reported['RedSAM_Radar'])");
    AIC_LUA_OK(friendly, "checking the friendly");
    AIC_EXPECT_EQ(unquote(friendly.returnValue), std::string("nil"),
                  "an own-team contact must NOT be reported at all. Five archived fratricide "
                  "rejections were this script handing the model its own side's SAM radar as an "
                  "indistinguishable contact and the model then being rejected for naming it");
    return true;
}

// AIC-ORD-2's winchester obligation, and the second thing that ran unconditionally in the shipped
// script and not at all in this one. An aircraft with empty rails used to set a situation note and
// wait for an order that might never come.
AIC_TEST(ReferenceScriptEgressesWhenWinchesterWithoutWaitingForAnOrder) {
    std::string error;
    const std::string source = readReferenceScript(error);
    AIC_EXPECT_TRUE(error.empty(), error);

    LuaHarness lua("ref-winchester");
    AIC_EXPECT_TRUE(lua.ok(), "the SDK's Lua eval runtime could not be created");
    AIC_LUA_OK(lua.eval(kStubWorld), "the stub world must load");
    AIC_LUA_OK(lua.eval(source), "the reference script must compile");

    AIC_LUA_OK(lua.eval(
        "aiCommander = nil\n"
        "world.ammo = { { hardpointName = 'R77_BVR', weaponProfileName = 'P', ammoCount = 0, ammoMax = 4 } }\n"
        "placeContact('BANDIT_01', 25.0, 'Blue', 1, 2)\n"
        "onInit('OWN'); onTick('OWN', 100.0, 0.05)"),
        "driving one tick on empty rails with a bandit in view");

    const auto egressed = lua.eval("return tostring(called('navigation.resumeWaypointFollowing'))");
    AIC_LUA_OK(egressed, "checking the egress");
    AIC_EXPECT_EQ(unquote(egressed.returnValue), std::string("true"),
                  "an aircraft with empty rails must egress rather than shadow the fight");

    const auto fired = lua.eval("return tostring(called('weapon.requestFire'))");
    AIC_LUA_OK(fired, "checking that nothing was fired from nowhere");
    AIC_EXPECT_EQ(unquote(fired.returnValue), std::string("false"),
                  "and it must not fire from a rail that carries nothing");
    return true;
}
