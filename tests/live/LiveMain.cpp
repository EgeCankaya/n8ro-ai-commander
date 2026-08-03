// Phase 1b live validation harness.
//
// This is NOT part of the unit suite and must never be added to ai-commander-tests.vcxproj. The
// PRD's CI requirement is explicit: the unit, stubbed-integration, and replay suites SHALL run on
// every build and SHALL NOT require an inference server or network access. Everything here needs
// both, so it is a separate binary invoked by hand.
//
// It drives the REAL LocalLlmClient, the REAL PromptRenderer, and the REAL Stage-A validator
// through CommanderRuntime::runWorkerCall - the same call the worker makes in production. A
// re-implementation in a scripting language would measure a program nobody ships.
//
// Measures, per PRD §Phase 1b:
//   soak  - acceptance rate over N orders, reject.schema and reject.shape reported SEPARATELY,
//           latency p50/p95/p99
//   cold  - the first order of a run completes rather than timing out, from a genuinely evicted
//           model (keep_alive: 0 forces the eviction, so "cold" means cold)
//   h2    - prefix-stable vs perturbed-prefix p95, whatever the outcome
//
// Usage: ai-commander-live-tests.exe [--orders N] [--mode soak|cold|h2|geo|all] [--model TAG]
//                                    [--base-url URL] [--no-format] [--out PATH]

#include "CommanderConfig.h"
#include "CommanderRuntime.h"
#include "LocalLlmClient.h"
#include "OrderSchema.h"
#include "OrderValidatorStageB.h"
#include "PromptRenderer.h"
#include "RejectReason.h"
#include "Snapshot.h"

#include <core/net/HttpClientFactory.h>
#include <core/net/IHttpClient.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace arkheon::aicommander;

// -- the situation set --------------------------------------------------------------------------
//
// Deliberately spread across every posture the vocabulary offers, including the awkward ones. A set
// that only ever presents "two bandits closing" measures one branch of the decision space and
// reports it as if it were the whole. These are the six situations the Phase 1b research spikes
// used, so the C++ numbers are comparable with them.
struct Situation {
    const char* label;
    OrderSnapshot snapshot;
};

OrderSnapshot baseSnapshot() {
    OrderSnapshot snapshot;
    snapshot.entityId = "RedSu35_01";
    snapshot.team = "Red";
    snapshot.latitudeDeg = 13.49;
    snapshot.longitudeDeg = 145.00;
    snapshot.altitudeHaeM = 10000.0;
    snapshot.headingDeg = 270.0;
    snapshot.velNMps = 0.0;
    snapshot.velEMps = -220.0;
    snapshot.velDMps = 0.0;
    snapshot.loadout.push_back(LoadoutReport{"STA1", "R77_BVR", 4, 4});
    return snapshot;
}

std::vector<Situation> buildSituations() {
    std::vector<Situation> situations;

    {
        OrderSnapshot s = baseSnapshot();
        s.simTimeS = 120.0;
        s.tracks.push_back(TrackReport{"BlueF16_01", 62000.0, 14.0});
        s.tracks.push_back(TrackReport{"BlueF16_02", 71000.0, 11.0});
        canonicalizeSnapshot(s);
        situations.push_back({"two tracks, full rail", s});
    }
    {
        OrderSnapshot s = baseSnapshot();
        s.simTimeS = 30.0;
        s.longitudeDeg = 145.30;
        canonicalizeSnapshot(s);
        situations.push_back({"no tracks at all", s});
    }
    {
        OrderSnapshot s = baseSnapshot();
        s.simTimeS = 480.0;
        s.longitudeDeg = 144.85;
        s.altitudeHaeM = 7000.0;
        s.headingDeg = 90.0;
        s.velEMps = 240.0;
        s.loadout.clear();
        s.loadout.push_back(LoadoutReport{"STA1", "R77_BVR", 0, 4});
        s.tracks.push_back(TrackReport{"BlueF16_02", 18000.0, 22.0});
        canonicalizeSnapshot(s);
        situations.push_back({"winchester, one track close", s});
    }
    {
        OrderSnapshot s = baseSnapshot();
        s.simTimeS = 300.0;
        s.longitudeDeg = 144.90;
        s.altitudeHaeM = 9000.0;
        s.headingDeg = 260.0;
        s.velEMps = -300.0;
        s.loadout.clear();
        s.loadout.push_back(LoadoutReport{"STA1", "R77_BVR", 2, 4});
        s.tracks.push_back(TrackReport{"BlueAim120_07", 9000.0, 28.0});
        s.tracks.push_back(TrackReport{"BlueF16_01", 24000.0, 19.0});
        s.situationNote = "Inbound munition closing fast inside its likely envelope.";
        canonicalizeSnapshot(s);
        situations.push_back({"munition inbound", s});
    }
    {
        OrderSnapshot s = baseSnapshot();
        s.simTimeS = 60.0;
        s.longitudeDeg = 145.20;
        s.situationNote = "On station, no contacts, holding area assignment.";
        canonicalizeSnapshot(s);
        situations.push_back({"static, nothing to do", s});
    }
    {
        OrderSnapshot s = baseSnapshot();
        s.simTimeS = 200.0;
        s.longitudeDeg = 144.95;
        s.velEMps = -280.0;
        s.loadout.clear();
        s.loadout.push_back(LoadoutReport{"STA1", "R77_BVR", 3, 4});
        s.tracks.push_back(TrackReport{"BlueF16_01", 31000.0, 20.0});
        s.situationNote = "Missile away at BlueF16_01 twelve seconds ago; assessment window open.";
        canonicalizeSnapshot(s);
        situations.push_back({"shot away, supporting", s});
    }

    return situations;
}

