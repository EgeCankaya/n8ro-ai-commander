#include "CommanderRuntime.h"

#include <core/json/JsonValue.h>

namespace arkheon::aicommander {

std::string CommanderRuntime::statsJson() const {
    // Built through the SDK's JsonValue rather than by string concatenation so a detail string
    // carrying a quote or a backslash cannot produce a malformed document. getStats() is read by
    // tooling, and a stats surface that can be broken by its own error message is not a health
    // surface.
    n8ro::core::JsonValue root = n8ro::core::JsonValue::object();

    (void)root.setBool("enabled", config_.enabled);
    (void)root.setString("backend", toString(config_.backend));
    (void)root.setBool("operational", isOperational());

    n8ro::core::JsonValue probe = n8ro::core::JsonValue::object();
    (void)probe.setString("result", toString(probeReport_.result));
    (void)probe.setString("detail", probeReport_.detail);
    (void)probe.setString("entityId", probeReport_.probedEntityId);
    (void)root.set("runtimeColumnProbe", probe);

    // Order-pipeline counters land here as the pipeline is built out (AIC-DET-1 / Observability).
    // They are emitted as zeros from the first build rather than appearing later, so anything
    // consuming getStats() sees a stable shape across the milestone.
    n8ro::core::JsonValue orders = n8ro::core::JsonValue::object();
    (void)orders.setInt64("requested", 0);
    (void)orders.setInt64("accepted", 0);
    (void)orders.setInt64("rejected", 0);
    (void)orders.setInt64("timeouts", 0);
    (void)root.set("orders", orders);

    return root.toString();
}

} // namespace arkheon::aicommander
