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
//   schema - does the hosted structured-output path accept what the adapter sends? One request.
//            Tests PRD §Corrections item 19, which is recorded as a PREDICTION, not a measurement
//
//   ttft-dump - writes the EXACT request bodies the Claude adapter would POST, captured at the
//            transport boundary so a streaming probe can measure time-to-first-token without
//            reconstructing a request nobody sends. Sends nothing; needs no key (PRD v1.8.11)
//
// Usage: ai-commander-live-tests.exe [--orders N] [--mode prefix|schema|soak|cold|h2|geo|ttft-dump|all]
//                                    [--backend local|claude] [--model TAG] [--base-url URL]
//                                    [--claude-model ID] [--claude-base-url URL] [--key-env NAME]
//                                    [--max-tokens N] [--no-format] [--out PATH] [--csv PATH]
//                                    [--prose-schema] [--dump-dir DIR]
//
//   --prose-schema  reconstructs C3's ARM A - the prefix as it was before v1.8.14 removed the prose
//                   copy of the order schema - as a control. `--mode soak` only; every other mode
//                   rejects it rather than ignoring it. What ships is arm B, which is the default,
//                   so an unqualified run measures the product and this flag measures the control.
//                   (It replaces `--no-prose-schema`, which selected the arm that now ships and
//                   could no longer succeed at all.)
//   --dump-dir      where `--mode ttft-dump` writes ttft-request-NN.json. Defaults to the working
//                   directory. The arm-A/arm-B TTFT pairing is a pairing of two such directories.
//
// `--backend claude` REACHES THE NETWORK and transmits the volatile suffix - position, velocity,
// heading, team, reported tracks, loadout. It is authorization-gated (PRD §Phase 2). The default is
// `local` so that every previously recorded invocation still means what it meant when it was run.

#include "ClaudeLlmClient.h"
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
#include <memory>
#include <optional>
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

// Published per-MTok rates, per PRD §Cost model. Cache reads bill at 0.1x input, writes at 1.25x.
// Sonnet is at its LIST rate here rather than the introductory one, because a cost figure that
// silently depends on a promotion expiring 2026-08-31 would read as a permanent measurement.
struct Rates {
    double inPerMTok = 1.0;
    double outPerMTok = 5.0;
};

Rates ratesFor(const std::string& model) {
    if (model.find("opus") != std::string::npos)   { return {5.0, 25.0}; }
    if (model.find("sonnet") != std::string::npos) { return {3.0, 15.0}; }
    return {1.0, 5.0};   // haiku, and the default
}

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

    // -- token accounting, for the hosted backend ------------------------------------------------
    //
    // Accumulated over EVERY request including rejected ones, because a rejected order is billed
    // exactly like an accepted one. A cost-per-order computed over accepted orders alone would
    // understate the bill by the rejection rate, which is the direction that flatters the model.
    long long tokensIn = 0;
    long long tokensOut = 0;
    long long cacheReadTokens = 0;
    int responsesWithTokens = 0;
    int cacheHits = 0;

    // Per-order rows, kept alongside the aggregates rather than instead of them.
    struct Row {
        std::int64_t latencyMs;
        int tokensIn;
        int tokensOut;
        int cacheReadTokens;
        bool accepted;
        std::string situation;
    };
    std::vector<Row> rows;

    // Least-squares fit of latencyMs against tokensOut. The INTERCEPT is the fixed cost of a
    // request - everything that does not depend on how much the model wrote, which is where prefix
    // processing lives - and the SLOPE is the marginal cost per output token. If the intercept is
    // small, shrinking the prefix cannot buy much latency however much it buys in cost, and C2 and
    // C3 are separate problems. That is the question this exists to answer, and it is not
    // answerable from a p95 alone.
    struct Fit { double intercept = 0.0; double slope = 0.0; double r2 = 0.0; int n = 0; };
    [[nodiscard]] Fit fitLatencyOnOutput() const {
        Fit fit;
        std::vector<const Row*> usable;
        for (const Row& row : rows) {
            if (row.tokensOut > 0) { usable.push_back(&row); }
        }
        fit.n = static_cast<int>(usable.size());
        if (fit.n < 3) { return fit; }

        double sumX = 0.0, sumY = 0.0;
        for (const Row* row : usable) {
            sumX += row->tokensOut;
            sumY += static_cast<double>(row->latencyMs);
        }
        const double meanX = sumX / fit.n;
        const double meanY = sumY / fit.n;

        double sxx = 0.0, sxy = 0.0, syy = 0.0;
        for (const Row* row : usable) {
            const double dx = row->tokensOut - meanX;
            const double dy = static_cast<double>(row->latencyMs) - meanY;
            sxx += dx * dx;
            sxy += dx * dy;
            syy += dy * dy;
        }
        if (sxx <= 0.0) { return fit; }
        fit.slope = sxy / sxx;
        fit.intercept = meanY - fit.slope * meanX;
        fit.r2 = (syy > 0.0) ? (sxy * sxy) / (sxx * syy) : 0.0;
        return fit;
    }

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

