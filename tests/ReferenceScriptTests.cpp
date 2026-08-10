#include "TestSupport.h"

#include "CommanderConfig.h"

#include <core/scripting/ILuaEvalRuntime.h>
#include <core/scripting/LuaScriptRuntimeFactory.h>

#include <regex>

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
    -- NED velocity per entity, m/s. The default is the shipped scenario's spawn speed, so every
    -- test that is not ABOUT clause 8 drives an aircraft that is plainly flying and the recovery
    -- stays latched off. A nil entry means the runtime cannot report a velocity, which the script
    -- must read as "unknown", never as "stopped".
    velocity  = { OWN = {0.0, 220.0, 0.0} },
    tracks = {},                 -- { {id=, rangeM=, snrDb=}, ... }
    info = {},                   -- id -> { team=, entityTypeCode = { kind=, domain= } }
    closestByKind = {},          -- kind -> { id, rangeM }
    ammo = { { hardpointName = "R77_BVR", weaponProfileName = "P", ammoCount = 4, ammoMax = 4 },
             { hardpointName = "R73_IR",  weaponProfileName = "P", ammoCount = 2, ammoMax = 2 } },
}

-- The last call of each navigation verb, kept STRUCTURED as well as noted. `calls` answers "was
-- this verb reached"; these answer "with what geometry and what speed", which is the whole subject
-- of AIC-ORD-2 clauses 7 and 8 - a hold that reaches requestGoTo at 1.5 m/s is not a fix.
lastGoTo, lastTrack, lastHold = nil, nil, nil

-- Great-circle distance in metres, on the same spherical approximation the script uses, so an
-- orbit assertion compares against the arithmetic that produced the point rather than a second
-- opinion about the shape of the Earth.
function distanceM(latA, lonA, latB, lonB)
    local rad = math.pi / 180.0
    local phi1, phi2 = latA * rad, latB * rad
    local dPhi, dLambda = (latB - latA) * rad, (lonB - lonA) * rad
    local a = math.sin(dPhi / 2.0) ^ 2
        + math.cos(phi1) * math.cos(phi2) * math.sin(dLambda / 2.0) ^ 2
    return 2.0 * 6371000.0 * math.atan(math.sqrt(a), math.sqrt(1.0 - a))
end

mission = { log = function(m) note("mission.log %s", m) end,
            markRunning = function() end, markCompleted = function() note("markCompleted") end }