// -- results ------------------------------------------------------------------------------------

struct Tally {
    int requested = 0;
    int accepted = 0;
    std::map<std::string, int> rejectByReason;
    std::vector<std::int64_t> latencies;
    std::map<std::string, int> postures;
    std::vector<std::string> firstFailures;   // capped; enough to diagnose without a wall of text

    // Which posture each situation drew, and one sample rationale. This is the order-QUALITY view,
    // and it is separate from acceptance on purpose: a run can be 100 % accepted and still be
    // tactically wrong, and only this column shows it. It is the input to the H1 assessment.
    std::map<std::string, std::map<std::string, int>> postureBySituation;
    std::map<std::string, std::string> sampleReason;

    [[nodiscard]] double acceptancePct() const {
        return requested == 0 ? 0.0 : 100.0 * accepted / requested;
    }
    [[nodiscard]] double reasonPct(const char* reason) const {
        const auto it = rejectByReason.find(reason);
        const int count = it == rejectByReason.end() ? 0 : it->second;
        return requested == 0 ? 0.0 : 100.0 * count / requested;
    }
    [[nodiscard]] std::int64_t percentile(double fraction) const {
        if (latencies.empty()) {
            return 0;
        }
        std::vector<std::int64_t> sorted = latencies;
        std::sort(sorted.begin(), sorted.end());
        std::size_t index = static_cast<std::size_t>(fraction * sorted.size());
        if (index >= sorted.size()) {
            index = sorted.size() - 1;
        }
        return sorted[index];
    }
};

void record(Tally& tally, const CandidateOrder& candidate, const std::string& situation) {
    ++tally.requested;
    tally.latencies.push_back(candidate.latencyMs);
    if (candidate.stageAAccepted) {
        ++tally.accepted;
        const std::string posture = toString(candidate.order.posture);
        ++tally.postures[posture];
        ++tally.postureBySituation[situation][posture];
        if (tally.sampleReason.find(situation) == tally.sampleReason.end()) {
            tally.sampleReason[situation] = posture + " - " + candidate.order.reason;
        }
        return;
    }
    const std::string reason = toString(candidate.stageAReason);
    ++tally.rejectByReason[reason];
    if (tally.firstFailures.size() < 8) {
        tally.firstFailures.push_back(reason + ": " + candidate.stageADetail);
    }
}

