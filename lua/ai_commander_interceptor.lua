-- Reference Tier-1 behaviour for the N8RO AI Entity Commander (AIC-ORD-2).
--
-- This is the deterministic half of the three-tier split. The commander (Tier 2) decides POSTURE,
-- TARGET, WAYPOINT and ROE; this script decides everything else -- pursuit geometry, crank offset,
-- defensive pump, launch discipline -- and it is the only thing that ever calls a request* verb.
--
-- Two obligations, in order, every tick:
--   1. REPORT what this aircraft can see (tracks and stores). The commander has no C++ read seam
--      for sensor detections or live ammo, so an unreported contact simply cannot be targeted:
--      Stage-B validation rejects any order naming a target that was never reported.
--   2. READ the published order and execute it with the verbs mapped below.
--
-- It degrades to ordinary waypoint following when `aiCommander` is nil (plugin not deployed) or
-- when the commander has released this entity. That path is not an afterthought -- it is how a
-- rollback works: deleting one DLL must leave a script that still flies.
--
-- Posture -> verb mapping (AIC-ORD-2):
--   ingress -> navigation.requestGoTo(id, lat, lon, alt, speed)
--   engage  -> navigation.requestTrackTarget(id, targetId, speed) + weapon.requestFire, spaced by
--              an assessment window. NOT weapon.canFire -- see considerFiring.
--   crank   -> navigation.requestGoTo(...) with a SCRIPT-computed offset steer point
--   defend  -> navigation.requestGoTo(...) with a SCRIPT-computed cold point
--   hold    -> navigation.requestHoldPosition(id, lat, lon, alt, orbitRadiusM, speed)
--   rtb     -> navigation.requestGoTo(...) + weapon.requestCeaseFire

local kCrankOffsetDeg   = 40.0     -- Off the own->target bearing while a shot is assessed.
local kManeuverLegM     = 25000.0  -- Length of a computed steer leg.
local kDefendAltM       = 3000.0   -- Descend while defending.
local kDefendSpeedMps   = 320.0
local kLaunchRangeFrac  = 0.8      -- Fire inside 0.8 of kinematic reach, not at the edge.
local kMissileRangeM    = 60000.0
local kEarthRadiusM     = 6371000.0

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
        state[entityId] = { mode = "", enrolled = false, lastSerial = -1, nextFireTime = 0.0 }
    end
    return state[entityId]
end

local function log(message)
    if mission ~= nil and mission.log ~= nil then
        mission.log(message)
    end
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

-- ---------------------------------------------------------------------------------------------
-- Obligation 1: report what this aircraft sees.
-- ---------------------------------------------------------------------------------------------

local function reportTracks(entityId)
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
            -- A false return means the commander's per-window list is full. Stop rather than
            -- spinning: the cap is commander.maxTracksInPrompt and it will not clear this tick.
            if not aiCommander.reportTrack(entityId, targetId, rangeM or 0.0, snrDb or 0.0) then
                break
            end
            reported = reported + 1
        end
    end
    return reported
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

local function totalRounds(entityId)
    if weapon == nil or weapon.getWeaponLoadout == nil then
        return 0
    end
    local loadout = weapon.getWeaponLoadout(entityId)
    if type(loadout) ~= "table" then
        return 0
    end
    local total = 0
    for _, row in ipairs(loadout) do
        total = total + (row.ammoCount or 0)
    end
    return total
end

-- The first hardpoint still carrying a round, or nil when the aircraft is winchester. The stock
-- oppint_red_interceptor.lua additionally picks by range against the profile's envelope; this
-- takes the simpler rule because Tier 2 supplies no weapon-selection intent and inventing one
-- here would be the script deciding something AIC-ORD-2 does not give it.
local function firstLoadedHardpoint(entityId)
    if weapon == nil or weapon.getWeaponLoadout == nil then
        return nil
    end
    local loadout = weapon.getWeaponLoadout(entityId)
    if type(loadout) ~= "table" then
        return nil
    end
    for _, row in ipairs(loadout) do
        if row.hardpointName ~= nil and (row.ammoCount or 0) > 0 then
            return row.hardpointName
        end
    end
    return nil
end

-- ---------------------------------------------------------------------------------------------
-- Obligation 2: execute the published order.
-- ---------------------------------------------------------------------------------------------

-- The commander-absent path. This is also the rollback path: with the DLL deleted, `aiCommander`
-- is nil and every commanded entity lands here.
local function fallBackToWaypoints(entityId, s)
    setMode(s, entityId, "waypoints", "commander absent")
    if navigation ~= nil and navigation.resumeWaypointFollowing ~= nil then
        navigation.resumeWaypointFollowing(entityId)
    end
