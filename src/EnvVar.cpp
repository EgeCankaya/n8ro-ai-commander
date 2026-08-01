#include "EnvVar.h"

#include <cstdlib>

namespace arkheon::aicommander {

bool tryReadEnvVar(const char* name, std::string& out) {
    out.clear();
    if (name == nullptr || *name == '\0') {
        return false;
    }

#ifdef _WIN32
    char* buffer = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr) {
        return false;
    }
    // length includes the terminator; assign explicitly rather than relying on it being
    // NUL-terminated at exactly that offset.
    out.assign(buffer, length > 0 ? length - 1 : 0);
    std::free(buffer);
#else
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return false;
    }
    out.assign(value);
#endif

    return !out.empty();
}

} // namespace arkheon::aicommander
