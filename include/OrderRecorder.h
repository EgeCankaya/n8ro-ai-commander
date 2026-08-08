#pragma once

#include "FallbackLadder.h"
#include "Order.h"
#include "RejectReason.h"
#include "Snapshot.h"

#include <cstdint>
#include <fstream>
#include <string>

namespace arkheon::aicommander {

// The lifecycle events the order log carries (AIC-DET-1). Replay reads `order.accepted`; the rest
// exist so a run is investigable rather than merely reproducible.
enum class OrderEvent {
    CommanderEnabled,
    CommanderDisabled,
    OrderRequested,
    OrderAccepted,
    OrderRejected,
    OrderTimeout,
    FallbackStanding,
    FallbackReleased,
    ReplayDivergence,
};

[[nodiscard]] const char* toString(OrderEvent event);

// Appends one JSON object per line to a rotated, size-capped log.
//
// Thread affinity: written on the SIMULATION thread only. The worker never touches the file — it
// has no file handle and no path, which keeps the "worker holds nothing but a snapshot and a
// client" rule intact and avoids interleaved writes from two threads.
//
// An accepted order's `t` is the simulation time at which it was PUBLISHED, not requested. Replay
// depends on that: it republishes at recorded publication times, and requesting times would drift
// the whole sequence by one inference latency.
class OrderRecorder {
public:
    OrderRecorder() = default;
    ~OrderRecorder();

    OrderRecorder(const OrderRecorder&) = delete;
    OrderRecorder& operator=(const OrderRecorder&) = delete;

    // Opens (or reopens) the log under `directory`. Returns false when the directory cannot be
    // created or the file cannot be opened — recording is best-effort and must never stall a frame,
    // so a failure disables recording rather than propagating.
    bool open(const std::string& directory, std::size_t maxBytesPerFile, int maxFiles);
    void close();

    [[nodiscard]] bool isOpen() const { return stream_.is_open(); }
    [[nodiscard]] const std::string& path() const { return activePath_; }

    void recordRequested(
        double simTimeS, std::int64_t frame, const OrderSnapshot& snapshot,
        const std::string& backend, const std::string& model,
        const std::string& snapshotHash, const std::string& promptHash);

    // The four cost figures AIC-DET-1 puts on a record, passed as one value rather than as four
    // positional ints (v1.8.18). They are all `int`, all plausible in any order, and two of them
    // were added at once — a transposed pair would produce an order log that is wrong in a way no
    // test asserts and no reader can spot, which is the same silent-wrongness this whole change is
    // about. Named fields make the call sites read as what they are.
    struct TokenUsage {
        int tokensIn = 0;
        int tokensOut = 0;
        int cacheReadTokens = 0;
        int cacheCreationTokens = 0;
    };

    // `orbitRadiusRepaired` marks an order whose `hold` orbit radius Stage A substituted rather
    // than rejecting (AIC-DET-1, v1.8.30, C14). Written only when true, so it is greppable across
    // an archive without adding a byte to the records that were not repaired - and written at all
    // because a field the plugin rewrote without saying so is the silent shortening §Corrections
    // item 26 already records as a defect, one field over.
    void recordAccepted(
        double publishedSimTimeS, std::int64_t frame, const Order& order, std::int64_t serial,
        std::int64_t latencyMs, const TokenUsage& usage, bool orbitRadiusRepaired = false);

    // `usage` is new here (v1.8.18) and it is the half of AIC-DET-1's widening that was not asked
    // for by C4 or C9. A rejected order is still billed: PRD §Corrections item 27(d) records four
    // truncated Sonnet orders charged for 512 output tokens each that produced nothing, against a
    // rejected record that carried no token field at all. An order log that prices only the orders
    // which survived validation understates the bill by exactly the orders worth asking about.
    void recordRejected(
        double simTimeS, std::int64_t frame, const std::string& entityId, std::int64_t serial,
        RejectReason reason, const std::string& detail, const std::string& rawBody,
        const TokenUsage& usage);

    void recordFallback(
        double simTimeS, std::int64_t frame, const std::string& entityId,
        FallbackLevel from, FallbackLevel to, double secondsSinceLastAccepted);

    void recordReplayDivergence(
        double simTimeS, std::int64_t frame, const std::string& entityId, std::int64_t serial,
        RejectReason failingCheck, const std::string& detail);

    void recordCommanderState(double simTimeS, bool enabled, const std::string& detail);

private:
    void writeLine(const std::string& line);
    void rotateIfNeeded();

    std::ofstream stream_;
    std::string directory_;
    std::string activePath_;
    std::size_t maxBytesPerFile_ = 16 * 1024 * 1024;
    int maxFiles_ = 4;
    std::size_t bytesWritten_ = 0;
};

// How much of a rejected body is retained. Enough to diagnose, bounded so a hostile or broken
// backend cannot fill the disk through the rejection path.
inline constexpr std::size_t kMaxRecordedBodyBytes = 4096;

} // namespace arkheon::aicommander
