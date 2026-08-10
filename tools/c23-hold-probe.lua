-- Arkheon Technologies
-- Proprietary and Confidential.
-- Unauthorized copying of this file, via any medium, is strictly prohibited.
-- (c) Arkheon Technologies. All rights reserved.

-- C23 CONFIRMING PROBE (PRD v1.8.37, docs/c23-report.md section 10.2).
--
-- THIS IS AN INSTRUMENT, NOT A TIER-1 SCRIPT. It is not shipped behaviour, it does not implement
-- AIC-ORD-2, it never fires a weapon and it never reads an order. It drives navigation directly
-- from simulation time so that the two questions the offline Lua suite cannot answer are answered
-- by the engine itself:
--
--   Q1  Does `navigation.requestHoldPosition` stop an aircraft that flies IN to the ordered orbit
--       from OUTSIDE it? The archive says the collapse begins within one cadence window of the
--       aircraft being inside the ordered radius, four times out of four - but every archived hold
--       was ordered at 0.00 m, so a hold ordered far away and then followed has never occurred.
--       §Corrections item 51(f) records that no number of further scenario repeats will produce
--       one, because the model does not order distant holds. It has to be INJECTED, and this is
--       the injection.
--
--   Q2  Does a commanded 300 m/s actually re-accelerate an aircraft that has stopped? The offline
--       suite asserts the COMMAND and cannot assert the outcome, and this archive already shows the
--       two are not the same thing: `RedSu35_01` flew `defend (ordered)` under a commanded 320 m/s
--       for 490 s and sustained about 150. AIC-ORD-2 clause 8's acceptance criterion says
--       "recovers above it within one cadence window"; only a run can show a recovery.
--
-- Both are mechanisms readable directly off recorded values - "does this speed stay above the floor
-- while the aircraft is inside the orbit" - which §Closing a row rule 1 (PRD v1.8.53) makes the one
-- category a single run may close. Both satisfy all three of its conjuncts: each is a yes/no
-- readable off the record, each quotes the value it turns on, and neither has a denominator the
-- scenario samples. Neither is a rate and neither is claimed as one.
--
-- NOTE: this comment used to cite "§Scope authority rule 4". THAT RULE NEVER EXISTED - it was cited
-- twelve times across this project as the authority for exactly this move, and §Scope authority is
-- three unnumbered paragraphs about the PRD/design boundary. §Corrections item 68(a) is the record;
-- C25 is the decision to write the real rule down, closed in v1.8.53.
--
-- NO COMMANDER, NO MODEL, NO NETWORK. The harness asserts that `data/config/plugins/ai-commander.cfg`
-- is absent before it starts, so `aiCommander` is nil throughout and nothing here can be an artifact
-- of an order.
--
-- THREE PHASES, on simulation time, identical for every entity this script drives:
--
--   A  0 - 30 s    DEGENERATE HOLD. requestHoldPosition at the aircraft's OWN position at t=0, with
--                  orbitRadiusM 5,000 and cruiseSpeedMps 320. This is the archived order reproduced
--                  with no model and no commander in the loop, and it is also exactly what
--                  AIC-VAL-2 rung 2 publishes by specification. The aircraft is inside the orbit
--                  from the first tick, so the collapse starts at once and is complete in ~20 s -
--                  which is what makes a stalled aircraft available to phase B while it is still
--                  alive.
--
--   B  30 - 90 s   RECOVERY. requestGoTo to a point 25 km ahead on the current course at 300 m/s,
--                  re-issued every tick - which is exactly what the fixed reference script does
--                  while `recovering`. THIS IS Q2, and it is answered inside one cadence window if
--                  it is answered at all.
--
--   C  90 - 200 s  DISTANT HOLD. requestHoldPosition at a point 20 km away on the reciprocal of the
--                  current course - away from the merge - same radius and speed. The aircraft flies
--                  OUT and captures the orbit from outside it. THIS IS Q1.
--
-- PHASE ORDER IS DELIBERATE AND WAS CHOSEN BY A FAILED RUN, which is worth recording because the
-- ordering is the only reason this instrument works. The first attempt (archived
-- `20260809T102936Z-c23-probe`) ran the distant hold FIRST, over 0-200 s, and answered Q1
-- decisively - and then both aircraft were destroyed by the Blue CAP at t≈85 and t≈149, before the
-- recovery phase was ever reached. This probe carries no defensive reflex and never fires, so its
-- subjects are on a clock. THE FRAGILE MEASUREMENT NOW GOES FIRST.
--
-- Every command is RE-ISSUED EVERY TICK, because that is what the reference script does and what
-- produced the archived behaviour. A probe that issued once would be measuring a different thing.

local kDistantHoldM    = 20000.0
local kOrbitRadiusM    = 5000.0
local kHoldSpeedMps    = 320.0
local kRecoverSpeedMps = 300.0   -- Tier 1's own cruise value, as clause 8 uses it.
local kRecoverLegM     = 25000.0
local kEarthRadiusM    = 6371000.0

local PHASE_A_END =  30.0
local PHASE_B_END =  90.0
local PHASE_C_END = 200.0

local state = {}

local function log(message)
    if mission ~= nil and mission.log ~= nil then
        mission.log(message)
    end
end

local function positionOf(entityId)
    if entityControl == nil or entityControl.getPositionGeodetic == nil then
        return nil
    end
    return entityControl.getPositionGeodetic(entityId)
end

local function velocityOf(entityId)
    if entityControl == nil or entityControl.getVelocityNed == nil then
        return nil
    end
    local velN, velE, velD = entityControl.getVelocityNed(entityId)
    if velN == nil or velE == nil or velD == nil then
        return nil
    end
    return velN, velE, velD
end

