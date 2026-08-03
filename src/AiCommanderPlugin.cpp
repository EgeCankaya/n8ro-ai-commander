#include "AiCommanderPlugin.h"

#include "LocalLlmClient.h"
#include "PromptRenderer.h"
#include "ReplayLlmClient.h"
#include "StubLlmClient.h"

#include <component/ComponentFieldAccess.h>
#include <component/ComponentTypeNames.h>
#include <core/logging/GlobalLogger.h>
#include <core/threading/IThreadRunner.h>
#include <entity/IEntity.h>
#include <entity/IEntityManager.h>
#include <entity/TransformRuntimeColumns.h>
#include <plugin/IPluginServices.h>
#include <plugin/IScriptingPluginService.h>
#include <plugin/PluginContext.h>
#include <scripting/ScriptingApiContext.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <system_error>
#include <string>
#include <string_view>
#include <utility>

namespace arkheon::aicommander {

namespace {

constexpr std::string_view kLogCategory = "arkheon.aiCommander";
constexpr std::string_view kPluginId = "ai-commander";

[[nodiscard]] const char* yesNo(bool value) {
    return value ? "non-null" : "NULL";
}

// Reads the doctrine block for the prompt's stable prefix. Missing is not an error: the prefix is
// still well-formed without it, and refusing to start over an absent text file would make the
// commander harder to bring up than it needs to be.
[[nodiscard]] std::string readDoctrine(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {};
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

// Resolves a host-working-directory-relative path to something an operator can act on. A log line
// saying "no file at data/config/plugins/ai-commander.cfg" is only half an answer when the reader
// does not know which directory that is relative to — which is precisely how Corrections item 16
// stayed invisible for two phases.
[[nodiscard]] std::string absoluteForLog(const std::string& path) {
    std::error_code error;
    const std::filesystem::path resolved = std::filesystem::absolute(path, error);
    return error ? path : resolved.string();
}

// The production StageBWorldView: reads live state through IEntityManager on the simulation
// thread. Never handed to a worker.
class EngineWorldView final : public StageBWorldView {
public:
    explicit EngineWorldView(const n8ro::sim::IEntityManager* manager) : manager_(manager) {}

    bool entityExists(const std::string& entityId) const override {
        return manager_ != nullptr && manager_->getEntity(entityId) != nullptr;
    }

    std::string teamOf(const std::string& entityId) const override {
        if (manager_ == nullptr) {
            return {};
        }
        const n8ro::sim::IEntity* entity = manager_->getEntity(entityId);
        return entity == nullptr ? std::string() : entity->getTeam();
    }

    bool positionOf(const std::string& entityId, double& lat, double& lon, double& alt) const override {
        if (manager_ == nullptr) {
            return false;
        }
        const std::optional<double> latitude = n8ro::sim::readComponentFieldReal(
            *manager_, entityId, n8ro::sim::kComponentTransform, "positionGeodetic/latitudeDeg");
        const std::optional<double> longitude = n8ro::sim::readComponentFieldReal(
            *manager_, entityId, n8ro::sim::kComponentTransform, "positionGeodetic/longitudeDeg");
        const std::optional<double> altitude = n8ro::sim::readComponentFieldReal(
            *manager_, entityId, n8ro::sim::kComponentTransform, "positionGeodetic/altitudeHaeM");
        if (!latitude || !longitude || !altitude) {
            return false;
        }
        lat = *latitude;
        lon = *longitude;
        alt = *altitude;
        return true;
    }

private:
    const n8ro::sim::IEntityManager* manager_ = nullptr;
};

} // namespace

AiCommanderPlugin::~AiCommanderPlugin() = default;

int AiCommanderPlugin::getInterfaceVersion() const {
    return 1;
}

n8ro::core::PluginMetadata AiCommanderPlugin::getMetadata() const {
    n8ro::core::PluginMetadata metadata;
    metadata.setPluginId(std::string(kPluginId));
    metadata.setVersion("0.1.0");
    metadata.setAuthor("Arkheon Technologies");
    return metadata;
}

void AiCommanderPlugin::rebuildBackend() {
    const CommanderConfig& config = runtime_.config();

    switch (config.backend) {
        case Backend::Stub:
            runtime_.setClient(std::make_unique<StubLlmClient>());
            break;
        case Backend::Replay: {
            auto replay = std::make_unique<ReplayLlmClient>();
            std::string error;
            if (!replay->load(config.replayPath, error)) {
                N8RO_LOG_ERROR(
                    std::string("ai-commander: replay backend unavailable - ") + error
                        + ". The commander will not issue orders.",
                    kLogCategory);
                runtime_.setClient(nullptr);
                return;
            }
            N8RO_LOG_INFO(
                std::string("ai-commander: replay loaded ") + std::to_string(replay->entryCount())
                    + " recorded orders from " + config.replayPath,
                kLogCategory);
            runtime_.setClient(std::move(replay));
            break;
        }
        case Backend::Local: {
            LocalClientConfig local;
            local.baseUrl = config.localBaseUrl;
            local.model = config.localModel;
            local.temperature = config.localTemperature;
            local.grammarEnabled = config.localGrammarEnabled;
            local.timeoutS = config.requestTimeoutS;

            // Constructed, not contacted. The server is first reached on the worker, at the first
            // order — nothing here blocks initialize(), and no model is loaded to warm a cache that
            // measurement showed pays for itself only by moving the cost (PRD §Corrections item 14).
            N8RO_LOG_INFO(
                std::string("ai-commander: local backend -> ") + local.baseUrl + " model='"
                    + local.model + "' (an Ollama tag) temperature="
                    + std::to_string(local.temperature) + " format="
                    + (local.grammarEnabled ? "on" : "OFF - unconstrained decoding")
                    + " timeoutS=" + std::to_string(local.timeoutS),
                kLogCategory);
            if (!local.grammarEnabled) {
                // Worth a warning of its own: with `format` off the decoder enforces nothing, and
                // Stage-A `shape` rejections go from near-zero to the majority of orders. That is a
                // legitimate configuration for an H3 baseline and a broken one for a demo.
                N8RO_LOG_WARNING(
                    "ai-commander: local.grammarEnabled is false - the order schema is NOT sent as "
                    "Ollama's `format`. Expect the majority of orders to be rejected `shape`. This "
                    "is only meaningful as an unconstrained-decoding baseline.",
                    kLogCategory);
            }
            runtime_.setClient(std::make_unique<LocalLlmClient>(local));
            break;
        }
        case Backend::Claude:
            // Phase 2. Deliberately not stubbed out with a silent fallback to `stub`: an operator
            // who selected `claude` and silently got canned orders would have no way to notice, and
            // every downstream measurement would be meaningless.
            N8RO_LOG_WARNING(
                std::string("ai-commander: backend '") + toString(config.backend)
                    + "' is not implemented in this build (Phase 2). The commander will not "
                      "issue orders. Set commander.backend to stub, replay, or local.",
                kLogCategory);
            runtime_.setClient(nullptr);
            return;
    }

    // The prefix is rebuilt with the backend, so a doctrine or config change invalidates the
    // prompt cache exactly once and deliberately (AIC-BE-3).
    const std::string doctrine = readDoctrine(config.doctrinePath);
    if (doctrine.empty()) {
        // Loudly, because the failure is otherwise invisible: the prefix renders "(none provided)",
        // every order is still accepted, and only order QUALITY degrades - which no counter shows.
        // The path is relative to the host's working directory, not to the DLL, so a deployed
        // plugin looks for it under the release root and finds nothing unless it was put there.
        N8RO_LOG_WARNING(
            std::string("ai-commander: no doctrine at '") + config.doctrinePath
                + "' (resolved relative to the host's working directory). The prompt prefix will "
                  "carry '(none provided)' and order quality is degraded, silently, in a way no "
                  "rejection counter reports. Copy the plugin's data/doctrine.txt there, or point "
                  "prompt.doctrinePath at it.",
            kLogCategory);
    } else {
        N8RO_LOG_INFO(
            std::string("ai-commander: doctrine loaded from '") + config.doctrinePath + "' ("
                + std::to_string(doctrine.size()) + " bytes)",
            kLogCategory);
    }
    runtime_.promptRenderer().build(config, doctrine);
}

void AiCommanderPlugin::applyDeployedConfigFile() {
    // R3 / AIC-API-2: the file is a DEFAULT source, never an override. If the host has already
    // applied configuration, that application is the authority and this read does not happen —
    // which is what makes "an explicit application always wins" true regardless of call order.
    if (configExplicitlyApplied_) {
        N8RO_LOG_INFO(
            "ai-commander: configuration was applied by the host before initialize(); "
            "the deployed config file was not read.",
            kLogCategory);
        return;
    }

    const std::string path = kDeployedConfigPath;
    const std::optional<std::vector<n8ro::core::PluginConfigField>> parsed =
        readDeployedConfigFile(path);
    if (!parsed) {
        // INFO, not WARNING. Absent-and-disabled is the state the deployment checklist REQUIRES of
        // a default deploy; warning on the correct state teaches operators to ignore warnings.
        N8RO_LOG_INFO(
            std::string("ai-commander: no deployed config at '") + absoluteForLog(path)
                + "'; using compiled defaults (commander.enabled=false).",
            kLogCategory);
        return;
    }

    const std::vector<n8ro::core::PluginConfigField>& fields = *parsed;
    if (fields.empty()) {
        N8RO_LOG_INFO(
            std::string("ai-commander: deployed config at '") + absoluteForLog(path)
                + "' carried no fields; using compiled defaults.",
            kLogCategory);
        return;
    }

    // Through applyConfigFields, not around it. One commit path means all-or-nothing application,
    // the rebuild-on-backend-change, and the commander.enabled transition record happen here for
    // free and cannot diverge from the host path. The failure is logged by applyConfigFields itself
    // with the offending field named, so nothing is restated here.
    if (applyConfigFields(fields)) {
        N8RO_LOG_INFO(
            std::string("ai-commander: applied ") + std::to_string(fields.size())
                + " field(s) from the deployed config at '" + absoluteForLog(path) + "'.",
            kLogCategory);
    } else {
        N8RO_LOG_WARNING(
            std::string("ai-commander: the deployed config at '") + absoluteForLog(path)
                + "' was rejected in full; compiled defaults remain in force.",
            kLogCategory);
    }

    // applyConfigFields sets this as a side effect of running. Clear it: the file is a default
    // source and must not masquerade as a host application to a LATER, genuine one.
    configExplicitlyApplied_ = false;
}

void AiCommanderPlugin::initialize(n8ro::core::PluginContext& context) {
    shutdown_ = false;
    scriptingApiRegistered_ = false;
    probeAttemptedThisRun_ = false;
    frameCounter_ = 0;

    // OQ-7, resolved 2026-08-01: the host supplies both non-null. The line stays because the field
    // is documented nullable and a future host could change that — the fallback below is what the
    // FR requires either way.
    {
        const std::string line = std::string("ai-commander: PluginContext.services is ")
            + yesNo(context.services != nullptr) + ", PluginContext.threadRunner is "
            + yesNo(context.threadRunner != nullptr) + ".";
        N8RO_LOG_INFO(line, kLogCategory);
    }
    threadRunner_ = context.threadRunner;

    if (context.services == nullptr) {
        N8RO_LOG_WARNING(
            "ai-commander: plugin services unavailable; aiCommander namespace not registered.",
            kLogCategory);
        return;
    }

    void* rawService =
        context.services->getService(n8ro::sim::IScriptingPluginService::kPluginServiceId);
    if (rawService == nullptr) {
        N8RO_LOG_WARNING(
            "ai-commander: scripting plugin service not provided by host; "
            "aiCommander namespace not registered.",
            kLogCategory);
        return;
    }

    auto* scriptingService = static_cast<n8ro::sim::IScriptingPluginService*>(rawService);
    const n8ro::sim::ScriptingApiContext& scriptingContext = scriptingService->scriptingContext();

    entityManager_ = scriptingContext.entityManager;
    world_ = std::make_unique<EngineWorldView>(entityManager_);

    // BEFORE rebuildBackend(): the adapter must be constructed from the configuration that will
    // actually govern the run, not from a compiled default that is superseded a line later
    // (AIC-API-2, v1.7.2). It is also before the recorder opens, so record.path is honoured.
    applyDeployedConfigFile();

    rebuildBackend();

    const CommanderConfig& config = runtime_.config();
    if (config.recordEnabled && !runtime_.recorder().open(config.recordPath, 16 * 1024 * 1024, 4)) {
        // Best-effort: recording must never stall or fail a frame.
        N8RO_LOG_WARNING(
            std::string("ai-commander: could not open the order log at '") + config.recordPath
                + "'; the run will not be recorded.",
            kLogCategory);
    }

    scriptingApiRegistered_ =
        apiModule_.registerWith(scriptingService->missionRegistrar(), scriptingContext);

    if (scriptingApiRegistered_) {
        N8RO_LOG_INFO(
            std::string("ai-commander: registered the aiCommander namespace (14 functions). ")
                + "backend=" + toString(config.backend)
                + " enabled=" + (config.enabled ? "true" : "false")
                + " cadenceS=" + std::to_string(static_cast<int>(config.cadenceS))
                + " maxCommandedEntities=" + std::to_string(config.maxCommandedEntities)
                + " prefixBytes=" + std::to_string(runtime_.promptRenderer().prefix().size())
                + " recording=" + (runtime_.recorder().isOpen() ? runtime_.recorder().path() : "off"),
            kLogCategory);
    } else {
        N8RO_LOG_WARNING(
            "ai-commander: one or more aiCommander functions failed to register.", kLogCategory);
    }
}

void AiCommanderPlugin::runProbeIfPending() {
    if (probeAttemptedThisRun_ || entityManager_ == nullptr) {
        return;
    }

    ProbeReport report = probeRuntimeColumns(*entityManager_);
    if (report.result == ProbeResult::NotRun) {
        return;   // No entity yet; retry on a later frame.
    }
    probeAttemptedThisRun_ = true;

    const std::string verdict = std::string("ai-commander: runtime-column probe ")
        + toString(report.result) + " - " + report.detail;
    if (report.result == ProbeResult::Pass) {
        N8RO_LOG_INFO(verdict, kLogCategory);
    } else {
        // A failed probe disables the commander. It does NOT fall back to a zero velocity: a
        // fabricated stationary own-ship would degrade every order downstream with no failing test
        // and no log line, which is the failure mode AIC-ARCH-4 exists to prevent.
        N8RO_LOG_ERROR(verdict, kLogCategory);
        N8RO_LOG_ERROR(
            "ai-commander: the commander will not issue orders while the probe is failing.",
            kLogCategory);
    }
    runtime_.setProbeReport(std::move(report));
}

EntityPosition AiCommanderPlugin::positionOf(const std::string& entityId) const {
    EntityPosition position;
    if (world_ == nullptr) {
        return position;
    }
    position.known = world_->positionOf(
        entityId, position.latitudeDeg, position.longitudeDeg, position.altitudeHaeM);
    return position;
}

bool AiCommanderPlugin::buildSnapshot(
    const std::string& entityId, double simTimeS, std::int64_t serial, OrderSnapshot& out) const {
    if (entityManager_ == nullptr) {
        return false;
    }
    const n8ro::sim::IEntity* entity = entityManager_->getEntity(entityId);
    if (entity == nullptr) {
        return false;
    }

    // Authored schema leaves — slash-joined paths from schema-reference.json.
    const std::optional<double> lat = n8ro::sim::readComponentFieldReal(
        *entityManager_, entityId, n8ro::sim::kComponentTransform, "positionGeodetic/latitudeDeg");
    const std::optional<double> lon = n8ro::sim::readComponentFieldReal(
        *entityManager_, entityId, n8ro::sim::kComponentTransform, "positionGeodetic/longitudeDeg");
    const std::optional<double> alt = n8ro::sim::readComponentFieldReal(
        *entityManager_, entityId, n8ro::sim::kComponentTransform, "positionGeodetic/altitudeHaeM");
    const std::optional<double> heading = n8ro::sim::readComponentFieldReal(
        *entityManager_, entityId, n8ro::sim::kComponentTransform, "headingDeg");

    // Runtime columns — DOT-joined paths, owned by TransformRuntimeColumns.h. These resolve or
    // return nullopt (OQ-9), and AIC-ARCH-4 has already verified they resolve on this tree.
    const std::optional<double> velN = n8ro::sim::readComponentFieldReal(
        *entityManager_, entityId, n8ro::sim::kComponentTransform,
        n8ro::sim::kTransformColumnVelocityNorth);
    const std::optional<double> velE = n8ro::sim::readComponentFieldReal(
        *entityManager_, entityId, n8ro::sim::kComponentTransform,
        n8ro::sim::kTransformColumnVelocityEast);
    const std::optional<double> velD = n8ro::sim::readComponentFieldReal(
        *entityManager_, entityId, n8ro::sim::kComponentTransform,
        n8ro::sim::kTransformColumnVelocityDown);

    // A nullopt on ANY required field aborts this entity's snapshot for the tick. Filling a hole
    // with a default would produce an order computed from a state the aircraft was never in.
    if (!lat || !lon || !alt || !heading || !velN || !velE || !velD) {
        return false;
    }

    out.entityId = entityId;
    out.simTimeS = simTimeS;
    out.serial = serial;
    out.latitudeDeg = *lat;
    out.longitudeDeg = *lon;
    out.altitudeHaeM = *alt;
    out.headingDeg = *heading;
    out.velNMps = *velN;
    out.velEMps = *velE;
    out.velDMps = *velD;
    out.team = entity->getTeam();
    return true;
}

void AiCommanderPlugin::drainCompletedOrders(double simTimeS, std::int64_t frame) {
    if (world_ == nullptr) {
        return;
    }
    const CommanderConfig& config = runtime_.config();

    for (const std::string& entityId : runtime_.rosterIds()) {
        EntityCommandState* state = runtime_.find(entityId);
        if (state == nullptr) {
            continue;
        }
        std::optional<CandidateOrder> candidate = state->completed.take();
        if (!candidate.has_value()) {
            continue;
        }
        // The request is no longer in flight whatever the outcome, or a single rejection would
        // suppress this entity's requests for the rest of the run.
        state->requestInFlight = false;

        // -- Stage A verdict, acted on here because the worker could not ---------------------------
        // A syntactic rejection must increment its counter and write its record just as a semantic
        // one does; the worker had neither the counters nor the recorder. Handling it here also
        // stops an empty order reaching Stage B, where it would be re-reported as a `roster`
        // failure and double-counted under the wrong reason.
        if (!candidate->stageAAccepted) {
            if (candidate->stageAReason != RejectReason::None) {
                runtime_.countRejection(candidate->stageAReason);
                runtime_.recorder().recordRejected(simTimeS, frame, entityId,
                    candidate->snapshot.serial, candidate->stageAReason,
                    candidate->stageADetail, candidate->rawBody);
            }
            // reason == None means "no order was due" (a replay with nothing at this simulation
            // time, or an empty 204 body). Not a rejection, not an event — nothing to count.
            continue;
        }

        // -- Stage B: semantic validation against live state, here on the simulation thread ------
        StageBRequest request;
        request.commandedEntityId = entityId;
        request.onRoster = true;
        request.snapshotSimTimeS = candidate->snapshot.simTimeS;
        request.currentSimTimeS = simTimeS;
        request.publishedSerial = state->ladder.published.serial;
        request.candidateSerial = candidate->snapshot.serial;
        // B3 validates against the picture that accompanied THIS order's snapshot, not the picture
        // current now — otherwise a target legitimately visible when the order was requested would
        // be rejected merely because inference outlived the cadence window (ADR-3).
        request.reportedTrackIds.reserve(candidate->snapshot.tracks.size());
        for (const TrackReport& track : candidate->snapshot.tracks) {
            request.reportedTrackIds.push_back(track.targetEntityId);
        }
        // B8 takes the same window rule for the same reason: the stores as they were when the
        // order was requested, not as they are now (AIC-VAL-1, v1.7.3).
        request.reportedAmmoCounts.reserve(candidate->snapshot.loadout.size());
        for (const LoadoutReport& hardpoint : candidate->snapshot.loadout) {
            request.reportedAmmoCounts.push_back(hardpoint.ammoCount);
        }

        const StageBOutcome outcome =
            validateStageB(candidate->order, request, config, *world_);

        if (!outcome.accepted) {
            runtime_.countRejection(outcome.reason);
            // The raw body travels with the candidate whether or not Stage A accepted it, so a
            // Stage-B rejection can carry it too. It used to pass an empty string here, which made
            // AIC-DET-1's "carrying the reason and the raw body" false for every Stage-B reason:
            // a `geofence` record said how FAR the waypoint was and never where it WAS, so the
            // Phase 1b live-smoke failure had to be diagnosed by inference instead of read off.
            runtime_.recorder().recordRejected(simTimeS, frame, entityId,
                candidate->snapshot.serial, outcome.reason, outcome.detail, candidate->rawBody);
            if (config.backend == Backend::Replay) {
                // A Stage-B rejection during replay means live state has diverged from the
                // recorded run. That is the signal the reproduction guarantee has broken, and it
                // must be visible rather than silently absorbed as an ordinary rejection.
                runtime_.recorder().recordReplayDivergence(simTimeS, frame, entityId,
                    candidate->snapshot.serial, outcome.reason, outcome.detail);
            }
            // Reject-and-retain: the previously published order stays in force untouched.
            continue;
        }

        // -- publish -----------------------------------------------------------------------------
        // Wholesale replacement. A script can never observe a half-applied order.
        acceptOrder(state->ladder, candidate->order, candidate->snapshot.serial, simTimeS);
        runtime_.countAcceptance(candidate->latencyMs);
        // `t` is PUBLICATION time, which is what replay keys on.
        runtime_.recorder().recordAccepted(simTimeS, frame, candidate->order,
            candidate->snapshot.serial, candidate->latencyMs,
            candidate->tokensIn, candidate->tokensOut);
    }
}

void AiCommanderPlugin::advanceLadders(double simTimeS, std::int64_t frame) {
    const CommanderConfig& config = runtime_.config();

    for (const std::string& entityId : runtime_.rosterIds()) {
        EntityCommandState* state = runtime_.find(entityId);
        if (state == nullptr) {
            continue;
        }
        const FallbackLevel before = state->ladder.level;
        const EntityPosition position = positionOf(entityId);

        if (advanceFallbackLadder(state->ladder, simTimeS, config, position)) {
            // One record and one log line per TRANSITION, never per frame.
            const double sinceS = state->ladder.lastAcceptedSimTimeS < 0.0
                ? -1.0
                : simTimeS - state->ladder.lastAcceptedSimTimeS;
            runtime_.recorder().recordFallback(
                simTimeS, frame, entityId, before, state->ladder.level, sinceS);
            N8RO_LOG_INFO(
                std::string("ai-commander: ") + entityId + " fallback " + toString(before) + " -> "
                    + toString(state->ladder.level),
                kLogCategory);
        }
    }
}

void AiCommanderPlugin::dispatchRequests(double simTimeS, std::int64_t frame) {
    if (!runtime_.isOperational()) {
        return;
    }
    const CommanderConfig& config = runtime_.config();
    ILlmClient* client = runtime_.client();
    if (client == nullptr) {
        return;
    }

    for (const std::string& entityId : runtime_.rosterIds()) {
        EntityCommandState* state = runtime_.find(entityId);
        if (state == nullptr) {
            continue;
        }
        // Per-entity request suppression: never two in flight for one aircraft. Without it a slow
        // backend produces overlapping requests and a self-inflicted slowdown.
        if (state->requestInFlight) {
            continue;
        }
        if (simTimeS - state->lastRequestSimTimeS < config.cadenceS) {
            continue;
        }

        OrderSnapshot snapshot;
        if (!buildSnapshot(entityId, simTimeS, state->nextSerial, snapshot)) {
            continue;   // A required field was unavailable; skip this entity for the tick.
        }
        snapshot.tracks = state->pendingTracks;
        snapshot.loadout = state->pendingLoadout;
        snapshot.situationNote = state->situationNote;
        canonicalizeSnapshot(snapshot);

        // The reported lists are cleared when the snapshot is taken, so a window's report reflects
        // exactly one Tier-1 pass and a script that stops reporting stops contributing tracks
        // rather than leaving a stale picture in place (AIC-API-1, v1.2).
        state->pendingTracks.clear();
        state->pendingLoadout.clear();

        const std::string prompt = runtime_.promptRenderer().render(snapshot);
        runtime_.recorder().recordRequested(simTimeS, frame, snapshot,
            client->backendName(), client->modelName(),
            PromptRenderer::stableHash(runtime_.promptRenderer().renderSuffix(snapshot)),
            PromptRenderer::stableHash(prompt));

        state->requestInFlight = true;
        state->lastRequestSimTimeS = simTimeS;
        ++state->nextSerial;
        runtime_.countRequest();

        // The worker's callable captures ONLY the snapshot (by value), the prompt (by value), the
        // client pointer, and the destination slot. No IEntityManager, no IScenarioManager, no
        // MessageBusPacked, no ISimulationEngine, no ScriptingApiContext, no recorder, no roster.
        LatestWinsSlot<CandidateOrder>* slot = &state->completed;
        auto work = [snapshot, prompt, client, slot]() mutable {
            // The verdict rides across the slot with the candidate. A Stage-A rejection must reach
            // the simulation thread both so the entity's requestInFlight flag clears — otherwise it
            // goes permanently silent — and so the rejection can be counted and recorded there.
            (void)slot->publish(CommanderRuntime::runWorkerCall(std::move(snapshot), prompt, *client));
        };

        if (threadRunner_ != nullptr) {
            // OQ-7 resolved: the host does supply one. This is the primary path.
            (void)threadRunner_->submitBackgroundTask(std::move(work));
        } else {
            // The specified fallback. Running inline is NOT acceptable for a real backend — it
            // would block the update thread for the whole inference — so this path is only viable
            // because stub and replay do no I/O. A network backend with no thread runner leaves
            // the commander disabled, which rebuildBackend() reports.
            if (!loggedNoThreadRunner_) {
                loggedNoThreadRunner_ = true;
                N8RO_LOG_WARNING(
                    "ai-commander: no IThreadRunner; running the no-I/O backend inline. A network "
                    "backend will not be dispatched without one.",
                    kLogCategory);
            }
            work();
        }
    }
}

void AiCommanderPlugin::onTickFrame(double dt) {
    if (shutdown_) {
        return;
    }
    // Timed from the first statement to the last, so the recorded cost is the plugin's whole
    // contribution to the frame rather than a favourably-chosen slice of it.
    const auto started = std::chrono::steady_clock::now();

    simTimeS_ += dt;
    ++frameCounter_;
    runtime_.setCurrentSimTimeS(simTimeS_);

    runProbeIfPending();

    // Order matters. Draining first means the newest information a worker produced is published
    // before the ladder decides whether this entity has gone stale, so an order that arrived this
    // very frame is never counted against its own freshness.
    drainCompletedOrders(simTimeS_, frameCounter_);
    advanceLadders(simTimeS_, frameCounter_);
    dispatchRequests(simTimeS_, frameCounter_);

    const auto elapsed = std::chrono::steady_clock::now() - started;
    runtime_.frameCost().record(
        std::chrono::duration<double, std::micro>(elapsed).count());
}

void AiCommanderPlugin::writeRunEndStats() {
    // "All metrics are exposed through aiCommander.getStats() and written to the order log at run
    // end" (Observability). Emitted at run end rather than periodically: a per-frame or per-minute
    // stats line would bury the order records that are the log's actual purpose.
    const FrameCostHistogram& cost = runtime_.frameCost();
    if (cost.totalFrames() == 0) {
        return;
    }

    const std::string summary = std::string("ai-commander: run end - frames=")
        + std::to_string(cost.totalFrames())
        + " frameCost p50=" + std::to_string(cost.p50Us() / 1000.0)
        + "ms p95=" + std::to_string(cost.p95Us() / 1000.0)
        + "ms p99=" + std::to_string(cost.p99Us() / 1000.0)
        + "ms max=" + std::to_string(cost.maxUs() / 1000.0) + "ms"
        + " | requested=" + std::to_string(runtime_.stats().requested)
        + " accepted=" + std::to_string(runtime_.stats().accepted)
        + " rejected=" + std::to_string(runtime_.stats().rejected);
    N8RO_LOG_INFO(summary, kLogCategory);

    runtime_.recorder().recordCommanderState(simTimeS_, false, runtime_.statsJson());
}

void AiCommanderPlugin::onStop() {
    writeRunEndStats();

    // A new scenario brings a new entity set, so neither the probe verdict nor any per-entity
    // state carries over.
    probeAttemptedThisRun_ = false;
    simTimeS_ = 0.0;
    frameCounter_ = 0;
    runtime_.reset();
}

void AiCommanderPlugin::shutdown() {
    // A process killed without a clean onStop() must still leave its measurements behind — which
    // is the common case for a headless run.
    if (!shutdown_) {
        writeRunEndStats();
    }
    shutdown_ = true;
    runtime_.recorder().close();
    entityManager_ = nullptr;
    threadRunner_ = nullptr;
    world_.reset();
}

std::vector<n8ro::core::PluginConfigField> AiCommanderPlugin::getConfigFields() const {
    return toConfigFields(runtime_.config());
}

bool AiCommanderPlugin::applyConfigFields(
    const std::vector<n8ro::core::PluginConfigField>& fields) {
    // Recorded even when the parse below fails: the host ASKED, and a host that asked and was
    // refused must not then have a deployed default quietly applied in place of what it wanted.
    configExplicitlyApplied_ = true;

    CommanderConfig parsed;
    std::string error;
    if (!tryParseConfigFields(fields, runtime_.config(), parsed, error)) {
        // All-or-nothing: nothing has been written, so every prior value still stands. The reason
        // is logged because a rejection the operator cannot diagnose is indistinguishable from the
        // plugin ignoring them.
        N8RO_LOG_WARNING(
            std::string("ai-commander: rejected config - ") + error
                + ". No field was applied; previous configuration retained.",
            kLogCategory);
        return false;
    }

    const CommanderConfig previous = runtime_.config();
    runtime_.setConfig(parsed);

    const bool backendChanged = parsed.backend != previous.backend
        || parsed.replayPath != previous.replayPath
        || parsed.doctrinePath != previous.doctrinePath;
    if (backendChanged) {
        // Takes effect without restarting the process (AIC-ARCH-3). Any in-flight request from the
        // previous backend is discarded on completion, because its snapshot serial will no longer
        // be newer than what has since been published.
        rebuildBackend();
    }

    if (parsed.enabled != previous.enabled) {
        N8RO_LOG_INFO(
            std::string("ai-commander: commander.enabled -> ") + (parsed.enabled ? "true" : "false")
                + " (backend=" + toString(parsed.backend) + ").",
            kLogCategory);
        runtime_.recorder().recordCommanderState(simTimeS_, parsed.enabled,
            std::string("backend=") + toString(parsed.backend));
    }
    return true;
}

} // namespace arkheon::aicommander

extern "C" {

N8RO_CORE_API n8ro::core::IPlugin* create_plugin() {
    return new arkheon::aicommander::AiCommanderPlugin();
}

N8RO_CORE_API void destroy_plugin(n8ro::core::IPlugin* plugin) {
    delete plugin;
}

N8RO_CORE_API const char* get_plugin_signature() {
    return "N8RO_PLUGIN_V1";
}

} // extern "C"
