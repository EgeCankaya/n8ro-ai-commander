#pragma once

#include "Order.h"
#include "Snapshot.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace arkheon::aicommander {

// A candidate order coming back off the worker, before Stage B has seen it.
struct CandidateOrder {
    Order order;
    OrderSnapshot snapshot;   // The snapshot it derives from: carries the reported track list that
                              // Stage-B check B3 must validate against, and the time B2 ages from.
    std::int64_t latencyMs = 0;
    int tokensIn = 0;
    int tokensOut = 0;
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