entityControl = {
    getPositionGeodetic = function(id)
        local p = world.positions[id]
        if p == nil then return nil end
        return p[1], p[2], p[3]
    end,
    getEntityInfo = function(id) return world.info[id] end,
    getVelocityNed = function(id)
        local v = world.velocity[id]
        if v == nil then return nil end
        return v[1], v[2], v[3]
    end,
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
        lastGoTo = { id = id, lat = lat, lon = lon, alt = alt, spd = spd }
        note("navigation.requestGoTo %s alt=%.0f spd=%.0f", id, alt or -1, spd or -1); return true
    end,
    requestTrackTarget = function(id, tgt, spd)
        lastTrack = { id = id, target = tgt, spd = spd }
        note("navigation.requestTrackTarget %s -> %s spd=%.0f", id, tgt, spd or -1); return true
    end,
    requestHoldPosition = function(id, lat, lon, alt, r, spd)
        lastHold = { id = id, lat = lat, lon = lon, alt = alt, r = r, spd = spd }
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

// =============================================================================================
// C23 - AIC-ORD-2 CLAUSES 7 AND 8 (PRD v1.8.36). See docs/c23-report.md for the evidence.
//
// WHAT THE DEFECT IS, IN THE ONE PARAGRAPH THESE TESTS EXIST TO PIN. The model reads the
// aircraft's own position out of the prompt and hands it back as the `hold` waypoint - 19 of 19
// archived hold orders were issued at 0.00 m from own position - and AIC-VAL-2 rung 2 synthesizes
// the same geometry by specification. Under that order the aircraft ranges out and comes back, and
// in the first 20 s sampling interval after it is INSIDE the ordered orbitRadiusM its speed
// collapses 320 -> ~128 -> exactly 1.5000 m/s and latches there for the rest of the run. Every
// subsequent order is then rejected for copying that speed back: 58 of the archive's 114
// rejections, 50.9 %. It survives rung 1's retention, rung 2's standing order, and rung 3's full
// release to this script - because the verb this script falls back to,
// `navigation.resumeWaypointFollowing`, TAKES NO SPEED ARGUMENT.
//
// WHY THESE ARE THE TESTS THAT WOULD HAVE CAUGHT IT AND NO EXISTING ONE DOES. AIC-VAL-2's
// "no error state and no stall" is satisfied in full by an aircraft parked at 1.5 m/s, which is
// what happened for 320-360 seconds in three consecutive runs while every published metric stayed
// green. The recorded stall geometry - velN 0.0544, velE 1.4990, velD 0.0 - is driven verbatim
// below, so these are regressions against a state the archive actually contains rather than
// against one imagined for the occasion.
// =============================================================================================

// The recorded stall: ||(0.0544, 1.4990, 0.0)|| = 1.4999 m/s, against safety.minSpeedMps = 50.
constexpr const char* kArchivedStallVelocity = "world.velocity.OWN = {0.0544, 1.4990, 0.0}\n";

// A commander publishing one order, with every getter the script touches. Postures that carry no
// waypoint still answer getWaypoint, exactly as the plugin does.
std::string commanderPublishing(const char* posture, const char* roe, const char* target,
                                const char* waypointLat, const char* waypointLon,
                                const char* orbitRadiusM, const char* cruiseSpeedMps) {
    std::ostringstream out;
    out << "aiCommander = {\n"
           "  requestCommand = function() return true end,\n"
           "  isValid = function() return true end,\n"
           "  getPosture = function() return '" << posture << "', '" << target << "', "
        << cruiseSpeedMps << " end,\n"
           "  getRoe = function() return '" << roe << "' end,\n"
           "  getWaypoint = function() return " << waypointLat << ", " << waypointLon
        << ", 9000.0 end,\n"
           "  getOrbitRadiusM = function() return " << orbitRadiusM << " end,\n"
           "  getOrderSerial = function() return 1 end,\n"
           "  reportTrack = function() return true end,\n"
           "  reportLoadout = function() return true end,\n"
           "  setSituationNote = function() return true end,\n"
           "}\n";
    return out.str();
}

// CLAUSE 7, THE ONSET. Tier 1 owns `hold`'s geometry when the ordered point is one the aircraft is
// already sitting inside.
//
// The order below is the archived one exactly: waypoint == own position, orbitRadiusM = 8000,
// cruiseSpeedMps = 320. Handing that to navigation.requestHoldPosition is what parked three
// aircraft; the script must instead fly an orbit it computed, AT THE ORDERED SPEED - the model
// still supplies where and how wide, and only the geometry moves to Tier 1.
AIC_TEST(ReferenceScriptFliesHoldItselfWhenTheOrderedPointIsInsideTheOrbit) {
    std::string error;
    const std::string source = readReferenceScript(error);
    AIC_EXPECT_TRUE(error.empty(), error);

    LuaHarness lua("ref-hold-inside");
    AIC_EXPECT_TRUE(lua.ok(), "the SDK's Lua eval runtime could not be created");
    AIC_LUA_OK(lua.eval(kStubWorld), "the stub world must load");
    AIC_LUA_OK(lua.eval(source), "the reference script must compile");

    AIC_LUA_OK(lua.eval(
        commanderPublishing("hold", "weaponsTight", "", "13.50", "144.80", "8000.0", "320.0")
        + "onInit('OWN'); onTick('OWN', 100.0, 0.05)"),
        "driving one tick under a hold ordered at the aircraft's own position");

    const auto handedOver = lua.eval("return tostring(called('navigation.requestHoldPosition'))");
    AIC_LUA_OK(handedOver, "checking that the ordered point was not handed straight to the engine");
    AIC_EXPECT_EQ(unquote(handedOver.returnValue), std::string("false"),
                  "AIC-ORD-2 clause 7: a hold whose waypoint lies INSIDE the ordered orbitRadiusM "
                  "of the current position must NOT be satisfied by navigation.requestHoldPosition. "
                  "That call is C23's onset - 19 of 19 archived holds were ordered at 0.00 m and "
                  "the aircraft's speed collapsed to 1.5000 m/s within one cadence window of being "
                  "inside the orbit");

    const auto flew = lua.eval("return tostring(called('navigation.requestGoTo'))");
    AIC_LUA_OK(flew, "checking that the script flew something");
    AIC_EXPECT_EQ(unquote(flew.returnValue), std::string("true"),
                  "and it must fly an orbit it computes itself rather than doing nothing - a hold "
                  "that commands no navigation is the same parked aircraft by another route");

    // ON the circle, not merely somewhere: the aim point must sit at the ORDERED radius from the
    // ORDERED centre. A point at an arbitrary distance would be a steer, not an orbit.
    const auto radius = lua.eval(
        "return string.format('%.0f', distanceM(13.50, 144.80, lastGoTo.lat, lastGoTo.lon))");
    AIC_LUA_OK(radius, "measuring the computed aim point against the ordered radius");
    AIC_EXPECT_EQ(unquote(radius.returnValue), std::string("8000"),
                  "the computed aim point must lie on the ORDERED orbit radius - the model supplies "
                  "where and how wide, and only the geometry is Tier 1's");

    // AT THE ORDERED SPEED. Clause 7 says "at the ordered cruiseSpeedMps"; substituting Tier 1's
    // own speed here would be clause 8 leaking into a case that is not a stall.
    const auto speed = lua.eval("return string.format('%.0f', lastGoTo.spd)");
    AIC_LUA_OK(speed, "checking the commanded speed");
    AIC_EXPECT_EQ(unquote(speed.returnValue), std::string("320"),
                  "clause 7 flies the orbit at the ORDERED cruise speed. This aircraft is not "
                  "stalled, so Tier 1 has no business overriding the speed it was given");
    return true;
}

// CLAUSE 7'S BOUNDARY, AND IT IS AS IMPORTANT AS THE CLAUSE.
//
// Outside the orbit there is somewhere to fly to, and the archive says that state is FINE: under a
// hold ordered 3.2-4.4 km away the aircraft sustained 320 m/s for up to sixty seconds with no decay
// at all, in every run. Clause 7 takes the geometry ONLY where the evidence puts the pathology, so
// this test pins what the fix deliberately does not touch. Without it, a later "simplification" to
// "Tier 1 always computes hold" would pass every other test in this file.
AIC_TEST(ReferenceScriptLeavesADistantHoldToTheEngineUntilItIsInsideTheOrbit) {
    std::string error;
    const std::string source = readReferenceScript(error);
    AIC_EXPECT_TRUE(error.empty(), error);

    LuaHarness lua("ref-hold-outside");
    AIC_EXPECT_TRUE(lua.ok(), "the SDK's Lua eval runtime could not be created");
    AIC_LUA_OK(lua.eval(kStubWorld), "the stub world must load");
    AIC_LUA_OK(lua.eval(source), "the reference script must compile");

    // Own is at 13.50/144.80; the hold centre is 0.50 deg north, ~55.6 km, against a 5 km radius.
    AIC_LUA_OK(lua.eval(
        commanderPublishing("hold", "weaponsTight", "", "14.00", "144.80", "5000.0", "320.0")
        + "onInit('OWN'); onTick('OWN', 100.0, 0.05)"),
        "driving one tick under a hold ordered 55 km away");

    const auto handedOver = lua.eval("return tostring(called('navigation.requestHoldPosition'))");
    AIC_LUA_OK(handedOver, "checking the outside-the-orbit case");
    AIC_EXPECT_EQ(unquote(handedOver.returnValue), std::string("true"),
                  "a hold ordered OUTSIDE the orbit radius must still go to the engine's hold "
                  "controller. Clause 7 is scoped to the case the archive indicts and no wider - "
                  "the aircraft held 320 m/s for up to sixty seconds in exactly this state");

    const auto radius = lua.eval("return string.format('%.0f', lastHold.r)");
    AIC_LUA_OK(radius, "checking the ordered radius was passed through");
    AIC_EXPECT_EQ(unquote(radius.returnValue), std::string("5000"),
                  "and the ordered radius must reach the engine unaltered");
    return true;
}

// CLAUSE 8, AND THIS IS THE REGRESSION TEST FOR C23 ITSELF.
//
// The aircraft is driven at the ARCHIVED stall velocity under every posture the script can be in,
// and under no order at all - which is the case that matters most, because the stall survived a
// full release to Tier 1 and twelve archived below-floor samples have no order in force.
//
// The assertion is not "some verb was called". It is that the aircraft was commanded a speed AT OR
// ABOVE Tier 1's own cruise value, by a verb that can carry one. Before this fix, the `hold` row
// reached navigation.requestHoldPosition and the no-order row reached
// navigation.resumeWaypointFollowing - which takes no speed argument at all - and both left the
// aircraft at 1.5000 m/s for the remaining 320-360 seconds of the run.
AIC_TEST(ReferenceScriptRecoversFromTheArchivedStallUnderEveryPostureAndUnderNone) {
    std::string error;
    const std::string source = readReferenceScript(error);
    AIC_EXPECT_TRUE(error.empty(), error);

    struct Case {
        const char* name;
        std::string setup;
        const char* why;
    };
    const Case cases[] = {
        {"no order at all (aiCommander nil - the rollback path)", "aiCommander = nil\n",
         "the DLL is deleted and the entity is Tier 1's alone"},
        {"released by the ladder (rung 3)",
         "aiCommander = { requestCommand = function() return true end,\n"
         "  isValid = function() return false end,\n"
         "  reportTrack = function() return true end,\n"
         "  reportLoadout = function() return true end,\n"
         "  setSituationNote = function() return true end }\n",
         "rung 3 released the entity and the script is on its own behaviour - twelve archived "
         "below-floor samples sit in exactly this state at exactly 1.5000 m/s"},
        {"hold at own position (the archived order)",
         commanderPublishing("hold", "weaponsFree", "", "13.50", "144.80", "8000.0", "1.4999"),
         "the order the model actually issued, with the speed it actually copied"},
        {"ingress",
         commanderPublishing("ingress", "weaponsFree", "", "13.50", "144.80", "0.0", "1.4999"),
         "ingress carries a waypoint and a copied speed like any other posture"},
        {"rtb",
         commanderPublishing("rtb", "weaponsFree", "", "13.50", "144.80", "0.0", "1.4999"),
         "rtb returns early with its cease-fire, and clause 8 says 'under any posture'"},
        {"defend",
         commanderPublishing("defend", "weaponsFree", "", "13.50", "144.80", "0.0", "1.4999"),
         "defend is the ONE posture that already satisfied this clause before it was written - the "
         "branch passes kDefendSpeedMps and discards the ordered speed outright - and it is also "
         "the only posture under which no archived aircraft ever went below the floor. It is here "
         "as a positive control: this row passed before the fix and must still pass after it"},
        {"engage with no target (commands no navigation at all)",
         commanderPublishing("engage", "weaponsFree", "", "13.50", "144.80", "0.0", "1.4999"),
         "this branch issues NO navigation verb when the target is empty, so only the backstop "
         "can reach it - a tick that commands nothing leaves a stalled aircraft stalled"},
        {"crank with no target (commands no navigation at all)",
         commanderPublishing("crank", "weaponsFree", "", "13.50", "144.80", "0.0", "1.4999"),
         "same branch shape as engage, and crank is the posture C21 showed nobody tests"},
    };

    for (const Case& c : cases) {
        LuaHarness lua(std::string("ref-c23-recover-") + c.name);
        AIC_EXPECT_TRUE(lua.ok(), "the SDK's Lua eval runtime could not be created");
        AIC_LUA_OK(lua.eval(kStubWorld), "the stub world must load");
        AIC_LUA_OK(lua.eval(source), "the reference script must compile");

        AIC_LUA_OK(lua.eval(kArchivedStallVelocity + c.setup
                            + "onInit('OWN'); onTick('OWN', 100.0, 0.05)"),
                   "driving one tick at the archived stall velocity under " << c.name);

        const auto commanded = lua.eval(
            "local best = -1\n"
            "if lastGoTo ~= nil and lastGoTo.spd ~= nil and lastGoTo.spd > best then "
            "best = lastGoTo.spd end\n"
            "if lastTrack ~= nil and lastTrack.spd ~= nil and lastTrack.spd > best then "
            "best = lastTrack.spd end\n"
            "return string.format('%.0f', best)");
        AIC_LUA_OK(commanded, "reading the highest commanded speed under " << c.name);
        const std::string best = unquote(commanded.returnValue);
        AIC_EXPECT_TRUE(best != "-1" && std::stod(best) >= 300.0,
                        "AIC-ORD-2 clause 8, case '"
                            << c.name << "': an entity below safety.minSpeedMps must be commanded "
                            << "back to Tier 1's OWN cruise value within one cadence window, and "
                            << "the highest speed any verb carried this tick was " << best
                            << " m/s. " << c.why);

        // AND IT MUST NOT BE THE TWO VERBS THAT CANNOT CARRY A SPEED. This is the half that makes
        // it a regression rather than a restatement: requestHoldPosition's speed was measured not
        // being honoured inside the orbit, and resumeWaypointFollowing has no speed parameter at
        // all, so reaching either one while stalled is the defect reproducing.
        const auto parked = lua.eval(
            "return tostring(called('navigation.resumeWaypointFollowing') "
            "or called('navigation.requestHoldPosition'))");
        AIC_LUA_OK(parked, "checking the speedless verbs under " << c.name);
        AIC_EXPECT_EQ(unquote(parked.returnValue), std::string("false"),
                      "case '"
                          << c.name
                          << "': a stalled aircraft must not be handed to resumeWaypointFollowing "
                             "(which takes NO speed argument) or to requestHoldPosition (whose "
                             "speed was measured not being honoured inside the orbit). Those two "
                             "verbs are how C23 survived all three rungs of AIC-VAL-2's ladder");
    }
    return true;
}

// AIC-VAL-2's standing requirement, applied to this fix: "no rung SHALL leave the entity less
// capable than the same entity with no commander installed."
//
// A recovery that seized the whole tick would be the cheapest implementation and would re-create
// the defect class C21 was: an aircraft that cannot shoot. Tier 1 takes the SPEED and nothing else,
// so the launch path is reached from a stalled aircraft exactly as it is from a flying one.
AIC_TEST(ReferenceScriptStillFiresWhileRecoveringFromAStall) {
    std::string error;
    const std::string source = readReferenceScript(error);
    AIC_EXPECT_TRUE(error.empty(), error);

    LuaHarness lua("ref-c23-still-fires");
    AIC_EXPECT_TRUE(lua.ok(), "the SDK's Lua eval runtime could not be created");
    AIC_LUA_OK(lua.eval(kStubWorld), "the stub world must load");
    AIC_LUA_OK(lua.eval(source), "the reference script must compile");

    AIC_LUA_OK(lua.eval(
        std::string(kArchivedStallVelocity)
        + "placeContact('BANDIT_01', 25.0, 'Blue', 1, 2)\n"
        + commanderPublishing("hold", "weaponsFree", "", "13.50", "144.80", "8000.0", "1.4999")
        + "onInit('OWN'); onTick('OWN', 100.0, 0.05)"),
        "driving one tick at the archived stall velocity with a bandit in range");

    const auto fired = lua.eval("return tostring(called('weapon.requestFire'))");
    AIC_LUA_OK(fired, "checking the launch path is still reachable while recovering");
    AIC_EXPECT_EQ(unquote(fired.returnValue), std::string("true"),
                  "recovering from a stall must not suppress the launch path. AIC-VAL-2 requires "
                  "that no rung leave the entity less capable than one with no commander at all, "
                  "and a fix for C23 that grounded the aircraft would breach that requirement "
                  "while closing this one - which is the shape of C21");
    return true;
}

// THE LATCH, AND WHY IT NEEDS TWO THRESHOLDS.
//
// Recovery entered at safety.minSpeedMps and left at safety.minSpeedMps would hand navigation back
// to the posture that stalled the aircraft the moment it crossed 50 m/s, and the aircraft would
// oscillate about the floor forever - satisfying the letter of clause 8 and none of its intent.
// So it clears at kResumeFlyingSpeedMps (100), not at the floor.
//
// THE UPPER THRESHOLD HAS A CEILING AS WELL AS A FLOOR, which a run had to find. It was 150 until
// v1.8.38, when the confirming probe measured a recovering aircraft settling at 132.2-146.5 m/s
// under a commanded 300 - so 150 sat inside the band and the latch might never have cleared,
// leaving Tier 1 holding navigation for the rest of the run. The 60 m/s case below is what pins
// the lower end of the band; nothing pins the upper end except that measurement, which is why the
// constant carries it in a comment rather than a number alone.
AIC_TEST(ReferenceScriptHoldsTheRecoveryUntilTheAircraftIsProperlyFlyingAgain) {
    std::string error;
    const std::string source = readReferenceScript(error);
    AIC_EXPECT_TRUE(error.empty(), error);

    LuaHarness lua("ref-c23-hysteresis");
    AIC_EXPECT_TRUE(lua.ok(), "the SDK's Lua eval runtime could not be created");
    AIC_LUA_OK(lua.eval(kStubWorld), "the stub world must load");
    AIC_LUA_OK(lua.eval(source), "the reference script must compile");

    const std::string commander =
        commanderPublishing("hold", "weaponsTight", "", "13.50", "144.80", "8000.0", "1.4999");

    // Tick 1: stalled. Tick 2: above the floor but not yet flying. Tick 3: at the LOWEST speed the
    // probe measured a recovering aircraft settle at - the latch must have cleared by here, or it
    // may never clear at all. Tick 4: properly flying.
    AIC_LUA_OK(lua.eval(std::string(kArchivedStallVelocity) + commander
                        + "onInit('OWN'); onTick('OWN', 100.0, 0.05)\n"
                          "world.velocity.OWN = {0.0, 60.0, 0.0}\n"
                          "onTick('OWN', 100.05, 0.05)\n"
                          "midSpd = lastGoTo.spd\n"
                          "world.velocity.OWN = {0.0, 132.2, 0.0}\n"
                          "onTick('OWN', 100.10, 0.05)\n"
                          "settleSpd = lastGoTo.spd\n"
                          "world.velocity.OWN = {0.0, 200.0, 0.0}\n"
                          "calls = {}; lastGoTo = nil\n"
                          "onTick('OWN', 100.15, 0.05)"),
               "driving four ticks across the recovery band");

    const auto mid = lua.eval("return string.format('%.0f', midSpd)");
    AIC_LUA_OK(mid, "checking the speed commanded at 60 m/s");
    AIC_EXPECT_EQ(unquote(mid.returnValue), std::string("300"),
                  "at 60 m/s the aircraft is above safety.minSpeedMps and still not flying. "
                  "Releasing the recovery here hands navigation straight back to the hold that "
                  "stalled it, and the aircraft oscillates about the floor instead of recovering");

    // THE CEILING ON THE THRESHOLD, and it is the half a run had to supply. 132.2 m/s is the lowest
    // speed the confirming probe observed a recovering aircraft settle at under a commanded 300. If
    // kResumeFlyingSpeedMps is ever raised above that, the latch stops clearing in normal flight and
    // Tier 1 keeps navigation for the rest of the run - `hold` never orbits and
    // `resumeWaypointFollowing` never runs. That is what the 150 this replaced would have done.
    const auto settled = lua.eval("return string.format('%.0f', settleSpd)");
    AIC_LUA_OK(settled, "checking the speed commanded at the measured settling floor");
    AIC_EXPECT_EQ(unquote(settled.returnValue), std::string("1"),
                  "at 132.2 m/s - the lowest speed a recovering aircraft was measured settling at - "
                  "the recovery MUST have cleared and the ordered speed must be back in force. A "
                  "resume threshold above the settling band is a latch that never opens");

    const auto released = lua.eval("return string.format('%.0f', lastGoTo.spd)");
    AIC_LUA_OK(released, "checking the speed commanded at 200 m/s");
    AIC_EXPECT_EQ(unquote(released.returnValue), std::string("1"),
                  "at 200 m/s the recovery must have cleared and the ORDERED speed must be back in "
                  "force - 1.4999 m/s, which Stage B's safety.minSpeedMps floor would never let "
                  "through in production and is used here precisely because no other value "
                  "distinguishes 'the order's speed' from 'Tier 1's'. Tier 1 takes the speed only "
                  "while the aircraft is not flying; it does not acquire it permanently");
    return true;
}

// UNKNOWN IS NOT STOPPED.
//
// entityControl.getVelocityNed can return a nil triplet - the stub documents it - and a release
// tree could in principle lack the verb entirely. Either way the script must behave exactly as it
// did before v1.8.36 rather than concluding that an aircraft it cannot measure has stopped: a
// recovery that fires on every entity whose velocity is unreadable would seize navigation from a
// fully serviceable aircraft, which is a worse defect than the one it is here to fix.
// THE ONE PLACE A TEXT CHECK IS THE RIGHT INSTRUMENT, AND IT IS WORTH SAYING WHY.
//
// Every other test in this file asserts on WHAT THE SCRIPT DID, because a grep passes on a script
// refactored into a shape that no longer works. Here the literal value IS the subject: the script
// carries its own speed floor as a constant, and `safety.minSpeedMps` carries the same quantity in
// C++, and NOTHING LINKS THEM. The `aiCommander` namespace does not publish the bound, and adding a
// getter is new API surface that needs a PRD revision (docs/c23-report.md section 7.3, item 3).
//
// So the two numbers are pinned to each other instead. The failure this catches is the one
// `CommanderConfig.h` explicitly invites: "A DEPLOYMENT COMMANDING ROTARY-WING OR LOITERING
// PLATFORMS MUST LOWER IT" - an operator does exactly that, and the reference script goes on
// recovering at 50 m/s because nobody remembered it had its own copy. That is §Corrections item
// 50(e)'s failure mode - a value that agrees by convention until it quietly does not - and the
// remedy this project has used successfully three times is a pin that fails the build.
AIC_TEST(ReferenceScriptSpeedFloorAgreesWithTheShippedSafetyBound) {
    std::string error;
    const std::string source = readReferenceScript(error);
    AIC_EXPECT_TRUE(error.empty(), error);

    const auto readConstant = [&source](const char* name, double& out) {
        const std::regex pattern(std::string("local\\s+") + name + "\\s*=\\s*([0-9]+\\.?[0-9]*)");
        std::smatch match;
        if (!std::regex_search(source, match, pattern)) {
            return false;
        }
        out = std::stod(match[1].str());
        return true;
    };

    double scriptFloor = -1.0;
    AIC_EXPECT_TRUE(readConstant("kMinFlyingSpeedMps", scriptFloor),
                    "the reference script must declare kMinFlyingSpeedMps as a literal - if it has "
                    "been renamed or computed, this pin is silently measuring nothing and the "
                    "rename is the change that needs reviewing");

    const arkheon::aicommander::CommanderConfig shipped;
    AIC_EXPECT_EQ(scriptFloor, shipped.minSpeedMps,
                  "lua/ai_commander_interceptor.lua's kMinFlyingSpeedMps ("
                      << scriptFloor << ") must equal the shipped safety.minSpeedMps default ("
                      << shipped.minSpeedMps
                      << "). They are the same physical quantity held in two places with no "
                         "mechanical link between them, and CommanderConfig.h instructs rotary-wing "
                         "deployments to lower the C++ one. If you changed the bound on purpose, "
                         "change BOTH and say so in the PRD");

    // The recovery threshold has no C++ counterpart, so what is pinned is the RELATIONSHIP: it must
    // sit above the floor (or the latch chatters) and below 132.2 m/s, the lowest speed the v1.8.38
    // probe measured a recovering aircraft settle at (or the latch never clears at all).
    double resumeAt = -1.0;
    AIC_EXPECT_TRUE(readConstant("kResumeFlyingSpeedMps", resumeAt),
                    "the reference script must declare kResumeFlyingSpeedMps as a literal");
    AIC_EXPECT_TRUE(resumeAt > scriptFloor && resumeAt < 132.2,
                    "kResumeFlyingSpeedMps ("
                        << resumeAt << ") must sit strictly between the floor (" << scriptFloor
                        << ") and 132.2 m/s. Below the floor the latch is meaningless; at or above "
                           "132.2 it is inside the band a recovering aircraft was measured settling "
                           "in, and Tier 1 would hold navigation for the rest of the run. The "
                           "value shipped at 150.0 until a run caught exactly that");
    return true;
}

AIC_TEST(ReferenceScriptDoesNotInventAStallWhenTheVelocityIsUnreadable) {
    std::string error;
    const std::string source = readReferenceScript(error);
    AIC_EXPECT_TRUE(error.empty(), error);

    LuaHarness lua("ref-c23-unknown-velocity");
    AIC_EXPECT_TRUE(lua.ok(), "the SDK's Lua eval runtime could not be created");
    AIC_LUA_OK(lua.eval(kStubWorld), "the stub world must load");
    AIC_LUA_OK(lua.eval(source), "the reference script must compile");

    AIC_LUA_OK(lua.eval(
        "world.velocity.OWN = nil\n"          // a nil triplet, as the stub documents
        "entityControl.getVelocityNed = nil\n" // and the verb missing entirely
        "aiCommander = nil\n"
        "onInit('OWN'); onTick('OWN', 100.0, 0.05)"),
        "driving one tick with no readable velocity and no commander");

    const auto resumed = lua.eval("return tostring(called('navigation.resumeWaypointFollowing'))");
    AIC_LUA_OK(resumed, "checking the unmeasurable case");
    AIC_EXPECT_EQ(unquote(resumed.returnValue), std::string("true"),
                  "with no readable velocity the script must fall back to the behaviour that "
                  "shipped before clause 8 existed. An unmeasurable aircraft is not a stalled one, "
                  "and a safety net that fires on absence of evidence is not a safety net");
    return true;
}
