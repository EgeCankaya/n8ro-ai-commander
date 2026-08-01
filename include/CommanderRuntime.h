#pragma once

#include "CommanderConfig.h"
#include "RuntimeColumnProbe.h"

#include <string>

namespace arkheon::aicommander {

// The plugin's simulation-thread state: the live configuration, the AIC-ARCH-4 probe verdict, and
// the counters behind aiCommander.getStats().
//
// Thread affinity: everything here is touched only on the host's single update thread — the frame
// callbacks and the Lua callbacks both run there (IPlugin.h:36). Nothing in this type is shared
// with a worker; the worker receives a POD snapshot by value and holds no pointer back into it.
//
// This is deliberately the *only* mutable plugin state the Lua callbacks can reach, so the
// "no I/O, no blocking, O(1)" obligation on every registered function is a property of this class
// rather than a promise each callback has to keep on its own.
class CommanderRuntime {
public:
    CommanderRuntime() = default;

    CommanderRuntime(const CommanderRuntime&) = delete;
    CommanderRuntime& operator=(const CommanderRuntime&) = delete;

    [[nodiscard]] const CommanderConfig& config() const { return config_; }

    // Replaces the live configuration wholesale. The caller has already validated it — this type
    // never sees a partially applied config, which is what AIC-API-2's all-or-nothing rule means
    // in practice.
    void setConfig(const CommanderConfig& config) { config_ = config; }

    [[nodiscard]] const ProbeReport& probeReport() const { return probeReport_; }
    void setProbeReport(ProbeReport report) { probeReport_ = std::move(report); }

    // True when the commander may issue orders at all: the master switch is on and the runtime
    // columns the snapshot depends on are known good. A failed probe disables the commander rather
    // than letting it run on fabricated zeros (AIC-ARCH-4).
    [[nodiscard]] bool isOperational() const {
        return config_.enabled && probeReport_.result == ProbeResult::Pass;
    }

    // The health surface (AIC-API-1). Returns a JSON object as text; there is no HTTP endpoint
    // because the plugin is in-process.
    [[nodiscard]] std::string statsJson() const;

private:
    CommanderConfig config_;
    ProbeReport probeReport_;
};

} // namespace arkheon::aicommander