void record(Tally& tally, const CandidateOrder& candidate, const std::string& situation,
            int cacheReadTokens = 0) {
    ++tally.requested;
    tally.latencies.push_back(candidate.latencyMs);
    if (candidate.tokensIn > 0 || candidate.tokensOut > 0) {
        tally.tokensIn += candidate.tokensIn;
        tally.tokensOut += candidate.tokensOut;
        ++tally.responsesWithTokens;
    }
    tally.cacheReadTokens += cacheReadTokens;
    if (cacheReadTokens > 0) {
        ++tally.cacheHits;
    }
    tally.rows.push_back({candidate.latencyMs, candidate.tokensIn, candidate.tokensOut,
                          cacheReadTokens, candidate.stageAAccepted, situation});
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

// The cost half of a tally, printed only when the backend actually reported token usage. Separated
// from formatTally so the Phase 1b local sections keep the exact shape they had when their numbers
// were recorded - a local run reports no usage, so nothing here fires and nothing there moves.
std::string formatCost(const Tally& tally, const std::string& model, double perOrderTarget) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    if (tally.responsesWithTokens == 0) {
        return out.str();
    }

    const Rates rates = ratesFor(model);
    const double n = static_cast<double>(tally.responsesWithTokens);

    // `input_tokens` EXCLUDES cached tokens - it is the uncached remainder, not the total. The API
    // reports the three populations in three separate fields:
    //
    //     input_tokens                 uncached input, billed at 1.0x   (here: the volatile suffix)
    //     cache_read_input_tokens      cache hits,     billed at 0.1x   (here: the stable prefix)
    //     cache_creation_input_tokens  cache writes,   billed at 1.25x
    //
    // An earlier version of this function subtracted cache reads from input_tokens on the assumption
    // that reads were a SUBSET of it. They are not, and in steady state that subtraction goes
    // negative (199 - 7608). Measured directly from a live response, which is the only thing that
    // settles it - the field names alone do not say whether they partition or nest.
    //
    // Cache CREATION is not summed here because neither ILlmClient nor this harness captures it.
    // Over a run of any length it is one write amortized across every order - 7,608 tokens once, or
    // about $0.0095 on Haiku, which is $0.00004/order over 240 - so its absence understates the
    // bill by well under a tenth of a percent. Stated rather than silently omitted.
    const double cost = (static_cast<double>(tally.tokensIn) * rates.inPerMTok / 1e6)
                      + (static_cast<double>(tally.cacheReadTokens) * rates.inPerMTok * 0.10 / 1e6)
                      + (static_cast<double>(tally.tokensOut) * rates.outPerMTok / 1e6);
    const double perOrder = cost / n;

    out << "  -- token accounting (measured, not modelled) --\n";
    out << "  responses w/ usage " << tally.responsesWithTokens << "\n";
    out << "  mean tokens in/out " << std::fixed << std::setprecision(1)
        << (static_cast<double>(tally.tokensIn) / n) << " / "
        << (static_cast<double>(tally.tokensOut) / n) << "\n";
    out << "  cache hits         " << tally.cacheHits << "/" << tally.responsesWithTokens
        << "   mean cache-read tokens " << std::setprecision(1)
        << (static_cast<double>(tally.cacheReadTokens) / n) << "\n";
    if (tally.cacheHits == 0) {
        out << "    *** ZERO cache reads. The prefix measured 4,489 tokens (OQ-8) and clears the\n"
               "        4,096 minimum, so zero reads means something is INVALIDATING the cache -\n"
               "        a different finding than OQ-8 asks about, and a 4.8x cost one.\n";
    }
    out << "  cost per order     $" << std::setprecision(5) << perOrder << "\n";
    out << "  four-ship-hour     $" << std::setprecision(2) << (perOrder * 720.0)
        << "   [PRD Success metrics target: <= $1.10]\n";
    if (perOrderTarget > 0.0) {
        const double deltaPct = 100.0 * (perOrder - perOrderTarget) / perOrderTarget;
        out << "  vs PRD Cost model  $" << std::setprecision(5) << perOrderTarget
            << " modelled, " << std::showpos << std::setprecision(1) << deltaPct << " %"
            << std::noshowpos << "   [gate: within +/- 20 %]  "
            << (std::abs(deltaPct) <= 20.0 ? "WITHIN" : "OUTSIDE") << "\n";
    }
    return out.str();
}

// The C2 latency decomposition, printed from per-order rows. Reports the fit and then says what it
// does NOT establish, because a regression over one arm cannot distinguish "the prefix is cheap"
// from "the prefix is expensive but constant" - only the second arm, with a different prefix, can.
std::string formatLatencyDecomposition(const Tally& tally) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    const Tally::Fit fit = tally.fitLatencyOnOutput();
    if (fit.n < 3) {
        return out.str();
    }

    out << "  -- latency decomposition (Carried out of Phase 2, C2) --\n";
    out << "  latencyMs ~ intercept + slope x tokensOut, over " << fit.n << " orders\n";
    out << "    intercept  " << std::fixed << std::setprecision(0) << fit.intercept
        << " ms      <- fixed per-request cost; prefix processing lives here\n";
    out << "    slope      " << std::setprecision(1) << fit.slope
        << " ms/token   <- marginal cost of writing one more token\n";
    out << "    r2         " << std::setprecision(3) << fit.r2 << "\n";

    // Attribute the MEAN latency between the two terms, which is the form the C2 decision needs.
    double meanOut = 0.0;
    int counted = 0;
    for (const Tally::Row& row : tally.rows) {
        if (row.tokensOut > 0) { meanOut += row.tokensOut; ++counted; }
    }
    if (counted > 0) {
        meanOut /= counted;
        const double outPart = fit.slope * meanOut;
        const double total = fit.intercept + outPart;
        if (total > 0.0) {
            out << "  at the mean output of " << std::setprecision(1) << meanOut << " tokens:\n";
            out << "    fixed      " << std::setprecision(0) << fit.intercept << " ms  ("
                << std::setprecision(0) << (100.0 * fit.intercept / total) << " %)\n";
            out << "    generation " << std::setprecision(0) << outPart << " ms  ("
                << std::setprecision(0) << (100.0 * outPart / total) << " %)\n";
        }
    }
    out << "  NOTE: one arm cannot separate a cheap prefix from an expensive CONSTANT one - both\n"
           "        land in the intercept. Comparing intercepts ACROSS two arms with different\n"
           "        prefix sizes is what identifies the prefix term. Run --mode soak twice with\n"
           "        different --doctrine and compare.\n";
    return out.str();
}

