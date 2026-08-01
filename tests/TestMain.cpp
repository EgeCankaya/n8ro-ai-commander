#include "TestSupport.h"

#include <core/logging/GlobalLogger.h>

#include <cstdio>

namespace arkheon::aicommander::testing {

// Function-local static: the registry must be constructed before the first AutoRegister runs, and
// static-initialization order across translation units is otherwise unspecified.
std::vector<TestFactory>& registry() {
    static std::vector<TestFactory> factories;
    return factories;
}

} // namespace arkheon::aicommander::testing

// The Phase-1a suite. It runs with no inference server and no network by construction: nothing it
// links constructs an IHttpClient, and the only backends it exercises are `stub` and `replay`.
int main(int argc, char* argv[]) {
    n8ro::core::GlobalLogger::initializeForTests();

    n8ro::core::TestRunner runner;
    for (const auto factory : arkheon::aicommander::testing::registry()) {
        runner.add(factory());
    }

    std::printf("ai-commander test suite: %zu cases\n",
                arkheon::aicommander::testing::registry().size());

    const int result = runner.runFromArguments(argc, argv);
    n8ro::core::GlobalLogger::flush();
    return result;
}
