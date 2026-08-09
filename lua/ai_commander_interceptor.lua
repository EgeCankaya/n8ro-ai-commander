-- Reference Tier-1 behaviour for the N8RO AI Entity Commander (AIC-ORD-2).
--
-- This is the deterministic half of the three-tier split. The commander (Tier 2) decides POSTURE,
-- TARGET, WAYPOINT and ROE; this script decides everything else -- pursuit geometry, crank offset,
-- defensive pump, launch discipline, target selection when none is ordered -- and it is the only
-- thing that ever calls a request* verb.
--
-- THE SHAPE OF THIS SCRIPT CHANGED IN PRD v1.8.30, AND THE REASON IS WORTH READING BEFORE EDITING.
-- The previous version was a parallel implementation that reproduced SOME of the shipped
-- oppint_red_interceptor.lua's behaviour and reached its fire logic from ONE of six postures. A
-- deployment A/B over twelve paired runs measured what that cost: 4 launches against the shipped
-- script's 32, 0 kills against 0, and 22 of 24 commanded aircraft destroyed against 0 of 24
-- (§Corrections items 45 and 46). Four mechanisms, none of which was the language model:
--
--   1. fallBackToWaypoints returned before any fire logic, so an aircraft with no order in force
--      did not shoot at all -- every gap between orders, every rejection, every timeout, and the
--      whole run if the backend was down.
--   2. considerFiring was reachable only from `engage`, which is 8.2% of every order this system
--      has ever accepted. `crank` -- the posture that exists to support a shot already in the air
--      -- could not fire, so an engage->crank sequence meant one shot and then silence.
--   3. All three rungs of AIC-VAL-2's ladder are non-fighting, rung 2's standing order being
--      `hold` by specification. Fixing (1) alone would have left (2) and (3) intact.
--   4. THERE WAS NO AUTOMATIC DEFENSIVE REFLEX. The shipped script's first action every tick is a
--      munition-defeat check inside 15 km; this script flew `defend` only when ordered, on a 20 s
--      cadence plus a measured p50 of 3.5 s. An AMRAAM crosses 15 km in about fifteen seconds.
--      §Non-goals assigns missile defeat to Tier 0/1 "where it already works" -- and Tier 1 had
--      not implemented it.
--
-- So this script is now THE SHIPPED SCRIPT'S BEHAVIOUR PLUS AN ORDER-OVERRIDE LAYER, which is the
-- structure AIC-ORD-2 requires. That is not a retreat from ADR-1's tiering claim; it is the strong
-- form of it. Confining the model to intent is only defensible if the deterministic tier keeps
-- working when the model is silent.
--
-- Tick order, and every step before step 5 runs WHETHER OR NOT a commander exists:
--   0. MEASURE. Is this aircraft still flying? Pure observation, before any navigation, so every
--      verb issued below goes out under the right authority (AIC-ORD-2 clause 8, C23).
--   1. REPORT what this aircraft can see (tracks and stores), when a commander is present. Pure
--      observation, no navigation -- so it happens even on the defensive path below, because the
--      model most needs the picture on the tick the aircraft is being shot at.
--   2. SURVIVE. A munition inside the threat range overrides every other consideration including
--      an order. No order, and no absence of an order, may suppress it.
--   3. EGRESS when winchester. An aircraft with empty rails does not shadow a fight.
--   4. Read the published order, if one is in force.
--   5. Execute it -- and the launch path is reachable from every posture except `rtb`.
--   6. With NO order in force, fall through to this script's own deconflicted target selection,
--      not to waypoint following alone.
--
-- It degrades to ordinary waypoint following when `aiCommander` is nil (plugin not deployed) or
-- when the commander has released this entity -- but it degrades to waypoint following WHILE STILL
-- FIGHTING. That path is not an afterthought: it is how a rollback works, and "deleting one DLL
-- must leave a script that still flies" was satisfied to the letter and not in substance, because
-- it still flew and it died.
--
-- Posture -> verb mapping (AIC-ORD-2):
--   ingress -> navigation.requestGoTo(id, lat, lon, alt, speed)
--   engage  -> navigation.requestTrackTarget(id, targetId, speed) + weapon.requestFire, spaced by
--              an assessment window. NOT weapon.canFire -- see considerFiring.
--   crank   -> navigation.requestGoTo(...) with a SCRIPT-computed offset steer point
--   defend  -> navigation.requestGoTo(...) with a SCRIPT-computed cold point
--   hold    -> navigation.requestHoldPosition WHILE OUTSIDE the ordered orbit; a SCRIPT-computed
--              orbit once inside it (AIC-ORD-2 clause 7 -- see flyHold)
--   rtb     -> navigation.requestGoTo(...) + weapon.requestCeaseFire

local kCrankOffsetDeg   = 40.0     -- Off the own->target bearing while a shot is assessed.
local kManeuverLegM     = 25000.0  -- Length of a computed steer leg.
local kDefendAltM       = 3000.0   -- Descend while defending.
local kDefendSpeedMps   = 320.0
local kEngageSpeedMps   = 300.0    -- Used only when no order supplies a speed.
local kEarthRadiusM     = 6371000.0

-- AIC-ORD-2 CLAUSE 7 (PRD v1.8.36, C23). How far around the orbit the script aims when it is flying
-- the hold itself. A LEAD ANGLE off our own angular position on the circle, recomputed from the
-- current position every tick -- so the aim point advances only as the aircraft advances, and the
-- orbit is stable. An accumulating angle would spin the aim point at the tick rate instead.
--
-- Every aircraft orbits the same way round. Crank deliberately splits the section left and right
-- for lateral separation; an orbit is the opposite case, where two aircraft turning opposite ways
-- about one point meet head-on twice per circuit.
local kOrbitLeadDeg = 30.0

-- AIC-ORD-2 CLAUSE 8 (PRD v1.8.36, C23). THE SPEED FLOOR, AND TIER 1'S OWN RECOVERY SPEED.
--
-- WHAT THIS IS FOR. Three archived runs put RedSu35_02 at exactly 1.5000 m/s for 320-360 seconds
-- apiece -- 58 archived orders were rejected for copying that speed back -- and the aircraft did
-- not recover when the commander retained its order, did not recover when the ladder published a
-- standing order, and did not recover when the ladder released it to this script entirely. The
-- release path is the reason: `navigation.resumeWaypointFollowing` TAKES NO SPEED ARGUMENT, so the
-- verb this script falls back to cannot command an aircraft to fly faster than it currently is.
--
-- kRecoverSpeedMps IS TIER 1'S OWN VALUE AND IS NEVER READ FROM AN ORDER. That is the whole of the
-- clause: across every accepted order in the archive that can be checked against the snapshot it
-- was answering, `cruiseSpeedMps` equalled `own.speedMps` to the last bit -- 61 of 61, on all four
-- postures that carry a speed. An order's speed is the aircraft's own current speed handed back to
-- it, so it can re-command a stall and can never break one.
local kRecoverSpeedMps      = 300.0   -- == kEngageSpeedMps, and for a different reason.