end

local function flyOrderedWaypoint(entityId, speedMps)
    local lat, lon, alt = aiCommander.getWaypoint(entityId)
    if lat == nil then
        return false
    end
    return navigation.requestGoTo(entityId, lat, lon, alt, speedMps)
end

-- crank: the model supplies the TARGET; the geometry is ours. Steering an offset off the
-- own->target bearing keeps the shot supported while reducing closure.
local function flyCrank(entityId, targetId, speedMps)
    if entityControl == nil or entityControl.getPositionGeodetic == nil then
        return false
    end
    local ownLat, ownLon, ownAlt = entityControl.getPositionGeodetic(entityId)
    local tgtLat, tgtLon = entityControl.getPositionGeodetic(targetId)
    if ownLat == nil or tgtLat == nil then
        return false
    end
    local bearing = bearingAndDistance(ownLat, ownLon, tgtLat, tgtLon)
    local crankLat, crankLon = pointAtBearing(ownLat, ownLon, bearing + kCrankOffsetDeg, kManeuverLegM)
    return navigation.requestGoTo(entityId, crankLat, crankLon, ownAlt, speedMps)
end

-- defend: the model says "defend"; WHICH threat and which way to run are ours. Turn cold from the
-- nearest contact and descend.
local function flyDefend(entityId, speedMps)
    if entityControl == nil or entityControl.getPositionGeodetic == nil then
        return false
    end
    local ownLat, ownLon = entityControl.getPositionGeodetic(entityId)
    if ownLat == nil then
        return false
    end

    local threatId
    if sensor ~= nil and sensor.getClosestHostileTrackById ~= nil then
        threatId = sensor.getClosestHostileTrackById(entityId)
    end

    local fleeBearing = 0.0
    if threatId ~= nil and threatId ~= "" then
        local tLat, tLon = entityControl.getPositionGeodetic(threatId)
        if tLat ~= nil then
            -- Bearing FROM the threat TO us, extended: that is the cold vector.
            fleeBearing = bearingAndDistance(tLat, tLon, ownLat, ownLon)
        end
    end
    local fleeLat, fleeLon = pointAtBearing(ownLat, ownLon, fleeBearing, kManeuverLegM)
    return navigation.requestGoTo(entityId, fleeLat, fleeLon, kDefendAltM, speedMps)
end

-- Launch discipline is the script's, not the model's. The order can say "engage"; only this
-- decides whether a shot is actually taken, and ROE gates it independently of posture.
--
-- `weapon.canFire` is deliberately NOT called (AIC-ORD-2, v1.8.21). It takes the carrier only --
-- no target -- while being documented "armed, in range, ammo > 0", so its range predicate cannot
-- be about the contact being shot at, and it was measured returning false for entire runs with a
-- full rail and a valid weapon component. Used as a precondition it made this whole function
-- unreachable. The shipped oppint_red_interceptor.lua, whose launch behaviour is the quality bar
-- this script is measured against, never calls it either.
local function considerFiring(entityId, s, simulationTimeS, targetId, roe)
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

    local rangeM
    if entityControl ~= nil and entityControl.getPositionGeodetic ~= nil then
        local ownLat, ownLon = entityControl.getPositionGeodetic(entityId)
        local tgtLat, tgtLon = entityControl.getPositionGeodetic(targetId)
        if ownLat ~= nil and tgtLat ~= nil then
            local _, r = bearingAndDistance(ownLat, ownLon, tgtLat, tgtLon)
            rangeM = r
            -- Fire inside 0.8 of kinematic reach rather than at the edge of the envelope.
            if rangeM > kMissileRangeM * kLaunchRangeFrac then
                return
            end
        end
    end

    -- Fire from a NAMED HARDPOINT, not the two-argument legacy form (AIC-ORD-2, v1.8.22). The
    -- engine distinguishes the two in its own log -- `hardpoint R77_BVR` against `profile
    -- Weapon_AAM_...` -- and only the hardpoint form draws down a loadout slot. Fired by profile,
    -- 66 launches left ammoCount at 6, which made `winchester` unreachable and left Stage-B's
    -- loadout check with no case to catch. No loaded hardpoint means no shot: an aircraft with
    -- nothing on the rails must report winchester rather than fire from nowhere.
    local hardpoint = firstLoadedHardpoint(entityId)
    if hardpoint == nil then
        return
    end

    -- The return is CHECKED, not discarded: a refused request must not open an assessment window,
    -- or a weapon that never fires would still look like one pacing its shots.
    if weapon.requestFire(entityId, targetId, hardpoint) then
        local assessS = (rangeM or kMissileRangeM * kLaunchRangeFrac) / kMissileAvgSpeedMps
            + kAssessMarginS
        if assessS < kAssessMinS then assessS = kAssessMinS end
        if assessS > kAssessMaxS then assessS = kAssessMaxS end
        s.nextFireTime = simulationTimeS + assessS
        log(string.format("%s launching %s at %s (%.0f m, assess %.0f s, %d left)",
            entityId, hardpoint, targetId, rangeM or -1.0, assessS, totalRounds(entityId)))
    end
