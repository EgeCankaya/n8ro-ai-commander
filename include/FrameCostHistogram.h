#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace arkheon::aicommander {

// Records the plugin's own per-frame cost so the p95/p99 budget in Success Metrics is a measured
// number rather than a claim (AIC-ARCH-2, Performance requirements).
//
// A bounded ring rather than an unbounded log: a long run must not grow this without limit, and
// the recent window is what an operator actually wants — "is the plugin inside budget now", not
// "was it in budget an hour ago". 4096 samples at 60 Hz is roughly the last minute.
//
// Percentiles are computed on demand by copying and sorting the window. That is O(n log n) on a
// few thousand elements and happens only when getStats() is called from Lua, never per frame; the
// per-frame path is a single push into the ring.
class FrameCostHistogram {
public:
    static constexpr std::size_t kWindowSize = 4096;

    void record(double microseconds);

    [[nodiscard]] double percentileUs(double fraction) const;
    [[nodiscard]] double p50Us() const { return percentileUs(0.50); }
    [[nodiscard]] double p95Us() const { return percentileUs(0.95); }
    [[nodiscard]] double p99Us() const { return percentileUs(0.99); }
    [[nodiscard]] double maxUs() const { return maxUs_; }
    [[nodiscard]] std::size_t sampleCount() const { return samples_.size(); }
    [[nodiscard]] std::int64_t totalFrames() const { return totalFrames_; }

    void reset();

private:
    std::vector<double> samples_;
    std::size_t next_ = 0;
    double maxUs_ = 0.0;          // Over the whole run, not just the window: the worst frame is
                                  // the one that matters for a p99 budget, and it must not age out.
    std::int64_t totalFrames_ = 0;
};

} // namespace arkheon::aicommander
