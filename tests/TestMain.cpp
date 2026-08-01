#include "TestSupport.h"

#include "EnvVar.h"

#include <core/logging/GlobalLogger.h>

#include <cstdio>

namespace arkheon::aicommander::testing {

// Function-local static: the registry must be constructed before the first AutoRegister runs, and
// static-initialization order across translation units is otherwise unspecified.
std::vector<TestFactory>& registry() {
    static std::vector<TestFactory> factories;
    return factories;
}

static std::string& releaseRootStorage() {
    static std::string root;
    return root;
}

const std::string& releaseRoot() { return releaseRootStorage(); }
void setReleaseRoot(std::string root) { releaseRootStorage() = std::move(root); }

} // namespace arkheon::aicommander::testing

// The Phase-1a suite. It runs with no inference server and no network by construction: nothing it
// links constructs an IHttpClient, and the only backends it exercises are `stub` and `replay`.
int main(int argc, char* argv[]) {
    // Order matters: initializeForTests() repoints N8RO_RELEASE at a test-artifacts directory, so
    // the real release root has to be read first or every SDK-path lookup resolves under it.
    {
        std::string release;
        if (arkheon::aicommander::tryReadEnvVar("N8RO_RELEASE", release)) {
            arkheon::aicommander::testing::setReleaseRoot(release);
        }
    }

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