end

function onInit(entityId)
    ensureState(entityId)
end

function onTick(entityId, simulationTimeS, deltaTimeS)
    local s = ensureState(entityId)

    -- Commander absent entirely -- plugin not deployed, or rolled back.
    if aiCommander == nil or aiCommander.requestCommand == nil then
        fallBackToWaypoints(entityId, s)
        return
    end

    -- Enrol once. Idempotent, but a false return means the roster is full and this aircraft flies
    -- its own waypoints rather than silently waiting for an order that will never come.
    if not s.enrolled then
        s.enrolled = aiCommander.requestCommand(entityId)
        if not s.enrolled then
            fallBackToWaypoints(entityId, s)
            return
        end
    end

    -- Obligation 1, BEFORE reading an order: report the picture. Doing it after would mean the
    -- commander's next snapshot carried the previous tick's view.
    reportTracks(entityId)
    reportLoadout(entityId)

    if totalRounds(entityId) == 0 then
        aiCommander.setSituationNote(entityId, "winchester")
    end

    -- Obligation 2: execute. No valid order -> Tier 1 keeps the aircraft flying.
    if not aiCommander.isValid(entityId) then
        fallBackToWaypoints(entityId, s)
        return
    end

    local posture, targetId, speedMps = aiCommander.getPosture(entityId)
    if posture == nil then
        fallBackToWaypoints(entityId, s)
        return
    end
    local roe = aiCommander.getRoe(entityId)

    -- weaponsHold is orthogonal to posture: cease fire whatever we are flying.
    if roe == "weaponsHold" and weapon ~= nil and weapon.requestCeaseFire ~= nil then
        weapon.requestCeaseFire(entityId)
    end

    if posture == "ingress" then
        setMode(s, entityId, "ingress")
        if not flyOrderedWaypoint(entityId, speedMps) then
            fallBackToWaypoints(entityId, s)
        end

    elseif posture == "hold" then
        setMode(s, entityId, "hold")
        local lat, lon, alt = aiCommander.getWaypoint(entityId)
        local radiusM = aiCommander.getOrbitRadiusM(entityId)
        if lat ~= nil and radiusM > 0.0 then
            navigation.requestHoldPosition(entityId, lat, lon, alt, radiusM, speedMps)
        else
            fallBackToWaypoints(entityId, s)
        end

    elseif posture == "rtb" then
        setMode(s, entityId, "rtb")
        if weapon ~= nil and weapon.requestCeaseFire ~= nil then
            weapon.requestCeaseFire(entityId)
        end
        if not flyOrderedWaypoint(entityId, speedMps) then
            fallBackToWaypoints(entityId, s)
        end

    elseif posture == "engage" then
        setMode(s, entityId, "engage", targetId)
        if navigation.requestTrackTarget ~= nil and targetId ~= nil and targetId ~= "" then
            navigation.requestTrackTarget(entityId, targetId, speedMps)
        end
        -- weaponsTight permits fire only against the ORDERED target, which is exactly what is
        -- passed here; weaponsFree would additionally allow the script's own selection.
        considerFiring(entityId, s, simulationTimeS, targetId, roe)

    elseif posture == "crank" then
        setMode(s, entityId, "crank", targetId)
        if not flyCrank(entityId, targetId, speedMps) then
            -- Geometry unavailable this tick; pursue rather than doing nothing.
            if navigation.requestTrackTarget ~= nil and targetId ~= nil and targetId ~= "" then
                navigation.requestTrackTarget(entityId, targetId, speedMps)
            end
        end

    elseif posture == "defend" then
        setMode(s, entityId, "defend")
        if not flyDefend(entityId, kDefendSpeedMps) then
            fallBackToWaypoints(entityId, s)
        end

    else
        -- An unknown posture cannot reach here -- the validator rejects anything outside the
        -- vocabulary -- but a script that assumed so and was wrong would fly nothing at all.
        fallBackToWaypoints(entityId, s)
    end

    s.lastSerial = aiCommander.getOrderSerial(entityId)
end
