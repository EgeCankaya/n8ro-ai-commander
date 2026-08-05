#include "TestSupport.h"

#include "EnvVar.h"

#include <core/logging/GlobalLogger.h>

#include <cstdio>
#include <filesystem>
#include <system_error>

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

static std::string& repoRootStorage() {
    static std::string root;
    return root;
}

const std::string& repoRoot() { return repoRootStorage(); }
void setRepoRoot(std::string root) { repoRootStorage() = std::move(root); }

// Walks up from the executable until a directory holding the solution file is found. The marker is
// `ai-commander.slnx` rather than `.git` or `data/`: a checkout can be exported without its git
// directory, and `data/` exists under the release tree too, which is exactly the directory this
// must not accidentally resolve to.
static std::string findRepoRoot(const char* argv0) {
    std::error_code ec;
    std::filesystem::path here = std::filesystem::absolute(std::filesystem::path(argv0), ec);
    if (ec) {
        return {};
    }
    for (here = here.parent_path(); !here.empty(); here = here.parent_path()) {
        if (std::filesystem::exists(here / "ai-commander.slnx", ec)) {
            return here.string();
        }
        if (!here.has_relative_path()) {
            break; // Reached the drive root; parent_path() is a fixed point from here.
        }
    }
    return {};
}

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

    // Resolved from argv[0] before initializeForTests() for the same reason as the release root:
    // that call repoints N8RO_RELEASE, and anything derived from the environment afterwards is
    // derived from the test-artifacts directory instead of from the real tree.
    if (argc > 0 && argv[0] != nullptr) {
        arkheon::aicommander::testing::setRepoRoot(
            arkheon::aicommander::testing::findRepoRoot(argv[0]));
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
