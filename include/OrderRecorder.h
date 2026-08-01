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

    void recordAccepted(
        double publishedSimTimeS, std::int64_t frame, const Order& order, std::int64_t serial,
        std::int64_t latencyMs, int tokensIn, int tokensOut);

    void recordRejected(
        double simTimeS, std::int64_t frame, const std::string& entityId, std::int64_t serial,
        RejectReason reason, const std::string& detail, const std::string& rawBody);

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