std::string formatTally(const std::string& title, const Tally& tally) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "\n=== " << title << " ===\n";
    out << "  requested        " << tally.requested << "\n";
    out << "  accepted         " << tally.accepted << "  ("
        << std::fixed << std::setprecision(1) << tally.acceptancePct() << " %)\n";
    // Reported SEPARATELY and labelled, because the < 1 % bar is schema-only (PRD §Phase 1b).
    out << "  reject.schema    " << std::fixed << std::setprecision(2)
        << tally.reasonPct("schema") << " %   [gate: < 1 %]\n";
    out << "  reject.shape     " << std::fixed << std::setprecision(2)
        << tally.reasonPct("shape") << " %   [not held to the 1 % bar; see PRD Corrections 13]\n";
    for (const auto& entry : tally.rejectByReason) {
        out << "    - " << std::setw(12) << std::left << entry.first << entry.second << "\n";
    }
    out << "  latency p50/p95/p99  " << tally.percentile(0.50) << " / " << tally.percentile(0.95)
        << " / " << tally.percentile(0.99) << " ms\n";
    out << "  postures         ";
    for (const auto& entry : tally.postures) {
        out << entry.first << "=" << entry.second << " ";
    }
    out << "\n";
    if (!tally.firstFailures.empty()) {
        out << "  first failures:\n";
        for (const std::string& failure : tally.firstFailures) {
            out << "    - " << failure << "\n";
        }
    }
    if (!tally.postureBySituation.empty()) {
        out << "  order quality (posture chosen per situation, and one sample rationale):\n";
        for (const auto& entry : tally.postureBySituation) {
            out << "    " << std::setw(30) << std::left << entry.first << " ";
            for (const auto& posture : entry.second) {
                out << posture.first << "=" << posture.second << " ";
            }
            out << "\n";
            const auto reason = tally.sampleReason.find(entry.first);
            if (reason != tally.sampleReason.end()) {
                out << "      \"" << reason->second << "\"\n";
            }
        }
    }
    return out.str();
}

// -- the run ------------------------------------------------------------------------------------

struct Options {
    int orders = 200;
    std::string mode = "all";
    std::string baseUrl = "http://localhost:11434";
    std::string model = "qwen2.5:7b-instruct-q8_0";
    std::string doctrinePath = "data/doctrine.txt";
    std::string outPath;
    bool grammarEnabled = true;
};