-- Below this the aircraft is not flying, whatever any order says.
--
-- IT IS THIS SCRIPT'S FLOOR, NOT THE PLUGIN'S. The `aiCommander` namespace does not publish
-- `safety.minSpeedMps`, and it should not be needed: a recovery that only triggered where the
-- validator would have rejected an order would not trigger at all with the commander absent, which
-- is where 12 of the 58 archived below-floor samples sit. 50.0 is the shipped `safety.minSpeedMps`
-- default and is chosen to agree with it, NOT derived from it -- an operator who lowers that bound
-- for rotary-wing or loitering platforms must lower this constant too.
local kMinFlyingSpeedMps    = 50.0

-- Hysteresis, and it is load-bearing rather than tidiness. Recovery owns navigation until the
-- aircraft is properly flying again, not merely one metre per second above the floor. With a single
-- threshold the aircraft crosses it, hands navigation straight back to the posture that stalled it,
-- decays, and crosses again -- satisfying the letter of clause 8 and none of its intent.
--
-- THE VALUE WAS 150.0 AND A RUN SAID NO (PRD v1.8.38, Corrections item 54(d)). It was chosen as
-- "half of Tier 1's cruise" with nothing behind it. The confirming probe then measured what an
-- aircraft under a commanded 300 m/s actually does: it reaches 299.98 in about twenty seconds and
-- then SETTLES AT 132.2-146.5 m/s, on both aircraft, and the archive says the same thing from the
-- other direction -- `RedSu35_01` flew `defend (ordered)` under a commanded 320 for 490 s and held
-- about 150. So 150.0 sat INSIDE the band the aircraft settles in, and the latch it guards might
-- never have cleared: Tier 1 would have kept navigation for the rest of the run, `hold` would never
-- have orbited and `resumeWaypointFollowing` would never have run.
--
-- 100.0 is 2x the floor -- far enough above it that the entry/exit pair cannot chatter -- and ~24 %
-- below the lowest speed the probe observed a recovering aircraft settle at. Like
-- `safety.minSpeedMps`, it is a policy choice rather than a derivation; unlike the 150 it replaces,
-- it has a measurement on both sides of it.
local kResumeFlyingSpeedMps = 100.0

-- Hardpoint engagement priority, longest reach first, matching the shipped script's table. The
-- disciplined launch range is engageRangeM * kLaunchRangeFrac -- a shot at the very edge of the
-- envelope is a wasted shot.
--
-- THIS REPLACED A `firstLoadedHardpoint` THAT IGNORED RANGE ENTIRELY (v1.8.30). It picked the
-- first rail with ammunition and checked it against a nominal kMissileRangeM of 60,000 m -- a
-- figure that matches NO rail in the scenario, where R77_BVR reaches 45,000 m and R73_IR 12,000 m.
-- A short-range infrared missile could therefore be launched at four times its kinematic reach.
-- The four archived commanded launches all fell at 17.6-27.0 km and none exercised it, so it was a
-- latent defect rather than an observed one; it is fixed here with the rest.
local HARDPOINTS = {
    { name = "R77_BVR", engageRangeM = 45000.0 },  -- beyond-visual-range AAM
    { name = "R73_IR",  engageRangeM = 12000.0 },  -- short-range IR AAM (WVR)
}
local kLaunchRangeFrac = 0.8

-- Defensive pump trigger (v1.8.30). A hostile MUNITION-kind track inside this range turns the
-- aircraft cold and descends it. The kind filter is load-bearing: the previous version's `defend`
-- fled `getClosestHostileTrackById(entityId)` with no filter at all -- the nearest hostile
-- ANYTHING, which in a BVR merge is an aircraft rather than the missile -- so even an ordered
-- `defend` could turn the aircraft cold from the wrong object.
local kThreatRangeM = 15000.0

-- DIS entity kind and domain (SISO-REF-010), as the shipped script reads them. Platforms are
-- kind 1 and are engaged; munitions are kind 2 and are the inbound-threat cue. Domains: land 1,
-- air 2, surface 3.
local DIS_KIND_PLATFORM = 1
local DIS_KIND_MUNITION = 2
local DIS_DOMAIN_LAND    = 1
local DIS_DOMAIN_AIR     = 2
local DIS_DOMAIN_SURFACE = 3

-- Launch spacing (AIC-ORD-2, v1.8.21). Assessment window = time of flight + margin, clamped --
-- the same sizing the shipped oppint_red_interceptor.lua uses, against the same medium-range AAM
-- archetype (~900 m/s averaged over a BVR shot). Without this the engage posture fires every
-- tick: the canFire precondition this script used to carry never returned true, so it stood in
-- for shot spacing that was never written.
local kMissileAvgSpeedMps = 900.0
local kAssessMarginS      = 5.0
local kAssessMinS         = 10.0
local kAssessMaxS         = 30.0

local state = {}

local function ensureState(entityId)
    if state[entityId] == nil then
        state[entityId] = {
            mode = "", enrolled = false, lastSerial = -1, nextFireTime = 0.0,
            team = nil, winchester = false,
            -- AIC-ORD-2 clause 8. `recovering` latches: it is set when the aircraft drops below
            -- kMinFlyingSpeedMps and cleared only at kResumeFlyingSpeedMps. `navigatedAtS` is the
            -- simulation time of the last navigation verb this script issued for this entity, and
            -- exists so a tick that commands nothing cannot leave a stalled aircraft stalled.
            recovering = false, speedMps = nil, navigatedAtS = nil, tickTimeS = nil,
        }
    end
    return state[entityId]
end

local function log(message)
    if mission ~= nil and mission.log ~= nil then
        mission.log(message)
    end
end

local function clamp(value, minValue, maxValue)
    return math.max(minValue, math.min(maxValue, value))
end

-- Great-circle bearing and distance. Spherical approximation, matching the idiom the shipped
-- oppint_red_interceptor.lua already uses -- this is steer-point arithmetic, not navigation.
local function bearingAndDistance(latA, lonA, latB, lonB)
    local rad = math.pi / 180.0
    local phi1, phi2 = latA * rad, latB * rad
    local dLambda = (lonB - lonA) * rad
    local y = math.sin(dLambda) * math.cos(phi2)
    local x = math.cos(phi1) * math.sin(phi2) - math.sin(phi1) * math.cos(phi2) * math.cos(dLambda)
    local bearing = (math.atan(y, x) / rad + 360.0) % 360.0

    local dPhi = (latB - latA) * rad
    local a = math.sin(dPhi / 2.0) ^ 2
        + math.cos(phi1) * math.cos(phi2) * math.sin(dLambda / 2.0) ^ 2
    local distance = 2.0 * kEarthRadiusM * math.atan(math.sqrt(a), math.sqrt(1.0 - a))
    return bearing, distance
end

