#include "CommanderRuntime.h"

#include "OrderSchema.h"
#include "OrderValidatorStageA.h"

#include <core/json/JsonValue.h>

#include <algorithm>
#include <string>

namespace arkheon::aicommander {

namespace {

// Tier-1-supplied strings are sanitized on INGRESS, before they can reach a prompt or a record.
// Tier 1 is trusted to choose WHICH tracks to report; it is not trusted to have sanitized their
// ids, which originate from the same external feeds the threat model names.
constexpr std::size_t kMaxSituationNoteChars = 256;

} // namespace

CommanderRuntime::~CommanderRuntime() = default;

void CommanderRuntime::setConfig(const CommanderConfig& config) {
    config_ = config;
}

bool CommanderRuntime::isOperational() const {
    return config_.enabled
        && client_ != nullptr
        && probeReport_.result == ProbeResult::Pass;
}

bool CommanderRuntime::requestCommand(const std::string& entityId) {
    if (entityId.empty() || !isSanitizedText(entityId)) {
        return false;
    }
    const auto existing = entities_.find(entityId);
    if (existing != entities_.end()) {
        return true;   // Idempotent, per AIC-API-1.
    }
    if (entities_.size() >= static_cast<std::size_t>(config_.maxCommandedEntities)) {
        // A hard cap, not a soft one: it is what keeps per-entity orders from drifting into
        // multi-entity coordination the order schema has no answer for.
        return false;
    }
    entities_.emplace(entityId, std::make_unique<EntityCommandState>());
    return true;
}

bool CommanderRuntime::releaseCommand(const std::string& entityId) {
    return entities_.erase(entityId) > 0;
}

bool CommanderRuntime::isOnRoster(const std::string& entityId) const {
    return entities_.find(entityId) != entities_.end();
}

std::vector<std::string> CommanderRuntime::rosterIds() const {
    std::vector<std::string> ids;
    ids.reserve(entities_.size());
    for (const auto& entry : entities_) {
        ids.push_back(entry.first);
    }
    return ids;
}

EntityCommandState* CommanderRuntime::find(const std::string& entityId) {
    const auto it = entities_.find(entityId);
    return it == entities_.end() ? nullptr : it->second.get();
}

const EntityCommandState* CommanderRuntime::find(const std::string& entityId) const {
    const auto it = entities_.find(entityId);
    return it == entities_.end() ? nullptr : it->second.get();
}

bool CommanderRuntime::reportTrack(
    const std::string& entityId, const std::string& targetEntityId, double rangeM, double snrDb) {
    if (!config_.enabled) {
        // Reporting into a disabled commander accumulates no state (AIC-API-1, v1.2).
        return false;
    }
    EntityCommandState* state = find(entityId);
    if (state == nullptr) {
        return false;
    }
    if (targetEntityId.empty() || targetEntityId.size() > kMaxEntityIdChars
        || !isSanitizedText(targetEntityId)) {
        return false;
    }
    if (!std::isfinite(rangeM) || !std::isfinite(snrDb) || rangeM < 0.0) {
        return false;
    }

    // Idempotent per target: a repeat replaces the earlier row rather than duplicating it, so a
    // script that reports inside a loop it runs twice does not double its own picture.
    for (TrackReport& existing : state->pendingTracks) {
        if (existing.targetEntityId == targetEntityId) {
            existing.rangeM = rangeM;
            existing.snrDb = snrDb;
            return true;
        }
    }
    if (state->pendingTracks.size() >= static_cast<std::size_t>(config_.maxTracksInPrompt)) {
        // Refuse rather than evict. Evicting would silently drop a track the script believed it
        // had reported, and the script has no way to notice.
        return false;
    }
    state->pendingTracks.push_back(TrackReport{targetEntityId, rangeM, snrDb});
    return true;
}

bool CommanderRuntime::reportLoadout(
    const std::string& entityId, const std::string& hardpointName,
    const std::string& weaponProfileName, int ammoCount, int ammoMax) {
    if (!config_.enabled) {
        return false;
    }
    EntityCommandState* state = find(entityId);
    if (state == nullptr) {
        return false;
    }
    if (hardpointName.empty() || hardpointName.size() > kMaxEntityIdChars
        || !isSanitizedText(hardpointName) || !isSanitizedText(weaponProfileName)) {
        return false;
    }
    if (ammoCount < 0 || ammoMax < 0) {
        return false;
    }

    for (LoadoutReport& existing : state->pendingLoadout) {
        if (existing.hardpointName == hardpointName) {
            existing.weaponProfileName = weaponProfileName;
            existing.ammoCount = ammoCount;
            existing.ammoMax = ammoMax;
            return true;
        }
    }
    if (state->pendingLoadout.size() >= static_cast<std::size_t>(config_.maxTracksInPrompt)) {
        return false;
    }
    state->pendingLoadout.push_back(
        LoadoutReport{hardpointName, weaponProfileName, ammoCount, ammoMax});
    return true;
}

bool CommanderRuntime::setSituationNote(const std::string& entityId, const std::string& text) {
    EntityCommandState* state = find(entityId);
    if (state == nullptr) {
        return false;
    }
    // Truncated and stripped rather than refused: a note is advisory context, and rejecting it for
    // one stray character would cost the model information for no safety gain.
    state->situationNote = sanitizeText(text, kMaxSituationNoteChars);
    return true;
}

const PublishedOrder* CommanderRuntime::publishedOrder(const std::string& entityId) const {
    const EntityCommandState* state = find(entityId);
    if (state == nullptr || !state->ladder.published.valid) {
        return nullptr;
    }
    return &state->ladder.published;
}

void CommanderRuntime::setClient(std::unique_ptr<ILlmClient> client) {
    client_ = std::move(client);
}

CandidateOrder CommanderRuntime::runWorkerCall(
    OrderSnapshot snapshot, const std::string& prompt, ILlmClient& client,
    bool& outAccepted, RejectReason& outReason, std::string& outDetail, std::string& outRawBody,
    std::size_t prefixLength) {
    // Everything this function can reach is its arguments. There is no `this`, no runtime, no
    // recorder, no entity manager — which is what makes AIC-ARCH-2's "the worker captures only a
    // POD snapshot and the ILlmClient" a property of the signature rather than a review note.
    CandidateOrder candidate;
    candidate.snapshot = snapshot;

    LlmRequest request;
    request.prompt = prompt;
    request.snapshot = std::move(snapshot);
    request.prefixLength = prefixLength;

    const LlmResult result = client.request(request);
    candidate.latencyMs = result.latencyMs;
    candidate.tokensIn = result.tokensIn;
    candidate.tokensOut = result.tokensOut;
    outRawBody = result.body;

    // Stage A check A1: a transport failure is `completed == false`, distinct from a completed
    // request carrying a non-2xx status.
    if (!result.completed) {
        outAccepted = false;
        outReason = RejectReason::Transport;
        outDetail = result.transportDetail.empty() ? "transport failure" : result.transportDetail;
        return candidate;
    }
    if (result.statusCode == 204 || result.body.empty()) {
        // Nothing due (replay) or an empty completed body. Not a rejection to be counted against
        // the model, and not a transport failure either — no order, no event.
        outAccepted = false;
        outReason = RejectReason::None;
        outDetail = "no order due";
        return candidate;
    }
    if (result.statusCode < 200 || result.statusCode >= 300) {
        outAccepted = false;
        outReason = RejectReason::Transport;
        outDetail = "HTTP status " + std::to_string(result.statusCode);
        return candidate;
    }

    // A2 needs to know how this backend wrapped the order, and it takes that as a value rather than
    // as a client — so Stage A stays a pure function and holds nothing live.
    const StageAOutcome outcome =
        validateStageA(result.body, candidate.snapshot.entityId, client.envelopeFormat());
    outAccepted = outcome.accepted;
    outReason = outcome.reason;
    outDetail = outcome.detail;
    if (outcome.accepted) {
        candidate.order = outcome.order;
    }
    return candidate;
}

CandidateOrder CommanderRuntime::runWorkerCall(
    OrderSnapshot snapshot, const std::string& prompt, ILlmClient& client,
    std::size_t prefixLength) {
    bool accepted = false;
    RejectReason reason = RejectReason::None;
    std::string detail;
    std::string rawBody;
    CandidateOrder candidate = runWorkerCall(
        std::move(snapshot), prompt, client, accepted, reason, detail, rawBody, prefixLength);

    // The verdict travels WITH the candidate rather than being discarded here. The worker cannot
    // touch the counters or the recorder — both are simulation-thread-only — so the only way a
    // Stage-A rejection reaches them is by riding across the slot.
    candidate.stageAAccepted = accepted;
    candidate.stageAReason = reason;
    candidate.stageADetail = std::move(detail);
    candidate.rawBody = std::move(rawBody);
    if (!accepted) {
        candidate.order = Order{};
    }
    return candidate;
}

void CommanderRuntime::countRequest() {
    ++stats_.requested;
}

void CommanderRuntime::countAcceptance(std::int64_t latencyMs) {
    ++stats_.accepted;
    stats_.lastLatencyMs = latencyMs;
}

void CommanderRuntime::countRejection(RejectReason reason) {
    if (reason == RejectReason::None) {
        return;
    }
    ++stats_.rejected;
    ++stats_.rejectByReason[toString(reason)];
}

void CommanderRuntime::reset() {
    entities_.clear();
    probeReport_ = ProbeReport{};
    frameCost_.reset();
}

std::string CommanderRuntime::statsJson() const {
    // Built through JsonValue rather than by concatenation so a detail string carrying a quote
    // cannot produce a malformed document. A health surface that can be broken by its own error
    // message is not a health surface.
    n8ro::core::JsonValue root = n8ro::core::JsonValue::object();

    (void)root.setBool("enabled", config_.enabled);
    (void)root.setBool("operational", isOperational());
    (void)root.setString("backend", toString(config_.backend));
    (void)root.setString("model", client_ != nullptr ? client_->modelName() : std::string());
    (void)root.setInt64("rosterSize", static_cast<std::int64_t>(entities_.size()));

    (void)root.setInt64("requested", stats_.requested);
    (void)root.setInt64("accepted", stats_.accepted);
    (void)root.setInt64("rejected", stats_.rejected);
    (void)root.setInt64("timeouts", stats_.timeouts);
    (void)root.setInt64("droppedSnapshots", stats_.droppedSnapshots);
    (void)root.setInt64("lastLatencyMs", stats_.lastLatencyMs);

    n8ro::core::JsonValue byReason = n8ro::core::JsonValue::object();
    for (const auto& entry : stats_.rejectByReason) {
        (void)byReason.setInt64(entry.first, entry.second);
    }
    (void)root.set("rejectByReason", byReason);

    // Per-frame plugin cost, in milliseconds to match the budget's units. This is the measured
    // answer to "does the commander fit inside the frame", and it is the number the runbook's
    // aicmd.frame.p95Ms alert reads.
    n8ro::core::JsonValue frame = n8ro::core::JsonValue::object();
    (void)frame.setDouble("p50Ms", frameCost_.p50Us() / 1000.0);
    (void)frame.setDouble("p95Ms", frameCost_.p95Us() / 1000.0);
    (void)frame.setDouble("p99Ms", frameCost_.p99Us() / 1000.0);
    (void)frame.setDouble("maxMs", frameCost_.maxUs() / 1000.0);
    (void)frame.setInt64("frames", frameCost_.totalFrames());
    (void)root.set("frame", frame);

    n8ro::core::JsonValue probe = n8ro::core::JsonValue::object();
    (void)probe.setString("result", toString(probeReport_.result));
    (void)probe.setString("detail", probeReport_.detail);
    (void)probe.setString("entityId", probeReport_.probedEntityId);
    (void)root.set("runtimeColumnProbe", probe);

    // Per-entity ladder levels: the operator-facing answer to "is the commander actually driving
    // this aircraft, or has it fallen back?"
    n8ro::core::JsonValue levels = n8ro::core::JsonValue::object();
    for (const auto& entry : entities_) {
        n8ro::core::JsonValue entity = n8ro::core::JsonValue::object();
        (void)entity.setString("level", toString(entry.second->ladder.level));
        (void)entity.setInt64("serial", entry.second->ladder.published.serial);
        (void)entity.setInt64("tracksReported",
            static_cast<std::int64_t>(entry.second->pendingTracks.size()));
        (void)levels.set(entry.first, entity);
    }
    (void)root.set("entities", levels);

    return root.toString();
}

} // namespace arkheon::aicommander
