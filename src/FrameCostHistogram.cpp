#include "FrameCostHistogram.h"

#include <algorithm>
#include <cmath>

namespace arkheon::aicommander {

void FrameCostHistogram::record(double microseconds) {
    if (!std::isfinite(microseconds) || microseconds < 0.0) {
        return;
    }
    ++totalFrames_;
    if (microseconds > maxUs_) {
        maxUs_ = microseconds;
    }

    if (samples_.size() < kWindowSize) {
        samples_.push_back(microseconds);
        return;
    }
    samples_[next_] = microseconds;
    next_ = (next_ + 1) % kWindowSize;
}

double FrameCostHistogram::percentileUs(double fraction) const {
    if (samples_.empty()) {
        return 0.0;
    }
    std::vector<double> sorted = samples_;
    std::sort(sorted.begin(), sorted.end());

    if (fraction <= 0.0) {
        return sorted.front();
    }
    if (fraction >= 1.0) {
        return sorted.back();
    }
    // Nearest-rank. Chosen over interpolation deliberately: an interpolated p99 can report a value
    // no frame actually took, and a budget assertion should name a frame that really happened.
    const std::size_t rank =
        static_cast<std::size_t>(std::ceil(fraction * static_cast<double>(sorted.size())));
    const std::size_t index = rank == 0 ? 0 : rank - 1;
    return sorted[std::min(index, sorted.size() - 1)];
}

void FrameCostHistogram::reset() {
    samples_.clear();
    next_ = 0;
    maxUs_ = 0.0;
    totalFrames_ = 0;
}

} // namespace arkheon::aicommander
