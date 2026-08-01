#include "TestSupport.h"

#include "FrameCostHistogram.h"
#include "RuntimeColumnProbe.h"

#include <optional>
#include <set>
#include <string>
#include <string_view>

using namespace arkheon::aicommander;

// Percentiles must name a frame that really happened. Nearest-rank rather than interpolation:
// an interpolated p99 can report a duration no frame ever took, and a budget assertion built on
// that is asserting a fiction.
AIC_TEST(FrameCostPercentilesAreNearestRank) {
    FrameCostHistogram histogram;
    for (int i = 1; i <= 100; ++i) {
        histogram.record(static_cast<double>(i));
    }

    AIC_EXPECT_EQ(histogram.sampleCount(), static_cast<std::size_t>(100), "sample count");
    AIC_EXPECT_EQ(histogram.totalFrames(), static_cast<std::int64_t>(100), "frame count");
    AIC_EXPECT_EQ(histogram.p50Us(), 50.0, "p50 of 1..100");
    AIC_EXPECT_EQ(histogram.p95Us(), 95.0, "p95 of 1..100");
    AIC_EXPECT_EQ(histogram.p99Us(), 99.0, "p99 of 1..100");
    AIC_EXPECT_EQ(histogram.maxUs(), 100.0, "max of 1..100");
    return true;
}

// An empty histogram must report zeros rather than reading off the end of an empty window.
AIC_TEST(FrameCostHandlesAnEmptyWindow) {
    const FrameCostHistogram histogram;
    AIC_EXPECT_EQ(histogram.p95Us(), 0.0, "p95 of nothing");
    AIC_EXPECT_EQ(histogram.maxUs(), 0.0, "max of nothing");
    AIC_EXPECT_EQ(histogram.sampleCount(), static_cast<std::size_t>(0), "no samples");
    return true;
}

// The window is bounded so a long run cannot grow it, but the worst frame over the WHOLE run must
// survive - a p99 budget is about the worst case, and letting it age out of a rolling window would
// quietly hide the frame that actually blew the budget.
AIC_TEST(FrameCostRetainsTheRunMaximumBeyondTheWindow) {
    FrameCostHistogram histogram;
    histogram.record(9999.0);   // The spike, recorded first.
    for (std::size_t i = 0; i < FrameCostHistogram::kWindowSize + 500; ++i) {
        histogram.record(10.0);
    }

    AIC_EXPECT_EQ(histogram.sampleCount(), FrameCostHistogram::kWindowSize,
                  "the window must stay bounded");
    AIC_EXPECT_EQ(histogram.maxUs(), 9999.0,
                  "the run maximum must survive after the spike aged out of the window");
    AIC_EXPECT_EQ(histogram.p95Us(), 10.0, "the recent window reflects recent frames");
    AIC_EXPECT_TRUE(histogram.totalFrames() > static_cast<std::int64_t>(FrameCostHistogram::kWindowSize),
                    "total frames counts every frame, not just the window");
    return true;
}

// Garbage in must not corrupt the histogram: a negative or non-finite duration is a bug upstream,
// and letting it into the window would poison every percentile after it.
AIC_TEST(FrameCostRejectsNonsenseSamples) {
    FrameCostHistogram histogram;
    histogram.record(-1.0);
    histogram.record(std::numeric_limits<double>::quiet_NaN());
    histogram.record(std::numeric_limits<double>::infinity());
    AIC_EXPECT_EQ(histogram.sampleCount(), static_cast<std::size_t>(0),
                  "non-finite and negative samples must be dropped");

    histogram.record(5.0);
    AIC_EXPECT_EQ(histogram.p95Us(), 5.0, "a valid sample still lands");
    return true;
}

// AIC-ARCH-4's PASS path: all three columns resolve.
AIC_TEST(ProbePassesWhenEveryColumnResolves) {
    std::set<std::string> asked;
    const ColumnReader reader =
        [&asked](const std::string&, std::string_view path) -> std::optional<double> {
            asked.insert(std::string(path));
            return 0.0;   // Resolves, reads zero. A stationary aircraft is a valid subject.
        };

    const ProbeReport report = probeRuntimeColumnsWith("RedSu35_01", reader);
    AIC_EXPECT_TRUE(report.result == ProbeResult::Pass,
                    "all columns resolving must pass, got: " + report.detail);
    AIC_EXPECT_EQ(report.probedEntityId, std::string("RedSu35_01"), "subject recorded");

    // All three velocity leaves, in the DOT-joined form TransformRuntimeColumns.h declares - not
    // the schema's slash form. Getting this wrong is the exact mistake the probe exists to catch.
    AIC_EXPECT_TRUE(asked.count("velocityNed.x") == 1, "velocityNed.x must be probed");
    AIC_EXPECT_TRUE(asked.count("velocityNed.y") == 1, "velocityNed.y must be probed");
    AIC_EXPECT_TRUE(asked.count("velocityNed.z") == 1, "velocityNed.z must be probed");
    AIC_EXPECT_EQ(asked.size(), static_cast<std::size_t>(3),
                  "the probe must not read a deliberately invalid path - that sprays "
                  "DynamicLayout errors into the host log on every run");
    return true;
}

// The branch the whole FR exists for. A column that does not resolve must FAIL, and the detail
// must name the offending path - an operator diagnosing this has only the log line to work from.
AIC_TEST(ProbeFailsWhenAColumnDoesNotResolve) {
    for (const char* broken : {"velocityNed.x", "velocityNed.y", "velocityNed.z"}) {
        const std::string brokenPath = broken;
        const ColumnReader reader =
            [&brokenPath](const std::string&, std::string_view path) -> std::optional<double> {
                if (path == brokenPath) {
                    return std::nullopt;   // The silent-rename case, made loud (OQ-9).
                }
                return 0.0;
            };

        const ProbeReport report = probeRuntimeColumnsWith("RedSu35_01", reader);
        AIC_EXPECT_TRUE(report.result == ProbeResult::Fail,
                        "an unresolvable '" + brokenPath + "' must fail the probe");
        AIC_EXPECT_TRUE(report.detail.find(brokenPath) != std::string::npos,
                        "the failure detail must name the offending path, got: " + report.detail);
        AIC_EXPECT_TRUE(report.detail.find("TransformRuntimeColumns.h") != std::string::npos,
                        "the failure detail must point at the header that owns these names");
    }
    return true;
}

// A probe with no subject is NotRun, not Fail: the caller retries on a later frame rather than
// recording a verdict the scenario has not had a chance to earn. Reporting Fail here would
// disable the commander on every healthy startup.
AIC_TEST(ProbeIsNotRunWithoutASubject) {
    const ColumnReader reader = [](const std::string&, std::string_view) -> std::optional<double> {
        return 0.0;
    };
    const ProbeReport report = probeRuntimeColumnsWith("", reader);
    AIC_EXPECT_TRUE(report.result == ProbeResult::NotRun,
                    "no subject means NotRun, never Fail");
    return true;
}

// The probe short-circuits on the first failure. It must not keep reading after a column has
// already disqualified the tree - each extra read of a bad path is another host-log error.
AIC_TEST(ProbeStopsAtTheFirstFailure) {
    int reads = 0;
    const ColumnReader reader =
        [&reads](const std::string&, std::string_view) -> std::optional<double> {
            ++reads;
            return std::nullopt;
        };

    const ProbeReport report = probeRuntimeColumnsWith("RedSu35_01", reader);
    AIC_EXPECT_TRUE(report.result == ProbeResult::Fail, "must fail");
    AIC_EXPECT_EQ(reads, 1, "the probe must stop at the first unresolvable column");
    return true;
}