void writeCsv(const std::string& path, const Tally& tally) {
    if (path.empty()) {
        return;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "[WARN] could not open " << path << " for the per-order CSV\n";
        return;
    }
    out << "latencyMs,tokensIn,tokensOut,cacheReadTokens,accepted,situation\n";
    for (const Tally::Row& row : tally.rows) {
        out << row.latencyMs << "," << row.tokensIn << "," << row.tokensOut << ","
            << row.cacheReadTokens << "," << (row.accepted ? 1 : 0) << ",\"" << row.situation
            << "\"\n";
    }
    std::cout << "[OK] wrote " << tally.rows.size() << " per-order rows to " << path << "\n";
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

    // Which adapter the measurements drive. Defaults to `local` so every Phase 1b invocation in the
    // PRD and in the loop artifacts keeps meaning exactly what it meant when it was recorded -
    // changing the default would silently re-point runs that were cited as local measurements.
    //
    // `claude` reaches api.anthropic.com and is therefore AUTHORIZATION-GATED (PRD v1.8.3). It
    // transmits the volatile suffix: position, velocity, heading, team, reported tracks, loadout.
    std::string backend = "local";
    std::string claudeModel = "claude-haiku-4-5";
    std::string claudeBaseUrl = "https://api.anthropic.com";
    std::string apiKeyEnvVar = "ANTHROPIC_API_KEY";

    // The output ceiling, in tokens. Defaults to the shipped `claude.maxTokens` so an unqualified
    // run keeps measuring the shipped configuration. It is a harness flag because C7 needs a run
    // whose ceiling does NOT censor the thing being measured: at 512 the Sonnet run's output-length
    // distribution is right-censored exactly where the question lives, so the tail is observable
    // only by raising this and re-running (PRD §Corrections item 25). Bounded like the config field.
    int claudeMaxTokens = 512;

    // Per-order rows, for questions the aggregates cannot answer. The soak reports a p95 and a mean
    // output length; it cannot say whether the SAME orders that were slow were the ones that emitted
    // the most tokens, and that correlation is the whole of the C2 latency decomposition.
    std::string csvPath;

    // C3's arm A, and it is now the arm that has to be RECONSTRUCTED.
    //
    // The order schema used to be transmitted TWICE per request (PRD §Corrections item 22): once as
    // prose, rendered into the prefix by PromptRenderer::build, and once structurally as
    // `output_config.format.schema`. C3 measured the pair at 120 orders per arm, both accepted
    // 120/120, the pre-registered decision rule fired, and the prose copy was REMOVED from the
    // shipped prefix (v1.8.14, §Corrections item 31).
    //
    // WHICH INVERTS THIS FLAG. It was `--no-prose-schema` and it EXCISED the block. After the
    // removal there is nothing to excise: the search string is no longer emitted, so the flag hit
    // std::exit(1) unconditionally and C3's arm A became unreconstructible from the binary - the
    // shipped artifact could no longer be turned back into the artifact the experiment compared it
    // against. Found in the post-merge review of PR #13.
    //
    // It is INVERTED RATHER THAN DELETED, and the reason is in §Corrections item 31(c) and in
    // PromptRenderer.cpp's own comment: the structured-output projection is now the ONLY description
    // of the order shape that reaches the model on the hosted path, and a backend that does not
    // constrain decoding would not inherit it - the prose copy would have to come back for that one.
    // Deleting the flag would leave that question un-re-measurable except by reverting a shipped
    // change. `--prose-schema` is what keeps the control arm reachable.
    //
    // True re-inserts exactly the prose block, and nothing else, into the built prefix. It does not
    // touch `--no-format`: structured outputs stay ON in both arms, because the premise the pair
    // tests is that the structural copy is doing the constraining. Turning both off would measure a
    // different question.
    bool proseSchema = false;

    // Where --mode ttft-dump writes its request bodies. Defaults to the working directory, which is
    // what it did unconditionally and with no way to say otherwise - unlike every other mode, which
    // takes --out. Two dumps taken from different directories are silently unrelated, and the
    // arm-A/arm-B pairing measure-ttft.ps1 depends on is a pairing of two DIRECTORIES, so where you
    // were standing when you took them was load-bearing and undeclared. Found in the post-merge
    // review of PR #13. --out is a report FILE path and cannot serve; this is a directory.
    std::string dumpDir = ".";
};