std::string readFile(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {};
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

// Forces Ollama to unload the model, so a "cold" measurement is actually cold rather than a
// measurement of an already-resident model.
bool evictModel(const Options& options) {
    std::unique_ptr<n8ro::core::IHttpClient> http = n8ro::core::HttpClientFactory::create();
    if (http == nullptr) {
        return false;
    }
    n8ro::core::HttpRequest request;
    request.method = n8ro::core::HttpMethod::Post;
    request.url = options.baseUrl + "/api/generate";
    request.contentType = "application/json";
    request.body = "{\"model\":\"" + options.model + "\",\"prompt\":\"\",\"keep_alive\":0}";
    request.timeoutS = 60;
    const auto response = http->send(request);
    return response.has_value();
}

LocalClientConfig clientConfigFrom(const Options& options, const CommanderConfig& config) {
    LocalClientConfig local;
    local.baseUrl = options.baseUrl;
    local.model = options.model;
    local.temperature = config.localTemperature;
    local.grammarEnabled = options.grammarEnabled;
    local.timeoutS = config.requestTimeoutS;
    return local;
}

// Perturbs the prefix in the one documented way, so H2 measures prefix stability and not some
// other difference that crept in alongside it.
std::string perturbPrefix(const std::string& prompt, int index) {
    const std::size_t marker = prompt.find("POSTURES:");
    if (marker == std::string::npos) {
        return prompt;
    }
    std::string padding(static_cast<std::size_t>(1 + (index % 7)), ' ');
    return prompt.substr(0, marker) + padding + prompt.substr(marker);
}

// -- geo: WHERE does the model point? -------------------------------------------------------------
//
// The Phase 1b live smoke failed acceptance on two Stage-B `geofence` rejections, waypoints ~5,300
// km from own-ship. The order log records how FAR a rejected waypoint was but not WHERE it was
// (Stage-B rejections carry an empty rawBody), so the cause had to be inferred. This mode measures
// it directly and offline: run each authored situation, and for every accepted order print the
// waypoint, its distance from own-ship, and whether the default geofence would have taken it.
//
// The hypothesis under test: the doctrine says "egress toward the home field" and, by design,
// carries no coordinates, and the volatile suffix supplies no reference geography beyond own-ship
// position - so a posture needing a destination has nothing to derive one from and the model
// invents a plausible-looking coordinate.
// Situations that FORCE a waypoint-choosing posture. Deliberately separate from
// buildSituations(): those six are the authored soak set the PRD's 200-order figures were measured
// against, and widening them would silently break the comparison. These exist only here.
//
// The six authored situations turn out to draw `hold` as their only waypoint posture, and `hold`
// is the easy case - the correct answer is "where you already are", which the model gets right at
// range 0 m. `ingress` and `rtb` are the cases where the model has to produce a destination it was
// never given, and they are what the live run actually rejected.
std::vector<Situation> buildWaypointSituations() {
    std::vector<Situation> situations;

    {
        // A contact far enough away that doctrine's "if there is a reason to be somewhere else,
        // ingress there" applies. NOTE what the prompt can and cannot say about that contact:
        // AIC-SEC-2 gives a track row exactly three scalars - id, rangeM, snrDb. No bearing. No
        // position. So "ingress toward the contact" is a destination the prompt does not contain.
        OrderSnapshot s = baseSnapshot();
        s.simTimeS = 60.0;
        s.tracks.push_back(TrackReport{"BlueF16_01", 128000.0, 9.0});
        s.situationNote = "Contact held at long range; commit decision pending.";
        canonicalizeSnapshot(s);
        situations.push_back({"distant contact, must close", s});
    }
    {
        // Winchester with NO live threat. Doctrine: "the answer to being out of missiles is to
        // leave" and "egress toward the home field". The home field is not in the prompt either.
        OrderSnapshot s = baseSnapshot();
        s.simTimeS = 700.0;
        s.loadout.clear();
        s.loadout.push_back(LoadoutReport{"STA1", "R77_BVR", 0, 4});
        s.situationNote = "winchester";
        canonicalizeSnapshot(s);
        situations.push_back({"winchester, no threat (rtb)", s});
    }
    {
        // Task complete, stores intact - the other documented route to rtb.
        OrderSnapshot s = baseSnapshot();
        s.simTimeS = 900.0;
        s.situationNote = "Assigned task complete; no further contacts assigned.";
        canonicalizeSnapshot(s);
        situations.push_back({"task complete (rtb)", s});
    }
    return situations;
}

std::string runGeoProbe(const Options& options, const CommanderConfig& config,
                        const PromptRenderer& renderer, int repeats) {
    LocalLlmClient client(clientConfigFrom(options, config));
    std::vector<Situation> situations = buildSituations();
    const std::vector<Situation> waypointCases = buildWaypointSituations();
    situations.insert(situations.end(), waypointCases.begin(), waypointCases.end());

    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "\n=== geo probe: where the model points ===\n";
    out << "  geofence bound " << static_cast<long long>(config.geofenceRadiusM) << " m\n\n";

    int withWaypoint = 0;
    int outsideFence = 0;
    double worstM = 0.0;

    for (const Situation& situation : situations) {
        for (int r = 0; r < repeats; ++r) {
            const std::string prompt = renderer.render(situation.snapshot);
            const CandidateOrder candidate =
                CommanderRuntime::runWorkerCall(situation.snapshot, prompt, client);

            out << "  " << std::left << std::setw(30) << situation.label << std::right;
            if (!candidate.stageAAccepted) {
                out << "  REJECTED " << toString(candidate.stageAReason) << "\n";
                continue;
            }

            const Order& order = candidate.order;
            out << "  " << std::setw(8) << toString(order.posture);
            if (!postureRequiresWaypoint(order.posture)) {
                out << "  (no waypoint for this posture)\n";
                continue;
            }

            ++withWaypoint;
            const double rangeM = slantRangeM(
                situation.snapshot.latitudeDeg, situation.snapshot.longitudeDeg,
                situation.snapshot.altitudeHaeM,
                order.latitudeDeg, order.longitudeDeg, order.altitudeHaeM);
            worstM = std::max(worstM, rangeM);

            const bool inside = rangeM <= config.geofenceRadiusM;
            if (!inside) {
                ++outsideFence;
            }
            out << "  own(" << std::fixed << std::setprecision(3)
                << situation.snapshot.latitudeDeg << ", " << situation.snapshot.longitudeDeg << ")"
                << "  -> wp(" << order.latitudeDeg << ", " << order.longitudeDeg << ", "
                << std::setprecision(0) << order.altitudeHaeM << " m)"
                << "  range " << static_cast<long long>(rangeM) << " m  "
                << (inside ? "inside" : "*** OUTSIDE THE FENCE ***") << "\n";
        }
    }

    out << "\n  waypoint-carrying orders " << withWaypoint
        << ", outside the fence " << outsideFence;
    if (withWaypoint > 0) {
        out << "  (" << std::fixed << std::setprecision(1)
            << (100.0 * outsideFence / withWaypoint) << " %)";
    }
    out << "\n  furthest waypoint " << static_cast<long long>(worstM) << " m\n";
    return out.str();
}

Tally runSoak(const Options& options, const CommanderConfig& config, const PromptRenderer& renderer,
              int orders, bool perturb) {
    LocalLlmClient client(clientConfigFrom(options, config));
    const std::vector<Situation> situations = buildSituations();

    Tally tally;
    for (int i = 0; i < orders; ++i) {
        const Situation& situation = situations[static_cast<std::size_t>(i) % situations.size()];
        std::string prompt = renderer.render(situation.snapshot);
        if (perturb) {
            prompt = perturbPrefix(prompt, i);
        }
        // The production call, not a re-implementation of it: prompt -> adapter -> A1 -> A2 -> A3
        // -> ... -> A7, with the Stage-A verdict packed into the candidate exactly as the worker
        // receives it.
        const CandidateOrder candidate =
            CommanderRuntime::runWorkerCall(situation.snapshot, prompt, client);
        record(tally, candidate, situation.label);

        if ((i + 1) % 20 == 0) {
            std::cout << "    " << (i + 1) << "/" << orders << " orders, "
                      << std::fixed << std::setprecision(1) << tally.acceptancePct()
                      << " % accepted\n"
                      << std::flush;
        }
    }
    return tally;
}

std::string runColdCheck(const Options& options, const CommanderConfig& config,
                         const PromptRenderer& renderer) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "\n=== cold first order (PRD §Phase 1b: the FIRST order of a run must complete) ===\n";

    if (!evictModel(options)) {
        out << "  [WARN] could not evict the model; the measurement below may be warm and therefore\n"
               "         vacuous. Treat it as unmeasured rather than as a pass.\n";
    } else {
        out << "  model evicted (keep_alive: 0), so 'cold' means cold\n";
    }

    // A fresh adapter, exactly as a run start would build one.
    LocalLlmClient client(clientConfigFrom(options, config));
    const std::vector<Situation> situations = buildSituations();

    const CandidateOrder first =
        CommanderRuntime::runWorkerCall(situations[0].snapshot, renderer.render(situations[0].snapshot), client);
    const CandidateOrder second =
        CommanderRuntime::runWorkerCall(situations[1].snapshot, renderer.render(situations[1].snapshot), client);

    const bool completed = first.stageAAccepted || first.stageAReason != RejectReason::Transport;
    out << "  first order   " << first.latencyMs << " ms   "
        << (first.stageAAccepted ? "accepted" : (std::string("rejected ") + toString(first.stageAReason)))
        << "\n";
    out << "  second order  " << second.latencyMs << " ms   "
        << (second.stageAAccepted ? "accepted" : (std::string("rejected ") + toString(second.stageAReason)))
        << "\n";
    out << "  timeout budget " << config.requestTimeoutS << " s\n";
    out << "  VERDICT: " << (completed
        ? "PASS - the first order completed rather than timing out"
        : "FAIL - the first order did not complete") << "\n";
    if (!first.stageADetail.empty()) {
        out << "  detail: " << first.stageADetail << "\n";
    }
    return out.str();
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (arg == "--orders") {
            options.orders = std::atoi(next().c_str());
        } else if (arg == "--mode") {
            options.mode = next();
        } else if (arg == "--base-url") {
            options.baseUrl = next();
        } else if (arg == "--model") {
            options.model = next();
        } else if (arg == "--doctrine") {
            options.doctrinePath = next();
        } else if (arg == "--out") {
            options.outPath = next();
        } else if (arg == "--no-format") {
            options.grammarEnabled = false;
        } else {
            std::cerr << "unknown argument: " << arg << "\n";
            return 2;
        }
    }

    CommanderConfig config;
    config.localBaseUrl = options.baseUrl;
    config.localModel = options.model;

    const std::string doctrine = readFile(options.doctrinePath);
    if (doctrine.empty()) {
        std::cerr << "[WARN] no doctrine at " << options.doctrinePath
                  << " - the prefix will say '(none provided)' and order quality is not being "
                     "measured as shipped\n";
    }

    PromptRenderer renderer;
    renderer.build(config, doctrine);

    std::ostringstream report;
    report.imbue(std::locale::classic());
    report << "N8RO AI Entity Commander - Phase 1b live validation\n";
    report << "  base url    " << options.baseUrl << "\n";
    report << "  model       " << options.model << "  (Ollama tag)\n";
    report << "  format      " << (options.grammarEnabled ? "on" : "OFF - unconstrained baseline") << "\n";
    report << "  timeout     " << config.requestTimeoutS << " s\n";
    report << "  doctrine    " << options.doctrinePath << " (" << doctrine.size() << " bytes)\n";
    report << "  prefixBytes " << renderer.prefix().size() << "\n";
    report << "  schemaBytes " << orderJsonSchemaText().size() << "\n";
    std::cout << report.str() << std::flush;

    // -- prefix: dump the deployed stable prefix and stop ----------------------------------------
    //
    // For OQ-8. The question is how many tokens the prefix is on ANTHROPIC's tokenizer, and there is
    // no offline Claude tokenizer - the only authority is POST /v1/messages/count_tokens. So this
    // mode writes the exact bytes the shipping prompt would carry, and count-prefix-tokens.ps1 sends
    // them.
    //
    // It runs BEFORE every inference mode and returns immediately: no server, no network, no model.
    // The dumped file is the prefix ALONE - it contains the system prompt, the vocabulary, the order
    // schema, and the doctrine, and by construction no volatile suffix and therefore no scenario
    // state whatsoever. That property is what the owner's authorization is scoped to, so it is worth
    // stating where the file is produced rather than only where it is sent.
    if (options.mode == "prefix") {
        const std::string outPath = "prefix.txt";
        std::ofstream out(outPath, std::ios::binary);
        if (!out) {
            std::cerr << "[FAIL] could not open " << outPath << " for writing\n";
            return 1;
        }
        out.write(renderer.prefix().data(),
                  static_cast<std::streamsize>(renderer.prefix().size()));
        out.close();
        std::cout << "[OK] wrote " << renderer.prefix().size() << " bytes to " << outPath
                  << " (stable prefix only - no snapshot, no scenario state)\n";
        return 0;
    }

    if (options.mode == "cold" || options.mode == "all") {
        const std::string section = runColdCheck(options, config, renderer);
        std::cout << section << std::flush;
        report << section;
    }

    if (options.mode == "geo" || options.mode == "all") {
        std::cout << "\n-- geo probe --\n" << std::flush;
        const std::string section = runGeoProbe(options, config, renderer, 2);
        std::cout << section << std::flush;
        report << section;
    }

    if (options.mode == "soak" || options.mode == "all") {
        std::cout << "\n-- soak: " << options.orders << " orders --\n" << std::flush;
        const Tally tally = runSoak(options, config, renderer, options.orders, false);
        const std::string section = formatTally("soak (prefix-stable)", tally);
        std::cout << section << std::flush;
        report << section;
    }

    if (options.mode == "h2" || options.mode == "all") {
        // H2 measures the deployed system, and Ollama's own prompt cache is part of that system.
        // The write-up says so rather than claiming to have isolated the model's KV reuse.
        const int half = std::max(20, options.orders / 2);
        std::cout << "\n-- H2: " << half << " stable vs " << half << " perturbed --\n" << std::flush;
        const Tally stable = runSoak(options, config, renderer, half, false);
        const Tally perturbed = runSoak(options, config, renderer, half, true);
        std::ostringstream section;
        section.imbue(std::locale::classic());
        section << formatTally("H2 - prefix stable", stable);
        section << formatTally("H2 - prefix perturbed", perturbed);
        const std::int64_t stableP95 = stable.percentile(0.95);
        const std::int64_t perturbedP95 = perturbed.percentile(0.95);
        section << "\n  H2 RESULT: stable p95 " << stableP95 << " ms vs perturbed p95 "
                << perturbedP95 << " ms";
        if (stableP95 > 0) {
            const double delta = 100.0 * (perturbedP95 - stableP95) / stableP95;
            section << "  (perturbed is " << std::fixed << std::setprecision(1) << delta
                    << " % slower)";
        }
        section << "\n  H2 predicted >= 30 % improvement from a byte-stable prefix. Reported as "
                   "measured, whatever it says.\n";
        std::cout << section.str() << std::flush;
        report << section.str();
    }

    if (!options.outPath.empty()) {
        std::ofstream out(options.outPath, std::ios::binary);
        out << report.str();
        std::cout << "\nreport written to " << options.outPath << "\n";
    }
    return 0;
}