local function pointAtBearing(latDeg, lonDeg, bearingDeg, distanceM)
    local rad = math.pi / 180.0
    local phi1, lambda1 = latDeg * rad, lonDeg * rad
    local theta, delta = bearingDeg * rad, distanceM / kEarthRadiusM
    local phi2 = math.asin(math.sin(phi1) * math.cos(delta)
        + math.cos(phi1) * math.sin(delta) * math.cos(theta))
    local lambda2 = lambda1 + math.atan(
        math.sin(theta) * math.sin(delta) * math.cos(phi1),
        math.cos(delta) - math.sin(phi1) * math.sin(phi2))
    return phi2 / rad, ((lambda2 / rad + 540.0) % 360.0) - 180.0
end

local function setMode(s, entityId, mode, detail)
    if s.mode ~= mode then
        s.mode = mode
        log(entityId .. " -> " .. mode .. (detail and (" (" .. detail .. ")") or ""))
    end
end

local function positionOf(entityId)
    if entityControl == nil or entityControl.getPositionGeodetic == nil then
        return nil
    end
    return entityControl.getPositionGeodetic(entityId)
end

-- ---------------------------------------------------------------------------------------------
-- AIC-ORD-2 clause 8: is this aircraft still flying, and if not, whose problem is it?
-- ---------------------------------------------------------------------------------------------

-- NED velocity, m/s, or nil when the runtime cannot supply it. `entityControl.getVelocityNed` is on
-- the shipped stub surface and this script did not call it until v1.8.36; a release tree that
-- somehow lacks it leaves every check below inert rather than failing, which is the right direction
-- for a safety net -- it degrades to the behaviour that shipped before it existed.
local function velocityNedOf(entityId)
    if entityControl == nil or entityControl.getVelocityNed == nil then
        return nil
    end
    local velN, velE, velD = entityControl.getVelocityNed(entityId)
    if velN == nil or velE == nil or velD == nil then
        return nil
    end
    return velN, velE, velD
end

-- Ground speed as a MAGNITUDE, m/s: ||(velN, velE, velD)||.
--
-- Deliberately the same definition the plugin uses for `own.speedMps` (Snapshot.h, groundSpeedMps),
-- so this script's floor and Stage B's `safety.minSpeedMps` bound describe the SAME QUANTITY. A
-- script that measured horizontal speed while the validator measured the full norm would disagree
-- with it in a descent, and the two numbers would have to be reconciled by a reader every time.
--
-- Returns nil when the velocity is unavailable. UNKNOWN IS NOT STOPPED: inventing a stall would
-- hand navigation to the recovery on an aircraft that is flying perfectly well.
local function groundSpeedOf(entityId)
    local velN, velE, velD = velocityNedOf(entityId)
    if velN == nil then
        return nil
    end
    return math.sqrt(velN * velN + velE * velE + velD * velD)
end

-- Course over ground, degrees clockwise from true north. Matches the plugin's courseOverGroundDeg,
-- INCLUDING its convention that a stationary entity returns 0.0 rather than a sentinel: at 1.5 m/s
-- the course is nearly meaningless, and the recovery below needs a direction rather than a good one.
local function courseOverGroundDeg(velN, velE)
    if math.abs(velN) < 1e-9 and math.abs(velE) < 1e-9 then
        return 0.0
    end
    return (math.atan(velE, velN) * 180.0 / math.pi + 360.0) % 360.0
end

-- The leg Tier 1 flies to get an aircraft moving again: straight ahead, level, at kRecoverSpeedMps.
-- Straight ahead rather than anywhere in particular because the recovery is not a tactic -- it is
-- the precondition for having one, and a stopped aircraft has no tactical geometry to preserve.
local function recoveryLeg(entityId)
    local ownLat, ownLon, ownAlt = positionOf(entityId)
    if ownLat == nil then
        return nil
    end
    local velN, velE = velocityNedOf(entityId)
    local bearing = 0.0
    if velN ~= nil then
        bearing = courseOverGroundDeg(velN, velE)
    end
    local lat, lon = pointAtBearing(ownLat, ownLon, bearing, kManeuverLegM)
    return lat, lon, ownAlt
end

local function isRecovering(entityId)
    local s = state[entityId]
    return s ~= nil and s.recovering == true
end

-- ---------------------------------------------------------------------------------------------
-- EVERY navigation call in this script goes through these four wrappers, and that is the whole of
-- clause 8's enforcement.
--
-- WHY A WRAPPER RATHER THAN A CHECK AT EACH SITE. The clause reads "SHALL NOT leave an entity below
-- safety.minSpeedMps for longer than one cadence window, under any posture and whether or not an
-- order is in force". That is a property of the WHOLE FILE. There are eleven navigation call sites
-- across six postures, the winchester egress, the defensive reflex and the no-order path; enforced
-- by eleven guards it would be one forgotten guard away from being false again, and §Corrections
-- item 50(e) is this project's record of exactly that failure mode surviving four revisions.
--
-- WHAT RECOVERY CHANGES, AND WHAT IT DELIBERATELY DOES NOT. The posture still decides WHERE and
-- WHAT; Tier 1 decides only HOW FAST, and only while the aircraft is below flying speed. It is
-- never a REDUCTION -- math.max, so the defensive pump's 320 m/s is not pulled down to 300 -- and
-- it suppresses no shot. AIC-VAL-2 requires that no rung leave the entity less capable than the
-- same entity with no commander installed, and a recovery that also grounded the launch path would
-- breach that requirement while fixing this one.
-- ---------------------------------------------------------------------------------------------

local function markNavigated(entityId)
    local s = state[entityId]
    if s ~= nil then
        s.navigatedAtS = s.tickTimeS
    end
end

local function navGoTo(entityId, lat, lon, alt, speedMps)
    if navigation == nil or navigation.requestGoTo == nil then
        return false
    end
    local commandedMps = speedMps or 0.0
    if isRecovering(entityId) then
        commandedMps = math.max(commandedMps, kRecoverSpeedMps)
    end
    markNavigated(entityId)
    return navigation.requestGoTo(entityId, lat, lon, alt, commandedMps) and true or false
end

local function navTrackTarget(entityId, targetId, speedMps)
    if navigation == nil or navigation.requestTrackTarget == nil then
        return false
    end
    local commandedMps = speedMps or 0.0
    if isRecovering(entityId) then
        commandedMps = math.max(commandedMps, kRecoverSpeedMps)
    end
    markNavigated(entityId)
    return navigation.requestTrackTarget(entityId, targetId, commandedMps) and true or false
end

