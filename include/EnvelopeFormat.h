#pragma once

namespace arkheon::aicommander {

// How the backend wraps the order document in its HTTP response body (Stage-A check A2).
//
// This is a value rather than a policy object on purpose. Stage A runs on the worker and is a pure
// function of its arguments - it touches no IEntityManager, no ScriptingApiContext, no client. The
// envelope is the one genuinely backend-specific thing it needs to know, so it arrives as data.
// Giving Stage A an ILlmClient pointer instead would have handed the worker a live object and
// dissolved the property that makes the two-stage split safe.
enum class EnvelopeFormat {
    // The body IS the order document. What the `stub` and `replay` adapters return, and what every
    // Phase-1a test was written against.
    Raw,

    // Ollama's POST /api/generate response: a JSON object carrying the order as a JSON STRING in
    // its `response` field, alongside `done`, `eval_count`, `total_duration` and friends. A2
    // unwraps that field before A3 parses it.
    OllamaGenerate,
};

} // namespace arkheon::aicommander