// Produces the prefix PromptRenderer::build produced BEFORE C3 - the arm-A control - by putting the
// prose schema block back where it used to be.
//
// This is the inverse of what used to live here. `exciseProseSchema` cut the block out of a prefix
// that still carried it, and stopped working the moment the shipped renderer stopped emitting it.
// Reconstruction is the only direction that survives its own experiment landing.
//
// WHY BY TEXT SURGERY AND NOT BY A CONFIG FLAG, unchanged from the original reasoning: a flag would
// be a product change, and this is a measurement, not a feature. And why on the RENDERER'S output
// rather than composed here: render() is `prefix + "\nSITUATION:\n" + suffix + "\n\nYour order:\n"`,
// so rebuilding that shape in a second place would silently diverge the day it changes. The
// renderer stays the single authority on what a prompt looks like; this cuts one block into it.
//
// THE INSERTION POINT, derived rather than guessed. build() emitted, and still emits:
//
//   kSystemPrompt '\n' kPostureVocabulary '\n' [ header schemaText '\n' ] '\n' "DOCTRINE:\n" ...
//                                              ^-- the block, gone since v1.8.14
//
// The '\n' after the block is the bare newline at PromptRenderer.cpp:95, which was KEPT precisely so
// the post-removal prefix stayed byte-identical to the arm that measured 120/120 (§Corrections item
// 31(f)). It is still there, so "\nDOCTRINE:\n" locates the seam exactly and the reconstruction is
// byte-exact rather than approximate.
//
// ASSERT, DO NOT ASSUME - mirroring what the excision did, in the opposite direction. The excision
// refused to run when the block was missing; this refuses to run when the block is already PRESENT,
// because inserting a second copy would measure a prompt nobody has ever shipped while looking like
// a clean control arm.
bool insertProseSchema(const std::string& prefix, std::string& out) {
    const std::string header = "ORDER SCHEMA (your reply must validate against this):\n";
    if (prefix.find(header) != std::string::npos) {
        std::cerr << "[FAIL] the prefix ALREADY carries the prose schema - PromptRenderer changed, "
                     "and re-inserting would send it twice\n";
        return false;
    }
    const std::string seam = "\nDOCTRINE:\n";
    const std::size_t at = prefix.find(seam);
    if (at == std::string::npos) {
        std::cerr << "[FAIL] the DOCTRINE seam was not found in the prefix - PromptRenderer changed, "
                     "and the arm-A insertion point can no longer be located\n";
        return false;
    }
    if (prefix.find(seam, at + 1) != std::string::npos) {
        std::cerr << "[FAIL] the DOCTRINE seam appears more than once - the insertion point is "
                     "ambiguous and a guess here would silently produce a prompt nobody shipped\n";
        return false;
    }
    const std::string block = header + orderJsonSchemaText() + "\n";
    out = prefix.substr(0, at) + block + prefix.substr(at);

    // The size the reconstruction has to land on, and it is a MEASURED number rather than a derived
    // one: arm A's prefix was 17,756 bytes and arm B's 8,750, a difference of 9,006 (§Corrections
    // item 31(a)). If the schema document ever changes shape, this stops matching and the run stops
    // - which is correct, because at that point the reconstruction is no longer arm A and pairing
    // its result with the archived arm-B rows would be comparing two different experiments.
    constexpr std::size_t kArmABlockBytes = 9006;
    if (block.size() != kArmABlockBytes) {
        std::cerr << "[FAIL] the reconstructed prose block is " << block.size() << " bytes, not the "
                  << kArmABlockBytes
                  << " C3's arm A measured. The order schema changed, so this is no longer arm A "
                     "and its rows must not be pooled with the archived ones "
                     "(tests/live/data/c3-quality-armA-prose-schema.csv).\n";
        return false;
    }
    return true;
}

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

ClaudeClientConfig claudeConfigFrom(const Options& options, const CommanderConfig& config) {
    ClaudeClientConfig claude;
    claude.baseUrl = options.claudeBaseUrl;
    claude.model = options.claudeModel;
    claude.apiKeyEnvVar = options.apiKeyEnvVar;
    claude.maxTokens = options.claudeMaxTokens;
    claude.timeoutS = config.requestTimeoutS;
    // Left empty deliberately: `effort` is a prohibition on Haiku 4.5, not an omission. The adapter
    // suppresses it anyway; not setting it here means the harness never depends on that suppression.
    return claude;
}

// One adapter, chosen by `--backend`, returned behind the interface every measurement already
// speaks. The modes below all drive `CommanderRuntime::runWorkerCall`, which takes an `ILlmClient&`
// - so selecting a backend is a construction detail and none of the measurement code changes shape.
//
// The concrete Claude pointer comes back alongside it, because `lastCacheReadTokens()` is on the
// concrete type and there is no way to ask the interface. That is the right place for it: a cache
// read count is a property of one backend, and widening ILlmClient to carry it would put a hosted
// concern into the interface the stub, replay, and local adapters implement.
struct ClientHandle {
    std::unique_ptr<ILlmClient> client;
    ClaudeLlmClient* claude = nullptr;   // non-owning; null unless backend == "claude"
};

ClientHandle makeClient(const Options& options, const CommanderConfig& config) {
    ClientHandle handle;
    if (options.backend == "claude") {
        auto claude = std::make_unique<ClaudeLlmClient>(claudeConfigFrom(options, config));
        handle.claude = claude.get();
        handle.client = std::move(claude);
        return handle;
    }
    handle.client = std::make_unique<LocalLlmClient>(clientConfigFrom(options, config));
    return handle;
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
    ClientHandle handle = makeClient(options, config);
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

    // -- the memorised-coordinate comparison (PRD §Carried out of Phase 1b, C1) -------------------
    //
    // Phase 1b characterised the local 7B substituting a MEMORISED real-world coordinate for a
    // waypoint whose correct value was own-ship position: −31.952247, 115.857309 - Perth, Western
    // Australia - drawn three times, across two entities and three runs, agreeing to four decimal
    // places on a number that appears nowhere in the prompt.
    //
    // The comparison is whether a frontier model does the same. It is reported whatever it shows;
    // a negative result is as publishable as a positive one and neither changes Stage B's standing.
    // The detector is proximity to Perth rather than "far from own-ship", because distance alone
    // cannot distinguish a memorised place-name from an ordinary bad number - and it is the
    // specificity of the coordinate that made the Phase 1b finding what it is.
    constexpr double kPerthLatDeg = -31.9523;
    constexpr double kPerthLonDeg = 115.8613;
    constexpr double kPerthRadiusM = 50000.0;
    int nearPerth = 0;

    for (const Situation& situation : situations) {
        for (int r = 0; r < repeats; ++r) {
            const std::string prompt = renderer.render(situation.snapshot);
            const CandidateOrder candidate = CommanderRuntime::runWorkerCall(
                situation.snapshot, prompt, *handle.client, renderer.prefix().size());

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
            const double perthM = slantRangeM(kPerthLatDeg, kPerthLonDeg, order.altitudeHaeM,
                                              order.latitudeDeg, order.longitudeDeg,
                                              order.altitudeHaeM);
            const bool memorised = perthM <= kPerthRadiusM;
            if (memorised) {
                ++nearPerth;
            }

            out << "  own(" << std::fixed << std::setprecision(3)
                << situation.snapshot.latitudeDeg << ", " << situation.snapshot.longitudeDeg << ")"
                << "  -> wp(" << order.latitudeDeg << ", " << order.longitudeDeg << ", "
                << std::setprecision(0) << order.altitudeHaeM << " m)"
                << "  range " << static_cast<long long>(rangeM) << " m  "
                << (inside ? "inside" : "*** OUTSIDE THE FENCE ***");
            if (memorised) {
                out << "  <<< WITHIN " << static_cast<long long>(kPerthRadiusM / 1000.0)
                    << " km OF PERTH - the Phase 1b memorised coordinate";
            }
            out << "\n";
        }
    }

    out << "\n  waypoint-carrying orders " << withWaypoint
        << ", outside the fence " << outsideFence;
    if (withWaypoint > 0) {
        out << "  (" << std::fixed << std::setprecision(1)
            << (100.0 * outsideFence / withWaypoint) << " %)";
    }
    out << "\n  furthest waypoint " << static_cast<long long>(worstM) << " m\n";

    out << "\n  -- memorised-coordinate comparison (PRD Carried out of Phase 1b, C1) --\n";
    out << "  Phase 1b baseline   local 7B, 3 observations / 2 entities / 3 runs, at\n"
           "                      -31.952247, 115.857309 (Perth, Western Australia)\n";
    out << "  this run            " << nearPerth << " of " << withWaypoint
        << " waypoint-carrying orders within 50 km of Perth\n";
    out << "  RESULT              "
        << (withWaypoint == 0
                ? "INCONCLUSIVE - no waypoint-carrying orders to examine"
                : (nearPerth > 0
                       ? "REPRODUCES - the substitution appears on this backend too"
                       : "DOES NOT REPRODUCE on this sample"))
        << "\n";
    if (withWaypoint > 0 && nearPerth == 0) {
        out << "                      A negative result over " << withWaypoint
            << " orders bounds the rate, it does not\n"
               "                      establish zero, and it says nothing about other memorised\n"
               "                      coordinates - the detector looks for Perth specifically.\n";
    }
    return out.str();
}