-- THE TWO VERBS THAT CANNOT CARRY TIER 1'S SPEED ARE THE TWO THAT STALLED THE AIRCRAFT.
--
-- `resumeWaypointFollowing` takes no speed argument at all, and `requestHoldPosition`'s speed was
-- measured not being honoured once the aircraft was inside the ordered orbit. So while recovering,
-- both are replaced outright by the recovery leg rather than being issued with a better argument:
-- there is no better argument to give them.
local function navHoldPosition(entityId, lat, lon, alt, radiusM, speedMps)
    if isRecovering(entityId) then
        local lat2, lon2, alt2 = recoveryLeg(entityId)
        if lat2 ~= nil then
            return navGoTo(entityId, lat2, lon2, alt2, kRecoverSpeedMps)
        end
    end
    if navigation == nil or navigation.requestHoldPosition == nil then
        return false
    end
    markNavigated(entityId)
    return navigation.requestHoldPosition(entityId, lat, lon, alt, radiusM, speedMps) and true
        or false
end

local function navResume(entityId)
    if isRecovering(entityId) then
        local lat, lon, alt = recoveryLeg(entityId)
        if lat ~= nil then
            return navGoTo(entityId, lat, lon, alt, kRecoverSpeedMps)
        end
    end
    if navigation == nil or navigation.resumeWaypointFollowing == nil then
        return false
    end
    markNavigated(entityId)
    return navigation.resumeWaypointFollowing(entityId) and true or false
end

-- Reads the speed and moves the latch. Pure observation -- it issues no verb -- so it runs first
-- every tick, before the survival reflex and before the order is read.
local function updateFlyingSpeed(entityId, s)
    local speedMps = groundSpeedOf(entityId)
    if speedMps == nil then
        return
    end
    s.speedMps = speedMps
    if speedMps < kMinFlyingSpeedMps then
        if not s.recovering then
            s.recovering = true
            log(string.format("%s below flying speed at %.4f m/s; Tier 1 takes navigation",
                entityId, speedMps))
        end
    elseif s.recovering and speedMps >= kResumeFlyingSpeedMps then
        s.recovering = false
        log(string.format("%s recovered to %.1f m/s; navigation returns to the posture",
            entityId, speedMps))
    end
end

-- Clause 8's backstop, for the branches that legitimately command no navigation at all -- `engage`
-- and `crank` both issue nothing when the ordered target is empty. A tick that commands nothing
-- leaves a stalled aircraft stalled, and "under any posture" includes the postures that decline to
-- steer. Called once, where every non-returning path converges.
local function ensureRecoveryNavigated(entityId, s)
    if not s.recovering or s.navigatedAtS == s.tickTimeS then
        return
    end
    local lat, lon, alt = recoveryLeg(entityId)
    if lat ~= nil then
        navGoTo(entityId, lat, lon, alt, kRecoverSpeedMps)
    end
end

-- Own team, read once per aircraft. Unlike the shipped script this carries NO scenario-specific
-- fallback side: a reference script that guessed "Red" would silently mis-classify every contact
-- in any scenario that is not this one. An unknown own-team simply means no contact can be proven
-- friendly, which the classifier below reports honestly as `unknown`.
local function ownTeamOf(entityId, s)
    if s.team == nil then
        s.team = ""
        if entityControl ~= nil and entityControl.getEntityInfo ~= nil then
            local info = entityControl.getEntityInfo(entityId)
            if info ~= nil and info.team ~= nil then
                s.team = info.team
            end
        end
    end
    return s.team
end

-- ---------------------------------------------------------------------------------------------
-- Obligation 1: report what this aircraft sees.
-- ---------------------------------------------------------------------------------------------

-- Maps one contact to the closed vocabularies aiCommander.reportTrack accepts (AIC-API-1,
-- v1.8.30). Returns kind, team, isOwnTeam.
--
-- WHY THIS EXISTS. The prompt used to carry {id, rangeM, snrDb} per track while §Doctrine told the
-- model "do not infer from an id what the id does not say" -- and Stage B then rejected the model
-- for failing to discriminate. 14 of 55 archived rejections were exactly that: engaging inbound
-- munitions (4), naming munitions that had already detonated (5), and engaging its own side's SAM
-- radar (5). The information was in Lua the whole time; this script fetched it and threw it away
-- before reporting. §Corrections item 46(c).
local function classifyTrack(targetId, ownTeam)
    if entityControl == nil or entityControl.getEntityInfo == nil then
        return "other", "unknown", false
    end
    local info = entityControl.getEntityInfo(targetId)
    if info == nil then
        return "other", "unknown", false
    end

    local kind = "other"
    local code = info.entityTypeCode
    if code ~= nil and code.kind ~= nil then
        if code.kind == DIS_KIND_MUNITION then
            kind = "munition"
        elseif code.kind == DIS_KIND_PLATFORM then
            if code.domain == DIS_DOMAIN_AIR then
                kind = "air"
            elseif code.domain == DIS_DOMAIN_LAND then
                kind = "ground"
            elseif code.domain == DIS_DOMAIN_SURFACE then
                kind = "surface"
            end
        end
    end

    -- `team` is a RELATION to this aircraft, never a scenario team name: AIC-SEC-2 asserts that no
    -- team name is rendered for a track, and a relation is a comparison result rather than
    -- scenario content. An unknown own-team or an unknown contact team both give `unknown`, which
    -- is the honest answer and is the one Stage-B B4 already refuses to shoot at.
    local team = "unknown"
    local isOwnTeam = false
    if info.team ~= nil and info.team ~= "" and ownTeam ~= "" then
        if info.team == ownTeam then
            team = "friendly"
            isOwnTeam = true
        else
            team = "hostile"
        end
    end
    return kind, team, isOwnTeam
end

local function reportTracks(entityId, ownTeam)
    if sensor == nil or sensor.getTrackNr == nil or sensor.getTrackById == nil then
        return 0
    end
    local count = sensor.getTrackNr(entityId)
    if count == nil or count <= 0 then
        return 0
    end
    local reported = 0
    -- getTrackById is 1-BASED, per the generated stub.
    for i = 1, count do
        local targetId, rangeM, snrDb = sensor.getTrackById(entityId, i)
        if targetId ~= nil and targetId ~= "" then
            local kind, team, isOwnTeam = classifyTrack(targetId, ownTeam)
            -- OWN-TEAM CONTACTS ARE NOT REPORTED (AIC-ORD-2, v1.8.30). All five archived
            -- `fratricide` rejections named RedSAM_FireControlRadar, and because Stage B runs B3
            -- before B4 every one of them PASSED the reported-track check -- which proves Tier 1
            -- put its own side's SAM radar into the picture and the model was then rejected for
            -- naming it. This does not reopen C13's decision: C13 refused filtering because it
            -- "would blind `defend`", which is true of MUNITIONS and does not apply to friendlies.
            -- Munitions stay in the picture and are now distinguishable by `kind` instead.
            if not isOwnTeam then
                -- A false return means the commander's per-window list is full. Stop rather than
                -- spinning: the cap is commander.maxTracksInPrompt and it will not clear this tick.
                if not aiCommander.reportTrack(entityId, targetId, rangeM or 0.0, snrDb or 0.0,
                                               kind, team) then
                    break
                end
                reported = reported + 1
            end
        end
    end
    return reported
end

