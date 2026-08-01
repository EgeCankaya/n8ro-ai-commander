#pragma once

#include <string>

namespace arkheon::aicommander {

// Reads an environment variable by name.
//
// std::getenv is unavailable here: the projects build with SDLCheck, which promotes MSVC's C4996
// deprecation of getenv to an error. This wraps _dupenv_s so there is one place that knows that,
// rather than a _CRT_SECURE_NO_WARNINGS that would switch the whole check off.
//
// The returned value is a copy the caller owns. Per ADR-5 a credential read through this must be
// held only for the duration of the request that uses it: never persisted, never logged, never
// written to an order record, and never returned from getConfigFields().
[[nodiscard]] bool tryReadEnvVar(const char* name, std::string& out);

} // namespace arkheon::aicommander
