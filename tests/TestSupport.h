#pragma once

#include <test/TestFramework.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

// Assertion helpers for the SDK's TestRunner, which reports a failure by returning false from
// run() and writing a message into `failureMessage`.
//
// Each macro appends the file and line, because a table-driven suite reports the same assertion
// text from many rows and "expected reject reason 'schema', got 'range'" is only actionable when
// you can tell which row said it.

#define AIC_FAIL(msg)                                                                              \
    do {                                                                                           \
        std::ostringstream aicFailStream;                                                          \
        aicFailStream << msg << "  [" << __FILE__ << ":" << __LINE__ << "]";                       \
        failureMessage = aicFailStream.str();                                                      \
        return false;                                                                              \
    } while (0)

#define AIC_EXPECT_TRUE(cond, msg)                                                                 \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            AIC_FAIL(msg << " (expected: " << #cond << ")");                                       \
        }                                                                                          \
    } while (0)

#define AIC_EXPECT_FALSE(cond, msg)                                                                \
    do {                                                                                           \
        if ((cond)) {                                                                              \
            AIC_FAIL(msg << " (expected NOT: " << #cond << ")");                                   \
        }                                                                                          \
    } while (0)

#define AIC_EXPECT_EQ(actual, expected, msg)                                                       \
    do {                                                                                           \
        const auto& aicActual = (actual);                                                          \
        const auto& aicExpected = (expected);                                                      \
        if (!(aicActual == aicExpected)) {                                                         \
            AIC_FAIL(msg << " - expected '" << aicExpected << "', got '" << aicActual << "'");     \
        }                                                                                          \
    } while (0)

namespace arkheon::aicommander::testing {

// Collects the suite's test cases so each translation unit can contribute its own without a
// central registry that every new file has to be threaded into by hand.
using TestFactory = std::unique_ptr<n8ro::core::TestCase> (*)();

std::vector<TestFactory>& registry();

// The release root, captured in main() BEFORE the SDK test framework initializes.
//
// This has to be captured early: GlobalLogger::initializeForTests() repoints N8RO_RELEASE at a
// per-run test-artifacts directory so log files land there, which is correct for logging and
// wrong for anything that needs the actual installed tree. Reading the variable from inside a test
// case yields "<release>\test_artifacts" and a file-not-found on every SDK path.
//
// Empty when the suite was run without setup.cmd.
const std::string& releaseRoot();
void setReleaseRoot(std::string root);

// The REPOSITORY root, derived in main() by walking up from the executable's own path until a
// directory holding `ai-commander.slnx` is found.
//
// It cannot be the working directory and it cannot be N8RO_RELEASE. ci-selfhosted.yml runs this
// suite as `cd /d "%N8RO_RELEASE%"` followed by an absolute path to the exe, so the CWD during a CI
// run is the RELEASE TREE, not the checkout - a test resolving "data/doctrine.txt" relative to the
// working directory would read C:\N8RO\data\ and find something else or nothing at all. And
// N8RO_RELEASE is repointed at test_artifacts by initializeForTests(), which is why releaseRoot()
// above exists in the first place.
//
// Walking up from argv[0] is the only one of the three that holds under every invocation this
// project actually uses: the Release build (tests/bin/release/), the AddressSanitizer build
// (tests/bin/asan/), and a run started from any directory.
//
// Empty when the marker was not found, which a test must treat as a FAILURE rather than a skip -
// see PromptRendererTests.cpp. A guard that quietly does nothing when it cannot find its input is
// worse than no guard, because the suite still reports green.
const std::string& repoRoot();
void setRepoRoot(std::string root);

struct AutoRegister {
    explicit AutoRegister(TestFactory factory) { registry().push_back(factory); }
};

} // namespace arkheon::aicommander::testing

// Declares a test case class with a name, a body, and self-registration.
// Usage:
//   AIC_TEST(ConfigDefaultsAreFailClosed) {
//       ... ; return true;
//   }
#define AIC_TEST(CLASS)                                                                            \
    namespace arkheon::aicommander::testing {                                                      \
    class CLASS final : public n8ro::core::TestCase {                                              \
    public:                                                                                        \
        N8RO_CORE_TEST_NAME(CLASS)                                                                 \
        bool run(std::string& failureMessage) override;                                            \
    };                                                                                             \
    static std::unique_ptr<n8ro::core::TestCase> make##CLASS() {                                   \
        return std::make_unique<CLASS>();                                                          \
    }                                                                                              \
    static const AutoRegister register##CLASS{&make##CLASS};                                       \
    }                                                                                              \
    bool arkheon::aicommander::testing::CLASS::run(std::string& failureMessage)
