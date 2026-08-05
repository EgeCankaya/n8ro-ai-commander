#pragma once

#include "Order.h"
#include "RejectReason.h"
#include "Snapshot.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace arkheon::aicommander {

// A candidate order coming back off the worker, before Stage B has seen it.
//
// It carries the Stage-A VERDICT as well as the order, because a Stage-A rejection still has to be
// counted and recorded — and neither the counters nor the recorder may be touched from the worker.
// Discarding the verdict at the thread boundary would silently drop every syntactic rejection from
// `aicmd.reject.*` and from the order log, which is precisely the evidence AIC-VAL-1 exists to
// produce.
struct CandidateOrder {
    Order order;
    OrderSnapshot snapshot;   // The snapshot it derives from: carries the reported track list that
                              // Stage-B check B3 must validate against, and the time B2 ages from.
    std::int64_t latencyMs = 0;
    int tokensIn = 0;
    int tokensOut = 0;

    // Prompt-cache accounting, carried across the slot for the same reason the Stage-A verdict is
    // (AIC-BE-2 / AIC-BE-3, v1.8.18): the worker may not touch the recorder, the counters, or the
    // log, so anything the simulation thread has to act on has to ride here or be lost. Dropping
    // these at the boundary is exactly what kept AIC-BE-2's recording SHALL unmet for twelve
    // revisions while the PRD reasoned about a guard reading a value that never arrived.
    int cacheReadTokens = 0;
    int cacheCreationTokens = 0;

    // Stage-A outcome, carried across the slot for the simulation thread to act on.
    bool stageAAccepted = false;
    RejectReason stageAReason = RejectReason::None;
    std::string stageADetail;
    std::string rawBody;      // Truncated by the recorder; retained for the order.rejected record.
};

// A mutex-guarded, latest-wins, single-slot exchange between the simulation thread and a worker.
//
// Latest-wins rather than a queue, and that is a correctness choice rather than a capacity one: a
// stale snapshot produces a stale order, so dropping an unconsumed older value is right, not lossy.
// A queue would faithfully deliver situations that no longer exist.
//
// The mutex is held only across a move of the held value. It is never held across I/O, never held
// while inference runs, and never held while any SDK call is in flight — the whole reason the
// worker gets a copy is so no lock has to span the seconds an inference takes.
template <typename T>
class LatestWinsSlot {
public:
    // Overwrites whatever was there. Returns true when a previous, unconsumed value was displaced,
    // which the caller counts as a dropped snapshot rather than treating as an error.
    bool publish(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        const bool displaced = held_.has_value();
        held_ = std::move(value);
        return displaced;
    }

    // Removes and returns the held value, if any. Consuming empties the slot, so the same value is
    // never processed twice.
    [[nodiscard]] std::optional<T> take() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!held_.has_value()) {
            return std::nullopt;
        }
        std::optional<T> out = std::move(held_);
        held_.reset();
        return out;
    }

    [[nodiscard]] bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return !held_.has_value();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        held_.reset();
    }

private:
    mutable std::mutex mutex_;
    std::optional<T> held_;
};

// The order currently published for one entity — what every aiCommander getter reads.
//
// Lives on the simulation thread only. It is NOT a LatestWinsSlot: a published order is read many
// times per frame by the Lua getters and replaced rarely, and it must never be consumed by
// reading. Publication is atomic in the sense that matters here: the whole struct is replaced or
// nothing is, so a script can never observe a half-applied order (AIC-VAL-2).
struct PublishedOrder {
    bool valid = false;
    Order order;
    std::int64_t serial = -1;
    double acceptedSimTimeS = 0.0;
};

} // namespace arkheon::aicommander
