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