-- Returns hardpointName -> ammoCount for this aircraft's current loadout.
local function readAmmoByHardpoint(entityId)
    local ammo = {}
    if weapon == nil or weapon.getWeaponLoadout == nil then
        return ammo
    end
    local loadout = weapon.getWeaponLoadout(entityId)
    if type(loadout) ~= "table" then
        return ammo
    end
    for _, row in ipairs(loadout) do
        if row.hardpointName ~= nil then
            ammo[row.hardpointName] = row.ammoCount or 0
        end
    end
    return ammo
end

local function reportLoadout(entityId)
    if weapon == nil or weapon.getWeaponLoadout == nil then
        return
    end
    local loadout = weapon.getWeaponLoadout(entityId)
    if type(loadout) ~= "table" then
        return
    end
    for _, row in ipairs(loadout) do
        if row.hardpointName ~= nil then
            aiCommander.reportLoadout(entityId, row.hardpointName,
                row.weaponProfileName or "", row.ammoCount or 0, row.ammoMax or 0)
        end
    end
end

local function totalRounds(ammoByHardpoint)
    local total = 0
    for _, rounds in pairs(ammoByHardpoint) do
        total = total + rounds
    end
    return total
end

-- Highest-priority hardpoint that has ammunition AND whose disciplined launch range covers rangeM.
-- Nil means no shot: either winchester, or the target is outside every loaded rail's envelope.
local function selectHardpoint(ammoByHardpoint, rangeM)
    for _, hp in ipairs(HARDPOINTS) do
        if (ammoByHardpoint[hp.name] or 0) > 0 and rangeM <= hp.engageRangeM * kLaunchRangeFrac then
            return hp.name
        end
    end
    return nil
end

-- ---------------------------------------------------------------------------------------------
-- Tier 0: survival. Runs first, every tick, unconditionally.
-- ---------------------------------------------------------------------------------------------

-- Defensive pump. A hostile MUNITION-kind track inside kThreatRangeM turns the aircraft cold from
-- it and descends. Returns true when the pump was flown, in which case nothing else runs this tick.
--
-- THIS IS THE MOST IMPORTANT FUNCTION IN THIS FILE AND IT DID NOT EXIST BEFORE v1.8.30. §Non-goals
-- says "anything reactive -- missile defeat, merge maneuvering, launch timing -- stays in Tier 0/1
-- where it already works", and the reference script did not implement the Tier-0/1 half of that
-- sentence: missile defeat was assigned to Tier 1 in the document and delegated to a 20-second
-- model loop in the code. An inbound AMRAAM crosses kThreatRangeM in roughly fifteen seconds; the
-- commander's control loop cannot react inside that window and was never intended to.
--
-- It runs BEFORE the order is read, and before enrolment, and it does not consult ROE. Turning
-- cold is not firing, and an ROE that forbids shooting has never forbidden surviving.
local function flyDefensiveIfThreatened(entityId, s)
    if sensor == nil or sensor.getClosestHostileTrackById == nil then
        return false
    end
    -- The kind filter is the fix, not decoration: without it this flees the nearest hostile
    -- ANYTHING, which in a BVR merge is an aircraft rather than the missile.
    local threatId, threatRangeM = sensor.getClosestHostileTrackById(entityId, DIS_KIND_MUNITION)
    if threatId == nil or threatId == "" or threatRangeM == nil or threatRangeM > kThreatRangeM then
        return false
    end
    if navigation == nil or navigation.requestGoTo == nil then
        return false
    end
    local ownLat, ownLon = positionOf(entityId)
    local threatLat, threatLon = positionOf(threatId)
    if ownLat == nil or threatLat == nil then
        return false
    end
    -- Bearing FROM the threat TO us, extended: that is the cold vector.
    local fleeBearing = bearingAndDistance(threatLat, threatLon, ownLat, ownLon)
    local fleeLat, fleeLon = pointAtBearing(ownLat, ownLon, fleeBearing, kManeuverLegM)
    -- kDefendSpeedMps already exceeds kRecoverSpeedMps, so the pump is its own recovery: an
    -- aircraft that is both stalled and under fire is turned cold AND re-accelerated by this one
    -- call, and clause 8 does not need to preempt survival to be satisfied.
    navGoTo(entityId, fleeLat, fleeLon, kDefendAltM, kDefendSpeedMps)
    setMode(s, entityId, "defend", string.format("%s at %.0f m", threatId, threatRangeM))
    return true
end

-- ---------------------------------------------------------------------------------------------
-- Target selection, when the model supplies none.
-- ---------------------------------------------------------------------------------------------