Tally runSoak(const Options& options, const CommanderConfig& config, const PromptRenderer& renderer,
              int orders, bool perturb) {
    ClientHandle handle = makeClient(options, config);
    const std::vector<Situation> situations = buildSituations();

    // C3's arm A, reconstructed. Resolved once before the loop: the prefix has to be BYTE-IDENTICAL
    // across the run or the cache never reads and the arm measures a cold path instead of the thing
    // it was asked about.
    //
    // The insertion is applied to the FULL PROMPT rather than composed from prefix + suffix, and the
    // difference matters. render() is `prefix + "\nSITUATION:\n" + suffix + "\n\nYour order:\n"` -
    // rebuilding that here would duplicate PromptRenderer's format in a second place and silently
    // diverge the day it changes. Taking render()'s output and putting one block back into it leaves
    // the renderer the single authority on what a prompt looks like, which is the property this
    // measurement most needs: arm B must be the shipped prompt EXACTLY, or the pair compares
    // nothing. Note which arm that sentence now names - arm B is what ships.
    std::size_t insertedBytes = 0;
    if (options.proseSchema) {
        std::string probe;
        if (!insertProseSchema(renderer.prefix(), probe)) {
            std::exit(1);
        }
        insertedBytes = probe.size() - renderer.prefix().size();
        std::cout << "    [C3 arm A, RECONSTRUCTED] prose schema re-inserted: "
                  << renderer.prefix().size() << " -> " << probe.size() << " bytes (+"
                  << insertedBytes
                  << "). This is the CONTROL, not what ships. Structured outputs remain ON.\n";
    }

    // The cache breakpoint goes at the real prefix/suffix boundary (AIC-BE-3). Suppressed when
    // perturbing, because H2 deliberately shifts the prefix - declaring a boundary that the
    // perturbation has moved would place the breakpoint mid-text and measure neither hypothesis.
    const std::size_t prefixLength = perturb ? 0 : renderer.prefix().size() + insertedBytes;

    Tally tally;
    for (int i = 0; i < orders; ++i) {
        const Situation& situation = situations[static_cast<std::size_t>(i) % situations.size()];
        std::string prompt = renderer.render(situation.snapshot);
        if (options.proseSchema) {
            std::string withProse;
            if (!insertProseSchema(prompt, withProse)) {
                std::exit(1);
            }
            prompt = withProse;
        }
        if (perturb) {
            prompt = perturbPrefix(prompt, i);
        }
        // The production call, not a re-implementation of it: prompt -> adapter -> A1 -> A2 -> A3
        // -> ... -> A7, with the Stage-A verdict packed into the candidate exactly as the worker
        // receives it.
        const CandidateOrder candidate = CommanderRuntime::runWorkerCall(
            situation.snapshot, prompt, *handle.client, prefixLength);
        record(tally, candidate, situation.label,
               handle.claude != nullptr ? handle.claude->lastCacheReadTokens() : 0);

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
    ClientHandle handle = makeClient(options, config);
    const std::vector<Situation> situations = buildSituations();

    const CandidateOrder first = CommanderRuntime::runWorkerCall(
        situations[0].snapshot, renderer.render(situations[0].snapshot), *handle.client,
        renderer.prefix().size());
    const CandidateOrder second = CommanderRuntime::runWorkerCall(
        situations[1].snapshot, renderer.render(situations[1].snapshot), *handle.client,
        renderer.prefix().size());

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

// -- schema: does the hosted structured-output path accept what we send? --------------------------
//
// This is the test for PRD §Corrections item 19, which records - explicitly as A PREDICTION FROM
// DOCUMENTATION AND NOT A MEASUREMENT - that the hosted path rejects `minimum`/`maximum`/
// `minLength`/`maxLength`, and does not list `oneOf`. The four-branch encoding that took
// reject.shape from 10/12 to 0/12 is built out of exactly those keywords, which is why
// orderJsonSchemaForStructuredOutputs() exists.
//
// ONE request settles it, and the three outcomes mean different things:
//   - a 400 naming a schema keyword          -> the prediction is CONFIRMED and the projection is a fix
//   - acceptance                             -> the prediction is REFUTED and the projection was insurance
//   - a 400 naming something else            -> neither; report it as its own finding
//
// The whole response body is printed rather than summarised. A schema rejection names the offending
// keyword and the JSON path it sits on, and that detail is the entire value of the probe - the
// third time this project has been misled by a diagnostic channel it summarised too early.
std::string runSchemaProbe(const Options& options, const CommanderConfig& config,
                           const PromptRenderer& renderer) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "\n=== schema acceptance probe (PRD Corrections item 19) ===\n";
    out << "  backend         " << options.backend << "\n";
    out << "  model           "
        << (options.backend == "claude" ? options.claudeModel : options.model) << "\n";
    out << "  prediction      the hosted path REJECTS minimum/maximum/minLength/maxLength,\n"
           "                  and does not list oneOf. Recorded in v1.8 as a prediction only.\n\n";

    ClientHandle handle = makeClient(options, config);
    const std::vector<Situation> situations = buildSituations();
    const Situation& situation = situations[0];

    bool accepted = false;
    RejectReason reason = RejectReason::None;
    std::string detail;
    std::string rawBody;
    const CandidateOrder candidate = CommanderRuntime::runWorkerCall(
        situation.snapshot, renderer.render(situation.snapshot), *handle.client,
        accepted, reason, detail, rawBody, renderer.prefix().size());

    out << "  situation       " << situation.label << "\n";
    out << "  latency         " << candidate.latencyMs << " ms\n";
    out << "  stage-A verdict "
        << (accepted ? std::string("accepted") : std::string("rejected ") + toString(reason))
        << (detail.empty() ? std::string() : "  - " + detail) << "\n";
    out << "  tokens in/out   " << candidate.tokensIn << " / " << candidate.tokensOut << "\n";
    if (handle.claude != nullptr) {
        out << "  cache read      " << handle.claude->lastCacheReadTokens() << " tokens\n";
    }
    out << "\n  --- response body, verbatim ---\n" << rawBody << "\n  --- end body ---\n\n";

    // Named keywords, searched for in the body. Only meaningful alongside a 4xx; a 200 that happens
    // to contain the word "minimum" in a rationale is not a schema rejection.
    static const char* const kKeywords[] = {
        "minimum", "maximum", "multipleOf", "minLength", "maxLength", "oneOf", "anyOf",
        "additionalProperties", "schema"
    };
    std::vector<std::string> mentioned;
    for (const char* keyword : kKeywords) {
        if (rawBody.find(keyword) != std::string::npos) {
            mentioned.emplace_back(keyword);
        }
    }

    const bool transportish = (reason == RejectReason::Transport);
    out << "  VERDICT: ";
    if (accepted) {
        out << "THE PROJECTION IS ACCEPTED by the hosted structured-output path.\n"
               "\n"
               "           READ THIS BEFORE CITING IT AGAINST CORRECTIONS ITEM 19. This mode sends\n"
               "           what the ADAPTER sends, and the adapter sends the PROJECTION - the\n"
               "           document with minimum/maximum/minLength/maxLength already stripped. So\n"
               "           this result establishes that the projection WORKS. It says NOTHING about\n"
               "           whether the canonical document would have been rejected, because the\n"
               "           keywords the prediction is about were never in the request.\n"
               "\n"
               "           Corrections item 19 is tested by posting the CANONICAL document:\n"
               "             ai-commander-live-tests.exe --mode dump-schemas\n"
               "             tests\\live\\probe-canonical-schema.ps1\n";
    } else if (transportish && !mentioned.empty()) {
        out << "PREDICTION CONFIRMED - rejected, and the body names schema keywords:\n           ";
        for (const std::string& keyword : mentioned) {
            out << keyword << " ";
        }
        out << "\n           The projection is load-bearing. Read the body above for the path.\n";
    } else if (transportish) {
        out << "REJECTED, but the body names no schema keyword. This is NOT a confirmation\n"
               "           of Corrections item 19 - it is its own finding. Read the body above.\n";
    } else {
        out << "The request completed and Stage A rejected it as '" << toString(reason)
            << "', which\n           is a MODEL-OUTPUT finding, not a schema-acceptance one: the API "
               "accepted\n           the schema. Corrections item 19 is refuted on acceptance.\n";
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
        } else if (arg == "--prose-schema") {
            options.proseSchema = true;
        } else if (arg == "--dump-dir") {
            options.dumpDir = next();
        } else if (arg == "--backend") {
            options.backend = next();
        } else if (arg == "--claude-model") {
            options.claudeModel = next();
        } else if (arg == "--claude-base-url") {
            options.claudeBaseUrl = next();
        } else if (arg == "--key-env") {
            options.apiKeyEnvVar = next();
        } else if (arg == "--max-tokens") {
            options.claudeMaxTokens = std::atoi(next().c_str());
        } else if (arg == "--csv") {
            options.csvPath = next();
        } else if (arg == "--no-prose-schema") {
            // Named explicitly so the failure is a sentence rather than "unknown argument". The
            // flag inverted when C3 landed: the prose block is no longer in the shipped prefix, so
            // there is nothing to turn off, and a run that quietly accepted the old spelling would
            // measure arm B while its command line said arm A.
            std::cerr << "--no-prose-schema no longer exists. C3 REMOVED the prose schema from the "
                         "shipped prefix (PRD v1.8.14), so the default run is already the arm that "
                         "flag used to select. Use --prose-schema to reconstruct arm A as a "
                         "control.\n";
            return 2;
        } else {
            std::cerr << "unknown argument: " << arg << "\n";
            return 2;
        }
    }

    // --prose-schema only reaches runSoak, and it used to be SILENTLY IGNORED everywhere else: a
    // `--mode ttft-dump --prose-schema` invocation would dump arm-B bodies while its command line
    // said arm A, and nothing said so. A flag that is read in one mode and dropped in the others is
    // a flag that will eventually label a run wrongly. Found in the post-merge review of PR #13.
    // It is honoured by `prefix` as well, and that is not a convenience: `--mode prefix
    // --prose-schema` writes arm A's exact bytes with no network and no key, which is the only way
    // to re-measure arm A's token count through count-prefix-tokens.ps1, and the only way to check
    // that the reconstruction still lands on 17,756 bytes without spending anything.
    if (options.proseSchema && options.mode != "soak" && options.mode != "prefix") {
        std::cerr << "--prose-schema applies to --mode soak (it reconstructs C3's arm-A prompt for "
                     "the paired comparison) and --mode prefix (it writes arm A's bytes for "
                     "inspection). It is not read by mode '"
                  << options.mode << "', and being ignored silently is how a run gets mislabelled.\n";
        return 2;
    }

    if (options.backend != "local" && options.backend != "claude") {
        std::cerr << "unknown backend: " << options.backend << " (expected 'local' or 'claude')\n";
        return 2;
    }

    // The same bound the config surface enforces (AIC-API-2, PRD v1.8.7). The harness validates it
    // rather than trusting the flag, because a probe that exceeds Stage-A A0's 64 KiB body cap would
    // report `range` rejections and read exactly like the truncation it was built to remove.
    if (options.claudeMaxTokens < 1
        || options.claudeMaxTokens > arkheon::aicommander::kMaxClaudeMaxTokens) {
        std::cerr << "--max-tokens must be within [1, "
                  << arkheon::aicommander::kMaxClaudeMaxTokens << "]\n";
        return 2;
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
    report << "N8RO AI Entity Commander - live validation\n";
    report << "  backend     " << options.backend << "\n";
    if (options.backend == "claude") {
        // Stated at the top of every hosted run, because the authorization is scoped to exactly
        // this and a reader of the report should not have to infer what left the machine.
        report << "  base url    " << options.claudeBaseUrl << "\n";
        report << "  model       " << options.claudeModel << "  (Anthropic model id)\n";
        report << "  key from    $" << options.apiKeyEnvVar << "  (name only; value never logged)\n";
        report << "  max tokens  " << options.claudeMaxTokens
               << (options.claudeMaxTokens == 512 ? "  (shipped default)" : "  (RAISED - the shipped default is 512)")
               << "\n";
        report << "  EGRESS      LIVE. Transmits the volatile suffix: position, velocity, heading,\n";
        report << "              team, reported tracks, loadout. Authorized PRD v1.8.3, scoped to\n";
        report << "              the synthetic LiveMain fixtures - NOT the real scenario.\n";
    } else {
        report << "  base url    " << options.baseUrl << "\n";
        report << "  model       " << options.model << "  (Ollama tag)\n";
    }
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
    // -- dump-schemas: both documents to disk, for the probe that actually tests Corrections 19 ----
    //
    // The `schema` mode below sends what the ADAPTER sends, which is the PROJECTION. That tests
    // whether the projection works; it cannot test the prediction, because the prediction is about
    // the keywords the projection strips. Answering Corrections item 19 requires posting the
    // CANONICAL document and seeing whether the API rejects it - so both are written out here and
    // probe-canonical-schema.ps1 posts each in turn.
    //
    // No network, no model, no scenario state: two JSON documents that are compiled into the DLL.
    if (options.mode == "dump-schemas") {
        struct Dump { const char* path; std::string text; };
        const Dump dumps[] = {
            {"schema-canonical.json",  orderJsonSchema().toString(true)},
            {"schema-projection.json", orderJsonSchemaForStructuredOutputs().toString(true)},
        };
        for (const Dump& dump : dumps) {
            std::ofstream out(dump.path, std::ios::binary);
            if (!out) {
                std::cerr << "[FAIL] could not open " << dump.path << "\n";
                return 1;
            }
            out << dump.text;
            std::cout << "[OK] wrote " << dump.text.size() << " bytes to " << dump.path << "\n";
        }
        return 0;
    }

    // -- ttft-dump: write the EXACT request bodies, so a streaming probe can measure TTFT ---------
    //
    // For C2 (PRD §Corrections item 28(f) and the fifth grant, v1.8.11). Time-to-first-token is the
    // lower-variance estimator that C2 needs, and it is not observable through the product's HTTP
    // seam at all: `n8ro::core::IHttpClient::send()` is blocking and returns a whole body, with no
    // chunk callback and no headers-received hook. So the measurement lives in a PowerShell probe,
    // exactly as count-prefix-tokens.ps1 and probe-canonical-schema.ps1 already do, and this mode
    // supplies it with the one thing a hand-written probe cannot be trusted to reproduce: the body.
    //
    // THE ERROR THIS MODE EXISTS TO PREVENT is §Corrections item 22. OQ-8 measured the prefix with
    // count_tokens, correctly and against the right tokenizer, and got a number for the wrong
    // object - the adapter sends an entire second copy of the schema after the renderer stops. A
    // TTFT probe that composed its own request body would repeat that error precisely: it would
    // measure a request nobody sends.
    //
    // So the body is not rebuilt here. It is CAPTURED AT THE TRANSPORT BOUNDARY - a fake
    // IHttpClient installed through the adapter's existing test seam records `request.body` and
    // returns nullopt, so what lands on disk is the byte sequence that would have gone on the wire,
    // produced by ClaudeLlmClient's own construction path. Nothing is sent: the capture happens
    // before any socket is opened, which is why this mode is NOT authorization-gated.
    if (options.mode == "ttft-dump") {
        if (options.backend != "claude") {
            std::cerr << "[FAIL] --mode ttft-dump requires --backend claude\n";
            return 1;
        }

        // Records what it is handed and refuses to send it. Returning nullopt drives the adapter's
        // transport-failure path, which is fine: the result is discarded and the capture already
        // happened. It also means the mode cannot reach the network even by mistake.
        //
        // FIRST CAPTURE WINS, and that is load-bearing rather than defensive. A nullopt return is
        // exactly the condition on which the adapter runs AIC-BE-4's TLS-availability control probe
        // - a second send() through this same client, to the same host, deliberately carrying no
        // prompt, no key and no body. Last-write-wins would therefore hand back an empty string
        // every time, and a probe built on it would have measured nothing while looking like it
        // worked. The control request is a real part of the adapter's behaviour; it is simply not
        // the request being measured.
        struct CapturingHttpClient final : public n8ro::core::IHttpClient {
            std::string* body;
            std::string* url;
            std::optional<n8ro::core::HttpResponse> send(
                const n8ro::core::HttpRequest& request) override {
                if (body->empty()) {
                    *body = request.body;
                    *url = request.url;
                }
                return std::nullopt;
            }
        };

        const std::vector<Situation> dumpSituations = buildSituations();
        int written = 0;
        std::string capturedUrl;

        for (std::size_t i = 0; i < dumpSituations.size(); ++i) {
            const Situation& situation = dumpSituations[i];

            std::string body;
            ClaudeLlmClient client(claudeConfigFrom(options, config));
            client.setHttpClientFactory([&body, &capturedUrl] {
                auto fake = std::make_unique<CapturingHttpClient>();
                fake->body = &body;
                fake->url = &capturedUrl;
                return fake;
            });

            const std::string prompt = renderer.render(situation.snapshot);
            // Discarded on purpose. runWorkerCall is the production call path, and driving it here
            // rather than calling the adapter directly is what makes the captured body the body a
            // real order would produce - including LlmRequest::prefixLength, which decides where
            // the cache breakpoint goes and is therefore part of what is being measured.
            (void)CommanderRuntime::runWorkerCall(situation.snapshot, prompt, client,
                                                  renderer.prefix().size());

            if (body.empty()) {
                std::cerr << "[FAIL] captured an empty body for " << situation.label << "\n";
                return 1;
            }

            std::ostringstream name;
            name << options.dumpDir << "/ttft-request-" << std::setfill('0') << std::setw(2)
                 << (i + 1) << ".json";
            std::ofstream out(name.str(), std::ios::binary);
            if (!out) {
                std::cerr << "[FAIL] could not open " << name.str()
                          << " (does the --dump-dir exist? this mode does not create it)\n";
                return 1;
            }
            out << body;
            out.close();

            std::cout << "[OK] " << std::left << std::setw(28) << name.str() << std::right
                      << std::setw(7) << body.size() << " bytes   " << situation.label << "\n";
            ++written;
        }

        std::cout << "\n[OK] wrote " << written << " request bodies for " << options.claudeModel
                  << " at max_tokens=" << options.claudeMaxTokens << " into " << options.dumpDir
                  << "\n";
        std::cout << "     The directory is what measure-ttft.ps1 -RequestDir takes, and the\n";
        std::cout << "     arm-A/arm-B pairing is a pairing of two of them - name them so a reader\n";
        std::cout << "     of the CSVs can tell which dump produced which arm.\n";
        std::cout << "     target URL (not contacted): " << capturedUrl << "\n";
        std::cout << "     NOTHING WAS SENT. The capture is at the transport boundary, before any\n";
        std::cout << "     socket is opened, so this mode needs no authorization and no API key.\n";
        std::cout << "     These bodies CARRY THE VOLATILE SUFFIX - position, velocity, heading,\n";
        std::cout << "     team, tracks, loadout, from the synthetic fixtures. measure-ttft.ps1 is\n";
        std::cout << "     what transmits them, and it is gated.\n";
        return 0;
    }

    if (options.mode == "prefix") {
        // Arm B - what ships - unless --prose-schema asks for the arm-A control. Both are the
        // stable prefix alone: no snapshot, no scenario state, which is the property the v1.8.1
        // authorization is scoped to and the reason this mode needs no gate.
        std::string text = renderer.prefix();
        if (options.proseSchema) {
            std::string armA;
            if (!insertProseSchema(text, armA)) {
                return 1;
            }
            text = armA;
        }
        const std::string outPath = options.outPath.empty() ? std::string("prefix.txt")
                                                            : options.outPath;
        std::ofstream out(outPath, std::ios::binary);
        if (!out) {
            std::cerr << "[FAIL] could not open " << outPath << " for writing\n";
            return 1;
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.close();
        std::cout << "[OK] wrote " << text.size() << " bytes to " << outPath << " ("
                  << (options.proseSchema ? "C3 ARM A, RECONSTRUCTED - the control, NOT what ships"
                                          : "the shipped prefix, C3 arm B")
                  << "; stable prefix only - no snapshot, no scenario state)\n";
        return 0;
    }

    // Runs FIRST in `all`, and is the cheapest thing here: one request. If the hosted path rejects
    // the schema, every subsequent mode would spend money reproducing the same 400.
    if (options.mode == "schema" || options.mode == "all") {
        const std::string section = runSchemaProbe(options, config, renderer);
        std::cout << section << std::flush;
        report << section;
        if (options.mode == "schema") {
            if (!options.outPath.empty()) {
                std::ofstream out(options.outPath, std::ios::binary);
                out << report.str();
            }
            return 0;
        }
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
        // $0.00105/order is §Cost model's Haiku CACHED row as recomputed in v1.8.2 off the measured
        // 4,489-token prefix. The +/- 20 % gate is against that, not against the superseded $0.00180
        // that was computed from a prefix which was never deployed.
        const std::string section = formatTally("soak (prefix-stable)", tally)
                                  + formatCost(tally, options.backend == "claude"
                                                          ? options.claudeModel : options.model,
                                               0.00105)
                                  + formatLatencyDecomposition(tally);
        std::cout << section << std::flush;
        report << section;
        writeCsv(options.csvPath, tally);
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