-- The same spherical arithmetic the reference script uses, so a distance printed here and a
-- distance computed by the script under test are the same quantity.
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
    return bearing, 2.0 * kEarthRadiusM * math.atan(math.sqrt(a), math.sqrt(1.0 - a))
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

local function courseDeg(velN, velE)
    if math.abs(velN) < 1e-9 and math.abs(velE) < 1e-9 then
        return 0.0
    end
    return (math.atan(velE, velN) * 180.0 / math.pi + 360.0) % 360.0
end

function onInit(entityId)
    state[entityId] = { phase = "", nextLogS = -1.0, holdLat = nil, holdLon = nil, holdAlt = nil }
    log(string.format("C23PROBE init %s", entityId))
end

function onTick(entityId, simulationTimeS, deltaTimeS)
    local s = state[entityId]
    if s == nil then
        onInit(entityId)
        s = state[entityId]
    end

    local ownLat, ownLon, ownAlt = positionOf(entityId)
    if ownLat == nil then
        return              -- destroyed or not yet materialized; nothing to measure.
    end
    local velN, velE, velD = velocityOf(entityId)
    local speedMps = -1.0
    if velN ~= nil then
        speedMps = math.sqrt(velN * velN + velE * velE + velD * velD)
    end

    local phase
    if simulationTimeS < PHASE_A_END then
        phase = "A-degenerate-hold"
    elseif simulationTimeS < PHASE_B_END then
        phase = "B-recovery"
    elseif simulationTimeS < PHASE_C_END then
        phase = "C-distant-hold"
    else
        phase = "D-done"
    end

    -- On entering a phase, fix the geometry ONCE. The command is re-issued every tick below, but
    -- against a point that does not move - a hold point recomputed each tick would chase the
    -- aircraft, which is a different experiment and the one the archive already contains.
    if s.phase ~= phase then
        s.phase = phase
        if phase == "A-degenerate-hold" then
            -- The archived order: hold where you already are. 19 of 19 measurable archived holds
            -- were issued at 0.00 m, and AIC-VAL-2 rung 2 synthesizes the same thing.
            s.holdLat, s.holdLon, s.holdAlt = ownLat, ownLon, ownAlt
            log(string.format(
                "C23PROBE phase %s %s holdPoint=%.6f,%.6f alt=%.0f radiusM=%.0f speed=%.1f "
                .. "initialDistanceM=0.0 entrySpeedMps=%.4f",
                phase, entityId, s.holdLat, s.holdLon, s.holdAlt, kOrbitRadiusM, kHoldSpeedMps,
                speedMps))
        elseif phase == "B-recovery" then
            s.holdLat, s.holdLon, s.holdAlt = nil, nil, nil
            log(string.format("C23PROBE phase %s %s entrySpeedMps=%.4f commandedMps=%.1f",
                phase, entityId, speedMps, kRecoverSpeedMps))
        elseif phase == "C-distant-hold" then
            -- The reciprocal of the current course: away from wherever the aircraft was heading,
            -- which in this scenario is away from the Blue CAP. A measurement cut short by a
            -- missile is not a measurement.
            local away = (courseDeg(velN or 0.0, velE or 0.0) + 180.0) % 360.0
            s.holdLat, s.holdLon = pointAtBearing(ownLat, ownLon, away, kDistantHoldM)
            s.holdAlt = ownAlt
            log(string.format(
                "C23PROBE phase %s %s holdPoint=%.6f,%.6f alt=%.0f radiusM=%.0f speed=%.1f "
                .. "initialDistanceM=%.1f entrySpeedMps=%.4f",
                phase, entityId, s.holdLat, s.holdLon, s.holdAlt, kOrbitRadiusM, kHoldSpeedMps,
                kDistantHoldM, speedMps))
        end
        s.nextLogS = -1.0
    end

    -- Issue the phase's command, every tick.
    if phase == "A-degenerate-hold" or phase == "C-distant-hold" then
        if navigation ~= nil and navigation.requestHoldPosition ~= nil and s.holdLat ~= nil then
            navigation.requestHoldPosition(entityId, s.holdLat, s.holdLon, s.holdAlt,
                                           kOrbitRadiusM, kHoldSpeedMps)
        end
    elseif phase == "B-recovery" then
        if navigation ~= nil and navigation.requestGoTo ~= nil then
            local ahead = courseDeg(velN or 0.0, velE or 0.0)
            local lat, lon = pointAtBearing(ownLat, ownLon, ahead, kRecoverLegM)
            navigation.requestGoTo(entityId, lat, lon, ownAlt, kRecoverSpeedMps)
        end
    end

    -- One sample per simulation second. The archive samples at the 20 s cadence, which is why its
    -- onset is bracketed to 16.5-17.1 s rather than measured; 1 Hz is what turns that bracket into
    -- a number.
    if simulationTimeS >= s.nextLogS then
        s.nextLogS = math.floor(simulationTimeS) + 1.0
        local distanceM = -1.0
        if s.holdLat ~= nil then
            local _, d = bearingAndDistance(ownLat, ownLon, s.holdLat, s.holdLon)
            distanceM = d
        end
        log(string.format(
            "C23PROBE t=%.1f %s %s lat=%.6f lon=%.6f alt=%.1f velN=%.4f velE=%.4f velD=%.4f "
            .. "speedMps=%.4f distanceToHoldM=%.1f inside=%s",
            simulationTimeS, entityId, s.phase, ownLat, ownLon, ownAlt,
            velN or 0.0, velE or 0.0, velD or 0.0, speedMps, distanceM,
            tostring(distanceM >= 0.0 and distanceM < kOrbitRadiusM)))
    end
end

function onShutdown(entityId)
    state[entityId] = nil
end
