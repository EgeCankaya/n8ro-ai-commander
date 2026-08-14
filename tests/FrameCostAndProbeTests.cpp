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

    // And the three geodetic leaves, in the SLASH-joined schema form (v1.8.55). Both spellings are
    // asserted in one test on purpose: the two path conventions live side by side in one component,
    // and a leaf moved from one list to the other would otherwise pass every check here.
    AIC_EXPECT_TRUE(asked.count("positionGeodetic/latitudeDeg") == 1, "latitudeDeg must be probed");
    AIC_EXPECT_TRUE(asked.count("positionGeodetic/longitudeDeg") == 1, "longitudeDeg must be probed");
    AIC_EXPECT_TRUE(asked.count("positionGeodetic/altitudeHaeM") == 1, "altitudeHaeM must be probed");

    AIC_EXPECT_EQ(asked.size(), static_cast<std::size_t>(6),
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

// ---------------------------------------------------------------------------------------------
// AIC-ARCH-4 widened to all seven leaves the plugin reads (v1.8.55, PRD §Corrections item 71).
// ---------------------------------------------------------------------------------------------

namespace {

// Resolves everything. The baseline each failure case mutates exactly one leaf away from.
std::optional<double> resolveAll(const std::string&, std::string_view) { return 0.0; }

// Passed explicitly in every test below rather than defaulted, so that no test silently exercises
// the absent-seam path when it meant to exercise a present one.
const HealthLeafProbe kHealthResolves = [](const std::string&) { return true; };
const HealthLeafProbe kHealthMissing = [](const std::string&) { return false; };

} // namespace

// The geodetic triple is fatal from v1.8.55, and the detail must name the leaf. Before this, an
// unresolvable position leaf presented as buildSnapshot returning false on every entity every tick,
// naming no path - "the commander does nothing", with nothing to grep for.
AIC_TEST(ProbeFailsWhenAGeodeticLeafDoesNotResolve) {
    for (const char* broken :
         {"positionGeodetic/latitudeDeg", "positionGeodetic/longitudeDeg",
          "positionGeodetic/altitudeHaeM"}) {
        const std::string brokenPath = broken;
        const ColumnReader reader =
            [&brokenPath](const std::string&, std::string_view path) -> std::optional<double> {
                if (path == brokenPath) {
                    return std::nullopt;
                }
                return 0.0;
            };

        const ProbeReport report = probeSnapshotLeavesWith("RedSu35_01", reader, kHealthResolves);
        AIC_EXPECT_TRUE(report.result == ProbeResult::Fail,
                        "an unresolvable " + brokenPath + " must fail the probe");
        AIC_EXPECT_TRUE(report.detail.find(brokenPath) != std::string::npos,
                        "the failure detail must name the offending leaf, got: " + report.detail);
        AIC_EXPECT_TRUE(report.detail.find("schema-reference.json") != std::string::npos,
                        "a schema leaf must point at the schema reference rather than at the runtime-column header");
    }
    return true;
}

// THE ASSERTION THE WHOLE SEVERITY SPLIT EXISTS FOR, and the one a later refactor is likeliest to
// break: an unreadable health leaf PASSES. It feeds only the C27 uncommandable guard, whose
// specified behaviour on an unreadable tier is to treat the airframe as commandable (item 70(d)).
// Failing here would disable the entire commander over a leaf worth four wasted orders per lost
// airframe - the opposite of the trade the guard itself makes, two PRD items apart.
AIC_TEST(ProbePassesButWarnsWhenTheHealthLeafDoesNotResolve) {
    const ProbeReport report = probeSnapshotLeavesWith("RedSu35_01", resolveAll, kHealthMissing);

    AIC_EXPECT_TRUE(report.result == ProbeResult::Pass,
                    "an unreadable health leaf must NOT fail the probe - it must not disable the commander");
    AIC_EXPECT_FALSE(report.warning.empty(), "but it must not be silent either");
    AIC_EXPECT_TRUE(report.warning.find("componentLifecycle/health") != std::string::npos,
                    "the warning must name the leaf, got: " + report.warning);
    AIC_EXPECT_TRUE(report.warning.find("ENABLED") != std::string::npos,
                    "the warning must say the commander is still running, or a reader assumes it is not");
    return true;
}

// A healthy tree carries no warning at all. Without this, the test above passes just as well against
// a build that warns unconditionally.
AIC_TEST(ProbeCarriesNoWarningWhenEveryLeafResolves) {
    const ProbeReport report = probeSnapshotLeavesWith("RedSu35_01", resolveAll, kHealthResolves);
    AIC_EXPECT_TRUE(report.result == ProbeResult::Pass, "everything resolved");
    AIC_EXPECT_TRUE(report.warning.empty(),
                    "a healthy tree must carry no warning, got: " + report.warning);
    return true;
}

// A fatal leaf outranks the warning. When the tree is broken enough to fail, the health check must
// not run at all: the verdict is already decided and reading further is noise in the host log.
AIC_TEST(ProbeDoesNotConsultHealthOnceALeafHasFailed) {
    int healthReads = 0;
    const HealthLeafProbe counting = [&healthReads](const std::string&) {
        ++healthReads;
        return false;
    };
    const ColumnReader velocityBroken =
        [](const std::string&, std::string_view path) -> std::optional<double> {
            if (path == "velocityNed.x") {
                return std::nullopt;
            }
            return 0.0;
        };

    const ProbeReport report = probeSnapshotLeavesWith("RedSu35_01", velocityBroken, counting);
    AIC_EXPECT_TRUE(report.result == ProbeResult::Fail, "a broken velocity column still fails");
    AIC_EXPECT_EQ(healthReads, 0, "health must not be consulted once the verdict is already Fail");
    AIC_EXPECT_TRUE(report.warning.empty(), "a failing probe reports its failure, not a warning");
    return true;
}

// The velocity columns are checked before the geodetic leaves, and the probe still short-circuits.
AIC_TEST(ProbeStopsAtTheFirstGeodeticFailure) {
    int reads = 0;
    const ColumnReader reader =
        [&reads](const std::string&, std::string_view path) -> std::optional<double> {
            ++reads;
            if (path == "positionGeodetic/latitudeDeg") {
                return std::nullopt;
            }
            return 0.0;
        };

    const ProbeReport report = probeSnapshotLeavesWith("RedSu35_01", reader, kHealthResolves);
    AIC_EXPECT_TRUE(report.result == ProbeResult::Fail, "must fail");
    AIC_EXPECT_EQ(reads, 4, "three velocity columns, then the first geodetic leaf, then stop");
    return true;
}

// The retained two-argument spelling must behave as it did before v1.8.55 for the leaves it covered,
// and must produce no warning: an absent health seam is "not checked", not "not found".
AIC_TEST(ProbeWithoutAHealthSeamChecksTheFatalLeavesAndWarnsAboutNothing) {
    const ProbeReport report = probeRuntimeColumnsWith("RedSu35_01", resolveAll);
    AIC_EXPECT_TRUE(report.result == ProbeResult::Pass, "the fatal leaves all resolved");
    AIC_EXPECT_TRUE(report.warning.empty(),
                    "an absent seam means unchecked, and unchecked must never present as a finding");
    return true;
}

// No subject is still NotRun, and a NotRun verdict must not carry a warning either: the caller
// retries on a later frame, and a warning on an unearned verdict would latch a finding about a tree
// nobody has looked at yet.
AIC_TEST(ProbeIsNotRunWithoutASubjectEvenWhenHealthIsMissing) {
    const ProbeReport report = probeSnapshotLeavesWith("", resolveAll, kHealthMissing);
    AIC_EXPECT_TRUE(report.result == ProbeResult::NotRun, "no subject means NotRun");
    AIC_EXPECT_TRUE(report.warning.empty(), "an unearned verdict carries no findings");
    return true;
}

// The pass detail must name what it actually verified. It named velocityNed alone for thirteen
// revisions - true then, and a false claim now that it verifies more.
AIC_TEST(ProbePassDetailNamesEveryLeafClassItVerified) {
    const ProbeReport report = probeSnapshotLeavesWith("RedSu35_01", resolveAll, kHealthResolves);
    AIC_EXPECT_TRUE(report.detail.find("velocityNed") != std::string::npos,
                    "the detail must still name the runtime columns, got: " + report.detail);
    AIC_EXPECT_TRUE(report.detail.find("positionGeodetic") != std::string::npos,
                    "and it must name the geodetic leaves it now also verifies, got: " + report.detail);
    return true;
}
