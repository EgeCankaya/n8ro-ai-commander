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

    // Anthropic's POST /v1/messages response: a JSON object whose `content` is an ARRAY of typed
    // blocks, the order document being the `text` of the first block of type "text", alongside
    // `stop_reason`, `stop_details`, `model`, and `usage`.
    //
    // Two things make this shape different in kind from OllamaGenerate rather than merely in layout,
    // and both are why it is a distinct value instead of a parameterization of the last one:
    //
    //   - It can carry a REFUSAL. `stop_reason == "refusal"` means the model declined, and it is
    //     checked BEFORE `content` is read, because on a refusal `content` is empty or partial and
    //     reading it first is how a partial answer gets mistaken for a whole one. This is the only
    //     envelope that can produce RejectReason::Refusal, which is why that reason existed with no
    //     producer until this value existed.
    //   - The refusal signal is `stop_reason`, NOT `stop_details`. `stop_details` may be null even
    //     on a refusal, so a guard written against it would miss refusals and then read the very
    //     content the check exists to protect against.
    ClaudeMessages,
};

} // namespace arkheon::aicommander