-- Hostile air-platform tracks sorted by live range, ties broken by id so the order is
-- deterministic. The shipped script's rule, and it belongs here for the same reason: with no order
-- in force, or under weaponsFree, target selection is Tier 1's job and always was.
local function listHostileAirPlatforms(entityId, ownTeam)
    local candidates = {}
    if sensor == nil or sensor.getTrackNr == nil or sensor.getTrackById == nil then
        return candidates
    end
    local seen = {}
    local count = sensor.getTrackNr(entityId) or 0
    for i = 1, count do
        local targetId, rangeM = sensor.getTrackById(entityId, i)
        if targetId ~= nil and targetId ~= "" and rangeM ~= nil and not seen[targetId] then
            seen[targetId] = true
            local kind, team = classifyTrack(targetId, ownTeam)
            if kind == "air" and team == "hostile" then
                candidates[#candidates + 1] = { id = targetId, rangeM = rangeM }
            end
        end
    end
    table.sort(candidates, function(a, b)
        if a.rangeM ~= b.rangeM then return a.rangeM < b.rangeM end
        return a.id < b.id
    end)
    return candidates
end

-- Section rank among the aircraft this script drives (1 = lead), by ascending entityId.
local function sectionRank(entityId)
    local ids = {}
    for id in pairs(state) do ids[#ids + 1] = id end
    table.sort(ids)
    for i, id in ipairs(ids) do
        if id == entityId then return i end
    end
    return 1
end

-- Section deconfliction: in rank order each aircraft claims its own closest still-unclaimed
-- bandit, so the pair never doubles up on one contact while another runs free. Computed once per
-- simulation time and cached, so every aircraft reads the same collision-free result regardless of
-- intra-frame tick order.
local assignment = { time = -1.0, byFighter = {} }

local function computeAssignment(simulationTimeS, candidates)
    local fighters = {}
    for id in pairs(state) do fighters[#fighters + 1] = id end
    table.sort(fighters)

    local claimed = {}
    local byFighter = {}
    for _, fighterId in ipairs(fighters) do
        local fighterLat, fighterLon = positionOf(fighterId)
        if fighterLat ~= nil then
            local bestId, bestRangeM = nil, nil
            for _, candidate in ipairs(candidates) do
                if not claimed[candidate.id] then
                    local banditLat, banditLon = positionOf(candidate.id)
                    if banditLat ~= nil then
                        local _, rangeM =
                            bearingAndDistance(fighterLat, fighterLon, banditLat, banditLon)
                        if bestRangeM == nil or rangeM < bestRangeM
                            or (rangeM == bestRangeM and candidate.id < bestId) then
                            bestId, bestRangeM = candidate.id, rangeM
                        end
                    end
                end
            end
            if bestId ~= nil then
                claimed[bestId] = true
                byFighter[fighterId] = bestId
            end
        end
    end
    assignment.time = simulationTimeS
    assignment.byFighter = byFighter
end

local function assignedTargetId(entityId, simulationTimeS, candidates)
    if #candidates == 0 then
        return nil
    end
    if entityControl == nil or entityControl.getPositionGeodetic == nil then
        return candidates[1].id
    end
    if assignment.time ~= simulationTimeS then
        computeAssignment(simulationTimeS, candidates)
    end
    return assignment.byFighter[entityId] or candidates[1].id
end

-- ---------------------------------------------------------------------------------------------
-- Launch discipline. The script's, never the model's.
-- ---------------------------------------------------------------------------------------------

-- Which target this aircraft may shoot at this tick, given the ROE and the ordered target.
--
-- This is where ROE does its work, and the three values mean exactly what AIC-ORD-2 says they
-- mean: weaponsHold forbids firing outright; weaponsTight permits fire ONLY against the ordered
-- target; weaponsFree additionally permits the script's own selection. The last clause is the one
-- that makes a `hold` or `ingress` order still able to defend itself, and it is the difference
-- between the ladder's rung 2 being a safe fallback and being the state 22 of 24 aircraft died in.
local function firingTargetId(orderedTargetId, roe, entityId, simulationTimeS, candidates)
    if roe == "weaponsHold" then
        return nil
    end
    if orderedTargetId ~= nil and orderedTargetId ~= "" then
        return orderedTargetId
    end
    if roe ~= "weaponsFree" then
        return nil
    end
    return assignedTargetId(entityId, simulationTimeS, candidates)
end

-- `weapon.canFire` is deliberately NOT called (AIC-ORD-2, v1.8.21). It takes the carrier only --
-- no target -- while being documented "armed, in range, ammo > 0", so its range predicate cannot
-- be about the contact being shot at, and it was measured returning false for entire runs with a
-- full rail and a valid weapon component. Used as a precondition it made this whole function
-- unreachable. The shipped oppint_red_interceptor.lua, whose launch behaviour is the quality bar
-- this script is measured against, never calls it either.
local function considerFiring(entityId, s, simulationTimeS, targetId, roe, ammoByHardpoint)
    if roe == "weaponsHold" then
        if weapon ~= nil and weapon.requestCeaseFire ~= nil then
            weapon.requestCeaseFire(entityId)
        end
        return
    end
    if weapon == nil or weapon.requestFire == nil then
        return
    end
    if targetId == nil or targetId == "" then
        return
    end
    -- One shot per assessment window. Checked before the geometry so a target that stays in
    -- range does not re-trigger every tick.
    if simulationTimeS < (s.nextFireTime or 0.0) then
        return
    end

    local ownLat, ownLon = positionOf(entityId)
    local tgtLat, tgtLon = positionOf(targetId)
    if ownLat == nil or tgtLat == nil then
        return
    end
    local _, rangeM = bearingAndDistance(ownLat, ownLon, tgtLat, tgtLon)

    -- Fire from a NAMED HARDPOINT, not the two-argument legacy form (AIC-ORD-2, v1.8.22). The
    -- engine distinguishes the two in its own log -- `hardpoint R77_BVR` against `profile
    -- Weapon_AAM_...` -- and only the hardpoint form draws down a loadout slot. Fired by profile,
    -- 66 launches left ammoCount at 6, which made `winchester` unreachable and left Stage-B's
    -- loadout check with no case to catch.
    --
    -- The rail is chosen BY ENVELOPE (v1.8.30): no loaded rail whose disciplined launch range
    -- covers this target means no shot, rather than a wasted round from whichever rail happened to
    -- be first in the table.
    local hardpoint = selectHardpoint(ammoByHardpoint, rangeM)
    if hardpoint == nil then
        return
    end

    -- The return is CHECKED, not discarded: a refused request must not open an assessment window,
    -- or a weapon that never fires would still look like one pacing its shots.
    if weapon.requestFire(entityId, targetId, hardpoint) then
        s.nextFireTime = simulationTimeS
            + clamp(rangeM / kMissileAvgSpeedMps + kAssessMarginS, kAssessMinS, kAssessMaxS)
        log(string.format("%s launching %s at %s (%.0f m, %d left)",
            entityId, hardpoint, targetId, rangeM, totalRounds(ammoByHardpoint)))
    end
end

-- ---------------------------------------------------------------------------------------------
-- Obligation 2: execute the published order -- or fight without one.
-- ---------------------------------------------------------------------------------------------

local function flyOrderedWaypoint(entityId, speedMps)
    local lat, lon, alt = aiCommander.getWaypoint(entityId)
    if lat == nil then
        return false
    end
    return navGoTo(entityId, lat, lon, alt, speedMps)
end

-- AIC-ORD-2 CLAUSE 7 (PRD v1.8.36, C23). TIER 1 OWNS `hold`'s GEOMETRY.
--
-- WHAT THE ARCHIVE SHOWS, AND WHY THE TEST IS PER-TICK RATHER THAN PER-ORDER.
--
-- Every archived `hold` order that can be checked against the snapshot it answered was issued at
-- 0.00 m from the aircraft's own reported position -- 19 of 19 -- because the model reads
-- `own.latitudeDeg` / `own.longitudeDeg` out of the prompt and hands them back as the waypoint. And
-- AIC-VAL-2 rung 2 synthesizes THE SAME GEOMETRY BY SPECIFICATION: "waypoint at the entity's
-- position at expiry". So the degenerate case is not a model quirk this script can wait out; it is
-- also what the fallback ladder publishes when the backend dies.
--
-- But the aircraft does NOT stay on that point, and this is where the archive corrected its own
-- earlier reading. Under the hold it ranges 3.2-4.4 km out at the ordered 320 m/s, for up to sixty
-- seconds, with no decay at all. The collapse begins in the first 20 s sampling interval after the
-- aircraft is INSIDE the ordered orbitRadiusM -- four times out of four, and twice by the radius
-- changing underneath a stationary aircraft rather than by the aircraft moving. Inside the orbit,
-- `requestHoldPosition` does not hold the ordered cruise speed: 320 -> ~128 -> 1.5000 m/s, latched.
--
-- Hence: OUTSIDE the orbit there is somewhere to fly to and the engine's hold controller flies it,
-- which is the measured-good state and is left alone. INSIDE it, Tier 1 flies the orbit itself,
-- from the same pointAtBearing arithmetic crank and defend already use, at the ORDERED cruise
-- speed. The model still supplies where and how wide; it no longer supplies the geometry -- which
-- is the division of labour the posture table already states for `crank`, applied to the one
-- posture where the model was supplying both.
local function flyHold(entityId, lat, lon, alt, radiusM, speedMps)
    local ownLat, ownLon = positionOf(entityId)
    if ownLat == nil or radiusM == nil or radiusM <= 0.0 then
        return navHoldPosition(entityId, lat, lon, alt, radiusM, speedMps)
    end
    local bearingFromCentre, distanceM = bearingAndDistance(lat, lon, ownLat, ownLon)
    if distanceM >= radiusM then
        return navHoldPosition(entityId, lat, lon, alt, radiusM, speedMps)
    end
    -- Inside. Aim at a point on the orbit a fixed angle round from our own angular position on it.
    -- At distanceM == 0 -- the case the model produces every single time -- bearingAndDistance
    -- returns 0.0, so the first leg is due north of the ordered point and the orbit builds from
    -- there. Arbitrary, deterministic, and the aircraft is flying by the next tick.
    local aimLat, aimLon =
        pointAtBearing(lat, lon, bearingFromCentre + kOrbitLeadDeg, radiusM)
    return navGoTo(entityId, aimLat, aimLon, alt, speedMps)
end

-- crank: the model supplies the TARGET; the geometry is ours. Steering an offset off the
-- own->target bearing keeps the shot supported while reducing closure. Lead cranks right, wingman
-- left, so the section also builds lateral separation.
local function flyCrank(entityId, targetId, speedMps, rank)
    local ownLat, ownLon, ownAlt = positionOf(entityId)
    local tgtLat, tgtLon = positionOf(targetId)
    if ownLat == nil or tgtLat == nil then
        return false
    end
    local bearing = bearingAndDistance(ownLat, ownLon, tgtLat, tgtLon)
    local side = (rank % 2 == 0) and -1.0 or 1.0
    local crankLat, crankLon =
        pointAtBearing(ownLat, ownLon, bearing + side * kCrankOffsetDeg, kManeuverLegM)
    return navGoTo(entityId, crankLat, crankLon, ownAlt, speedMps)
end

-- defend, as an ORDERED posture. Distinct from flyDefensiveIfThreatened above, which is the
-- reflex: this one runs when the model asked for it and there may be no munition in range at all,
-- so it falls back to turning cold from the nearest hostile of any kind.
local function flyDefend(entityId, speedMps)
    local ownLat, ownLon = positionOf(entityId)
    if ownLat == nil then
        return false
    end
    local threatId
    if sensor ~= nil and sensor.getClosestHostileTrackById ~= nil then
        -- Prefer a munition; fall back to anything hostile, because an ordered `defend` with no
        -- munition in view still means "get out of the way of whatever is coming".
        threatId = sensor.getClosestHostileTrackById(entityId, DIS_KIND_MUNITION)
        if threatId == nil or threatId == "" then
            threatId = sensor.getClosestHostileTrackById(entityId)
        end
    end

    local fleeBearing = 0.0
    if threatId ~= nil and threatId ~= "" then
        local tLat, tLon = positionOf(threatId)
        if tLat ~= nil then
            fleeBearing = bearingAndDistance(tLat, tLon, ownLat, ownLon)
        end
    end
    local fleeLat, fleeLon = pointAtBearing(ownLat, ownLon, fleeBearing, kManeuverLegM)
    return navGoTo(entityId, fleeLat, fleeLon, kDefendAltM, speedMps)
end

-- The commander-absent path, and the rollback path: with the DLL deleted, `aiCommander` is nil and
-- every commanded entity lands here.
--
-- IT NO LONGER STOPS AT resumeWaypointFollowing, AND THAT IS THE WHOLE OF C21'S FIX. This runs the
-- engagement logic the shipped script runs, and reaches waypoint following only when there is
-- genuinely nothing to fight. The previous version returned before any fire logic, so a commanded
-- aircraft with no order in force did not shoot at all -- and measured against the shipped script
-- over twelve pairs that cost 22 of 24 aircraft.
--
-- Deliberately NOT copied from the shipped script: its MERGE_POINT and HOME_POINT. Those are one
-- scenario's geography, and a reference script that hardcoded them would fly two aircraft toward a
-- point in the Marianas in every scenario that adopts it. `resumeWaypointFollowing` returns the
-- aircraft to its own authored route, which is the scenario-independent form of the same intent.
--
-- THE "waypoints" BRANCH BELOW IS WHERE C23 SURVIVED THE COMMANDER GETTING ENTIRELY OUT OF THE WAY.
-- Twelve archived samples sit below the speed floor with no order in force at all -- rung 3 has
-- released, `aiCommander.isValid` is false, this function is running -- and the speed stays at
-- exactly 1.5000 m/s. The reason is visible in the stub surface rather than in any run:
-- `navigation.resumeWaypointFollowing(entityId)` TAKES NO SPEED ARGUMENT. It is the only verb this
-- path calls, and it cannot ask for a speed, so a stalled aircraft handed back to Tier 1 is handed
-- to the one verb that has no way to un-stall it. navResume is what closes that.
local function fightWithoutAnOrder(entityId, s, simulationTimeS, ammoByHardpoint, ownTeam)
    local candidates = listHostileAirPlatforms(entityId, ownTeam)
    if #candidates == 0 then
        setMode(s, entityId, "waypoints", "no order, no contact")
        navResume(entityId)
        return
    end

    local rank = sectionRank(entityId)
    local targetId = assignedTargetId(entityId, simulationTimeS, candidates)
    if targetId == nil then
        setMode(s, entityId, "waypoints", "no order, no assignment")
        navResume(entityId)
        return
    end

    -- While the previous shot is assessed, crank instead of running hot at the target.
    if simulationTimeS < (s.nextFireTime or 0.0) then
        setMode(s, entityId, "crank", targetId)
        if not flyCrank(entityId, targetId, kEngageSpeedMps, rank) then
            navTrackTarget(entityId, targetId, kEngageSpeedMps)
        end
        ensureRecoveryNavigated(entityId, s)
        return
    end

    setMode(s, entityId, "engage", targetId .. ", no order")
    navTrackTarget(entityId, targetId, kEngageSpeedMps)
    -- weaponsFree, because that is what an aircraft with no commander has always had: the shipped
    -- script fires under its own discipline with no ROE gate above it at all. A fallback that
    -- granted less than the commander's absence would be a fallback that made things worse.
    considerFiring(entityId, s, simulationTimeS, targetId, "weaponsFree", ammoByHardpoint)
    ensureRecoveryNavigated(entityId, s)
end

function onInit(entityId)
    ensureState(entityId)
end

function onTick(entityId, simulationTimeS, deltaTimeS)
    local s = ensureState(entityId)
    s.tickTimeS = simulationTimeS
    local commanderPresent = aiCommander ~= nil and aiCommander.requestCommand ~= nil
    local ownTeam = ownTeamOf(entityId, s)

    -- Step 0: MEASURE (AIC-ORD-2 clause 8). Is this aircraft still flying? Pure observation, no
    -- verb issued, so it runs before everything -- including the survival reflex, whose own
    -- requestGoTo then goes out with the right speed on the same tick.
    updateFlyingSpeed(entityId, s)

    -- Step 1: enrol and REPORT. Pure observation, no navigation, so it happens before the survival
    -- check -- the model most needs the picture on the tick the aircraft is being shot at, and a
    -- report withheld this tick is a snapshot that describes the previous one.
    if commanderPresent then
        if not s.enrolled then
            s.enrolled = aiCommander.requestCommand(entityId)
        end
        if s.enrolled then
            reportTracks(entityId, ownTeam)
            reportLoadout(entityId)
        end
    end

    -- Step 2: SURVIVE. Unconditional, before the order is read, and not gated on the commander
    -- existing at all. No order and no absence of an order may suppress this.
    if flyDefensiveIfThreatened(entityId, s) then
        return
    end

    local ammoByHardpoint = readAmmoByHardpoint(entityId)
    local rounds = totalRounds(ammoByHardpoint)

    -- Step 3: EGRESS when winchester. Also unconditional: an aircraft with empty rails does not
    -- shadow a fight waiting for an order it cannot execute.
    if rounds == 0 then
        if commanderPresent and s.enrolled then
            aiCommander.setSituationNote(entityId, "winchester")
        end
        if not s.winchester then
            s.winchester = true
            log(entityId .. " winchester; egressing")
            if mission ~= nil and mission.markCompleted ~= nil then
                mission.markCompleted(entityId)
            end
        end
        setMode(s, entityId, "rtb", "winchester")
        -- navResume rather than the bare verb: a winchester aircraft is exactly as capable of
        -- stopping as a held one, and the egress path must not be the one place clause 8 does not
        -- reach. An aircraft that runs itself dry and then parks over the fight is worse than one
        -- that goes home.
        navResume(entityId)
        if weapon ~= nil and weapon.requestCeaseFire ~= nil then
            weapon.requestCeaseFire(entityId)
        end
        return
    end

    -- Step 4: read the order. Anything that is not a valid, in-force order falls through to
    -- step 6 -- which fights.
    if not commanderPresent or not s.enrolled or not aiCommander.isValid(entityId) then
        fightWithoutAnOrder(entityId, s, simulationTimeS, ammoByHardpoint, ownTeam)
        return
    end

    local posture, targetId, speedMps = aiCommander.getPosture(entityId)
    if posture == nil then
        fightWithoutAnOrder(entityId, s, simulationTimeS, ammoByHardpoint, ownTeam)
        return
    end
    local roe = aiCommander.getRoe(entityId)
    local rank = sectionRank(entityId)

    -- weaponsHold is orthogonal to posture: cease fire whatever we are flying.
    if roe == "weaponsHold" and weapon ~= nil and weapon.requestCeaseFire ~= nil then
        weapon.requestCeaseFire(entityId)
    end

    -- Step 5: execute the posture. Navigation only -- the launch decision is made once, below, so
    -- it cannot be reachable from some branches and not others. THAT was the defect: considerFiring
    -- used to sit inside the `engage` branch, and `engage` is 8.2% of every accepted order.
    if posture == "ingress" then
        setMode(s, entityId, "ingress")
        if not flyOrderedWaypoint(entityId, speedMps) then
            navResume(entityId)
        end

    elseif posture == "hold" then
        setMode(s, entityId, "hold")
        local lat, lon, alt = aiCommander.getWaypoint(entityId)
        local radiusM = aiCommander.getOrbitRadiusM(entityId)
        if lat ~= nil and radiusM ~= nil and radiusM > 0.0 then
            -- AIC-ORD-2 clause 7. flyHold, not the bare verb -- see its comment for why the test
            -- is on the CURRENT distance and is re-evaluated every tick rather than once per order.
            flyHold(entityId, lat, lon, alt, radiusM, speedMps)
        else
            navResume(entityId)
        end

    elseif posture == "rtb" then
        setMode(s, entityId, "rtb")
        if weapon ~= nil and weapon.requestCeaseFire ~= nil then
            weapon.requestCeaseFire(entityId)
        end
        if not flyOrderedWaypoint(entityId, speedMps) then
            navResume(entityId)
        end
        -- `rtb` is the ONE posture that keeps its cease-fire, and it returns here so no shot is
        -- taken below. Egressing while shooting is not egressing. The recovery backstop still runs:
        -- clause 8 says "under any posture", and `rtb` is a posture.
        ensureRecoveryNavigated(entityId, s)
        s.lastSerial = aiCommander.getOrderSerial(entityId)
        return

    elseif posture == "engage" then
        setMode(s, entityId, "engage", targetId)
        if targetId ~= nil and targetId ~= "" then
            navTrackTarget(entityId, targetId, speedMps)
        end

    elseif posture == "crank" then
        setMode(s, entityId, "crank", targetId)
        if not flyCrank(entityId, targetId, speedMps, rank) then
            -- Geometry unavailable this tick; pursue rather than doing nothing.
            if targetId ~= nil and targetId ~= "" then
                navTrackTarget(entityId, targetId, speedMps)
            end
        end

    elseif posture == "defend" then
        setMode(s, entityId, "defend", "ordered")
        if not flyDefend(entityId, kDefendSpeedMps) then
            navResume(entityId)
        end

    else
        -- An unknown posture cannot reach here -- the validator rejects anything outside the
        -- vocabulary -- but a script that assumed so and was wrong would fly nothing at all.
        fightWithoutAnOrder(entityId, s, simulationTimeS, ammoByHardpoint, ownTeam)
        return
    end

    -- The launch decision, made ONCE for every posture that reaches here: ingress, hold, engage,
    -- crank and defend. `rtb` returned above with its cease-fire intact.
    --
    -- `crank` firing again is the point rather than an oversight -- the posture exists to support a
    -- shot already in the air, and the assessment window inside considerFiring is what actually
    -- spaces the shots. `hold` and `ingress` firing is what ROE is for: under weaponsTight they can
    -- only shoot the ordered target, which those postures do not carry, so in practice they fire
    -- only under weaponsFree. That is exactly the authority an aircraft with no commander already
    -- has, which is why granting it here cannot make a fallback more dangerous than no fallback.
    local candidates = listHostileAirPlatforms(entityId, ownTeam)
    local shootAt = firingTargetId(targetId, roe, entityId, simulationTimeS, candidates)
    considerFiring(entityId, s, simulationTimeS, shootAt, roe, ammoByHardpoint)

    -- Clause 8's backstop, AFTER the launch decision so it cannot suppress a shot. `engage` and
    -- `crank` command no navigation at all when the ordered target is empty, and those two branches
    -- are the only way to reach this line having steered nothing.
    ensureRecoveryNavigated(entityId, s)

    s.lastSerial = aiCommander.getOrderSerial(entityId)
end

function onShutdown(entityId)
    state[entityId] = nil
end
