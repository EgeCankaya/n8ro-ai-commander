#include "TestSupport.h"

#include "CacheMinimumGuard.h"
#include "ClaudeLlmClient.h"
#include "EnvelopeFormat.h"
#include "Order.h"
#include "OrderSchema.h"
#include "OrderValidatorStageA.h"
#include "RejectReason.h"

#include <core/json/JsonValue.h>
#include <core/net/IHttpClient.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

using arkheon::aicommander::CacheState;
using arkheon::aicommander::ClaudeClientConfig;
using arkheon::aicommander::ClaudeLlmClient;
using arkheon::aicommander::EnvelopeFormat;
using arkheon::aicommander::LlmRequest;
using arkheon::aicommander::LlmResult;
using arkheon::aicommander::classifyCacheState;
using arkheon::aicommander::Posture;
using arkheon::aicommander::RejectReason;
using arkheon::aicommander::orderJsonSchema;
using arkheon::aicommander::orderJsonSchemaForStructuredOutputs;
using arkheon::aicommander::toString;
using arkheon::aicommander::validateStageA;
using n8ro::core::HttpRequest;
using n8ro::core::HttpResponse;
using n8ro::core::JsonValue;

namespace {

// The sentinel. Every redaction assertion in this file looks for THIS string, which is chosen to be
// implausible as a substring of anything else the adapter emits. A test that searched for a
// realistic-looking key could pass by luck.
constexpr const char* kSentinelKey = "sk-ant-SENTINEL-DO-NOT-LEAK-4f2b91c7";
constexpr const char* kKeyEnvVar = "AIC_TEST_CLAUDE_KEY";

// Same fake transport shape as the local adapter's suite: answers from a script, records what was
// put on the wire, and needs no server and no network.
class FakeHttpClient final : public n8ro::core::IHttpClient {
public:
    struct Reply {
        bool completed = true;   // false == std::nullopt from send(), i.e. a transport failure
        int statusCode = 200;
        std::string body;
    };

    std::vector<HttpRequest> requests;
    std::vector<Reply> replies;

    std::optional<HttpResponse> send(const HttpRequest& request) override {
        requests.push_back(request);
        const std::size_t index = requests.size() - 1;
        const Reply reply = replies.empty()
            ? Reply{}
            : replies[index < replies.size() ? index : replies.size() - 1];
        if (!reply.completed) {
            return std::nullopt;
        }
        HttpResponse response;
        response.statusCode = reply.statusCode;
        response.body = reply.body;
        return response;
    }

    [[nodiscard]] std::string headerValue(std::size_t requestIndex, const std::string& name) const {
        if (requestIndex >= requests.size()) {
            return {};
        }
        for (const auto& header : requests[requestIndex].headers) {
            if (header.name == name) {
                return header.value;
            }
        }
        return {};
    }
};

FakeHttpClient* attachFake(ClaudeLlmClient& client, std::unique_ptr<FakeHttpClient> fake) {
    FakeHttpClient* borrowed = fake.get();
    auto shared = std::make_shared<std::unique_ptr<FakeHttpClient>>(std::move(fake));
    client.setHttpClientFactory([shared]() -> std::unique_ptr<n8ro::core::IHttpClient> {
        return std::unique_ptr<n8ro::core::IHttpClient>(shared->release());
    });
    return borrowed;
}

[[nodiscard]] ClaudeClientConfig testConfig() {
    ClaudeClientConfig config;
    config.baseUrl = "https://api.anthropic.com";
    config.model = "claude-haiku-4-5";
    config.maxTokens = 512;
    config.apiKeyEnvVar = kKeyEnvVar;
    config.timeoutS = 90;
    return config;
}

void setKey(const char* value) {
#ifdef _WIN32
    (void)_putenv_s(kKeyEnvVar, value == nullptr ? "" : value);
#else
    if (value == nullptr) {
        (void)unsetenv(kKeyEnvVar);
    } else {
        (void)setenv(kKeyEnvVar, value, 1);
    }
#endif
}

[[nodiscard]] std::string orderText(Posture posture, const std::string& entityId = "RedSu35_01") {
    JsonValue order = JsonValue::object();
    (void)order.setInt64("schemaVersion", 1);
    (void)order.setString("entityId", entityId);
    (void)order.setString("posture", toString(posture));
    (void)order.setDouble("cruiseSpeedMps", 250.0);
    (void)order.setString("roe", "weaponsTight");
    (void)order.setString("reason", "Unit-test order.");
    if (arkheon::aicommander::postureRequiresTarget(posture)) {
        (void)order.setString("targetEntityId", "BlueF16_01");
    } else {
        (void)order.setString("targetEntityId", "");
    }
    if (arkheon::aicommander::postureRequiresWaypoint(posture)) {
        JsonValue waypoint = JsonValue::object();
        (void)waypoint.setDouble("latitudeDeg", 13.5);
        (void)waypoint.setDouble("longitudeDeg", 144.8);
        (void)waypoint.setDouble("altitudeHaeM", 10000.0);
        (void)order.set("waypoint", waypoint);
    }
    (void)order.setDouble("orbitRadiusM", posture == Posture::Hold ? 8000.0 : 0.0);
    return order.toString();
}

// A well-formed Anthropic Messages response carrying `inner` as its single text block.
//
// `cacheCreationTokens` defaults to 0 so every pre-existing call site keeps its meaning, and the
// cache tests below pass it explicitly. The pair is what the guard reads: see CacheMinimumGuard.h.
[[nodiscard]] std::string claudeEnvelope(
    const std::string& inner, int cacheReadTokens = 0, int cacheCreationTokens = 0) {
    JsonValue block = JsonValue::object();
    (void)block.setString("type", "text");
    (void)block.setString("text", inner);
    JsonValue content = JsonValue::array();
    (void)content.pushBack(block);

    JsonValue usage = JsonValue::object();
    (void)usage.setInt64("input_tokens", 4500);
    (void)usage.setInt64("output_tokens", 78);
    (void)usage.setInt64("cache_read_input_tokens", cacheReadTokens);
    (void)usage.setInt64("cache_creation_input_tokens", cacheCreationTokens);

    JsonValue root = JsonValue::object();
    (void)root.setString("id", "msg_test");
    (void)root.setString("type", "message");
    (void)root.setString("role", "assistant");
    (void)root.setString("model", "claude-haiku-4-5");
    (void)root.set("content", content);
    (void)root.setString("stop_reason", "end_turn");
    (void)root.set("usage", usage);
    return root.toString();
}

[[nodiscard]] arkheon::aicommander::LlmRequest requestFor(const std::string& entityId) {
    arkheon::aicommander::LlmRequest request;
    request.prompt = "STABLE PREFIX BYTES\nSITUATION:\n{}\n";
    request.prefixLength = std::string("STABLE PREFIX BYTES\n").size();
    request.snapshot.entityId = entityId;
    return request;
}

// Walks a schema document and reports whether `keyword` appears at any depth.
[[nodiscard]] bool containsKeyword(const JsonValue& node, const std::string& keyword) {
    if (node.isArray()) {
        for (std::size_t i = 0; i < node.size(); ++i) {
            if (containsKeyword(node.at(i), keyword)) {
                return true;
            }
        }
        return false;
    }
    if (!node.isObject()) {
        return false;
    }
    if (node.has(keyword)) {
        return true;
    }
    for (const std::string& key : node.keys()) {
        if (containsKeyword(node.get(key), keyword)) {
            return true;
        }
    }
    return false;
}

} // namespace

// -- TS-41: the projection carries no keyword the hosted path rejects ----------------------------
//
// PRD v1.8 Â§Corrections item 19. This is the test that keeps the projection honest without a
// network: if someone adds a bounded field to the canonical document, this fails on the next build
// rather than at the first live request.
AIC_TEST(ProjectionContainsNoUnsupportedKeyword) {
    const JsonValue& projected = orderJsonSchemaForStructuredOutputs();
    for (const char* keyword : {"minimum", "maximum", "multipleOf", "minLength", "maxLength"}) {
        AIC_EXPECT_TRUE(!containsKeyword(projected, keyword),
            "projection still carries the unsupported keyword '" << keyword << "'");
    }
    // `$schema` is dropped too: a draft-07 declaration on a document that is no longer
    // draft-07-complete would be a claim the projection cannot honour.
    AIC_EXPECT_TRUE(!projected.has("$schema"), "projection should not declare $schema");
    return true;
}

// -- TS-42: every PIN survives as a const --------------------------------------------------------
//
// The pins are the mechanism, not decoration. Dropping the bounds without replacing them would give
// back the 10/12 shape rejections Â§Corrections item 13 measured, and nothing else in the suite
// would notice â€” the document would still validate every legal order.
AIC_TEST(ProjectionPreservesEveryPinAsConst) {
    const JsonValue& projected = orderJsonSchemaForStructuredOutputs();
    const JsonValue branches = projected.get("anyOf");
    AIC_EXPECT_TRUE(branches.isArray() && branches.size() == 4,
        "projection should carry four anyOf branches, saw " << branches.size());

    std::size_t targetPins = 0;
    std::size_t orbitPins = 0;
    for (std::size_t i = 0; i < branches.size(); ++i) {
        const JsonValue properties = branches.at(i).get("properties");

        // schemaVersion was minimum==maximum==1 in the canonical document.
        const JsonValue version = properties.get("schemaVersion");
        AIC_EXPECT_TRUE(version.has("const") && version.get("const").asInt64() == 1,
            "branch " << i << ": schemaVersion pin did not survive as const");

        const JsonValue target = properties.get("targetEntityId");
        if (target.has("const")) {
            AIC_EXPECT_TRUE(target.get("const").asString().empty(),
                "branch " << i << ": targetEntityId const should be the empty string");
            ++targetPins;
        }
        const JsonValue orbit = properties.get("orbitRadiusM");
        if (orbit.has("const")) {
            AIC_EXPECT_TRUE(orbit.get("const").asDouble() == 0.0,
                "branch " << i << ": orbitRadiusM const should be 0");
            ++orbitPins;
        }
    }
    // transit, hold, defend forbid a target; transit, targeted, defend forbid an orbit.
    AIC_EXPECT_TRUE(targetPins == 3, "expected 3 empty-target pins, saw " << targetPins);
    AIC_EXPECT_TRUE(orbitPins == 3, "expected 3 zero-orbit pins, saw " << orbitPins);
    return true;
}

// -- TS-43: the projection is a projection, not a different schema -------------------------------
//
// The canonical document keeps its bounds. If this ever fails, the projection has mutated its
// source â€” which would make "one definition" false in the direction that is hardest to notice.
AIC_TEST(ProjectionDoesNotMutateTheCanonicalDocument) {
    (void)orderJsonSchemaForStructuredOutputs();
    const JsonValue& canonical = orderJsonSchema();
    AIC_EXPECT_TRUE(canonical.has("oneOf"), "canonical document lost its oneOf");
    AIC_EXPECT_TRUE(containsKeyword(canonical, "minimum"),
        "canonical document lost its numeric bounds - the projection mutated its source");
    AIC_EXPECT_TRUE(containsKeyword(canonical, "maxLength"),
        "canonical document lost its string bounds - the projection mutated its source");
    return true;
}

// -- TS-29: stop_reason 'refusal' produces RejectReason::Refusal ---------------------------------
//
// The reason existed with no producer from v1.2 until v1.8. This is the producer.
AIC_TEST(ClaudeRefusalProducesRefusalReason) {
    JsonValue details = JsonValue::object();
    (void)details.setString("type", "refusal");
    (void)details.setString("category", "cyber");
    JsonValue root = JsonValue::object();
    (void)root.setString("type", "message");
    (void)root.set("content", JsonValue::array());
    (void)root.setString("stop_reason", "refusal");
    (void)root.set("stop_details", details);

    const auto outcome =
        validateStageA(root.toString(), "RedSu35_01", EnvelopeFormat::ClaudeMessages);
    AIC_EXPECT_TRUE(!outcome.accepted, "a refusal must not be accepted");
    AIC_EXPECT_TRUE(outcome.reason == RejectReason::Refusal,
        "expected reject.refusal, got " << toString(outcome.reason));
    return true;
}

// -- TS-30: a refusal with a NULL stop_details is still a refusal --------------------------------
//
// This is the one that would break if the guard were written against stop_details. It would fall
// through, read `content`, and mistake an empty or partial answer for a whole one â€” presenting as
// an occasional truncated order rather than as a failure, which is the worst way for it to present.
AIC_TEST(ClaudeRefusalWithNullDetailsIsStillARefusal) {
    JsonValue root = JsonValue::object();
    (void)root.setString("type", "message");
    (void)root.set("content", JsonValue::array());
    (void)root.setString("stop_reason", "refusal");
    // stop_details deliberately absent.

    const auto outcome =
        validateStageA(root.toString(), "RedSu35_01", EnvelopeFormat::ClaudeMessages);
    AIC_EXPECT_TRUE(outcome.reason == RejectReason::Refusal,
        "a refusal with no stop_details must still reject `refusal`, got "
            << toString(outcome.reason));
    return true;
}

// -- TS-39/TS-40: envelope unwrap ----------------------------------------------------------------

AIC_TEST(ClaudeEnvelopeWithNoTextBlockRejectsEnvelopeNotParse) {
    JsonValue root = JsonValue::object();
    (void)root.setString("type", "message");
    (void)root.set("content", JsonValue::array());
    (void)root.setString("stop_reason", "end_turn");

    const auto outcome =
        validateStageA(root.toString(), "RedSu35_01", EnvelopeFormat::ClaudeMessages);
    AIC_EXPECT_TRUE(outcome.reason == RejectReason::Envelope,
        "a well-formed envelope carrying no order is `envelope`, not `parse`; got "
            << toString(outcome.reason));
    return true;
}

AIC_TEST(ClaudeEnvelopeUnwrapsAWellFormedOrder) {
    const auto outcome = validateStageA(
        claudeEnvelope(orderText(Posture::Hold)), "RedSu35_01", EnvelopeFormat::ClaudeMessages);
    AIC_EXPECT_TRUE(outcome.accepted,
        "a well-formed hold order should survive Stage A, got " << toString(outcome.reason));
    return true;
}

// -- TS-37: the key env var is absent ------------------------------------------------------------
//
// Named as a configuration error rather than left to surface as a 401 the operator has to decode.
AIC_TEST(ClaudeMissingApiKeyIsANamedConfigurationError) {
    setKey("");
    ClaudeLlmClient client(testConfig());
    FakeHttpClient* fake = attachFake(client, std::make_unique<FakeHttpClient>());

    const LlmResult result = client.request(requestFor("RedSu35_01"));
    AIC_EXPECT_TRUE(!result.completed, "no request should complete without a key");
    AIC_EXPECT_TRUE(result.transportDetail.find(kKeyEnvVar) != std::string::npos,
        "the detail should name the env var: " << result.transportDetail);
    AIC_EXPECT_TRUE(fake->requests.empty(),
        "no HTTP request should be attempted without a key, saw " << fake->requests.size());
    return true;
}

// -- TS-38: the key reaches the header and NOTHING else ------------------------------------------
//
// ADR-5's redaction is structural â€” the key is a local, never a member â€” and this is the assertion
// that the structure holds in practice.
AIC_TEST(ClaudeApiKeyAppearsOnlyInTheAuthHeader) {
    setKey(kSentinelKey);
    ClaudeLlmClient client(testConfig());
    auto fake = std::make_unique<FakeHttpClient>();
    fake->replies = {{true, 200, claudeEnvelope(orderText(Posture::Defend))}};
    FakeHttpClient* borrowed = attachFake(client, std::move(fake));

    const LlmResult result = client.request(requestFor("RedSu35_01"));

    AIC_EXPECT_TRUE(borrowed->headerValue(0, "x-api-key") == kSentinelKey,
        "the key must be sent on x-api-key");
    AIC_EXPECT_TRUE(borrowed->requests[0].body.find(kSentinelKey) == std::string::npos,
        "the key must not appear in the request body");
    AIC_EXPECT_TRUE(borrowed->requests[0].bearerToken.empty(),
        "Anthropic authenticates on x-api-key; bearerToken must be left empty");
    AIC_EXPECT_TRUE(result.transportDetail.find(kSentinelKey) == std::string::npos,
        "the key must not appear in any transport detail");
    AIC_EXPECT_TRUE(result.body.find(kSentinelKey) == std::string::npos,
        "the key must not appear in the recorded response body");
    setKey("");
    return true;
}

// -- TS-45: the cache breakpoint lands on the prefix/suffix boundary -----------------------------
//
// A breakpoint at the end of the whole prompt would write a fresh cache entry every request and
// read none â€” paying the write premium to receive nothing, with no counter moving to say so.
AIC_TEST(ClaudeCacheBreakpointSitsAtThePrefixBoundary) {
    setKey(kSentinelKey);
    ClaudeLlmClient client(testConfig());
    auto fake = std::make_unique<FakeHttpClient>();
    fake->replies = {{true, 200, claudeEnvelope(orderText(Posture::Defend), 4500)}};
    FakeHttpClient* borrowed = attachFake(client, std::move(fake));

    const auto request = requestFor("RedSu35_01");
    (void)client.request(request);

    const std::optional<JsonValue> body = JsonValue::parse(borrowed->requests[0].body);
    AIC_EXPECT_TRUE(body.has_value(), "request body should be JSON");
    const JsonValue blocks = body->get("messages").at(0).get("content");
    AIC_EXPECT_TRUE(blocks.size() == 2,
        "prompt should be split into a cached prefix block and an uncached suffix, saw "
            << blocks.size());
    AIC_EXPECT_TRUE(blocks.at(0).get("text").asString() == request.prompt.substr(0, request.prefixLength),
        "the first block must be exactly the declared prefix");
    AIC_EXPECT_TRUE(blocks.at(0).get("cache_control").get("type").asString() == "ephemeral",
        "the breakpoint belongs on the prefix block");
    AIC_EXPECT_TRUE(!blocks.at(1).has("cache_control"),
        "the volatile suffix must NOT carry a breakpoint");

    AIC_EXPECT_TRUE(client.lastCacheReadTokens() == 4500,
        "cache_read_input_tokens should be recorded; it is the only way to tell a hit from a miss");
    setKey("");
    return true;
}

// -- AIC-BE-2's recording SHALL, unmet from v1.8 to v1.8.17 (PRD C9 + C4) ------------------------
//
// The field was parsed into a private member and stopped there: LlmResult carried no cache field,
// so it never crossed runWorkerCall's boundary, never reached OrderRecorder, and never appeared in
// an order log. Every cached-token figure in the PRD came from tests/live and none from the
// product. This asserts the value is on the RESULT - the thing that actually crosses.
AIC_TEST(ClaudeCacheTokensCrossOnTheResultNotOnlyOnTheAdapter) {
    setKey(kSentinelKey);
    ClaudeLlmClient client(testConfig());
    auto fake = std::make_unique<FakeHttpClient>();
    // A steady-state hit: 5,118 read, nothing written. C3's arm B, per hit.
    fake->replies.push_back({true, 200, claudeEnvelope(orderText(Posture::Ingress), 5118, 0)});
    (void)attachFake(client, std::move(fake));

    LlmRequest request;
    request.prompt = "PREFIX-BYTES|SUFFIX";
    request.prefixLength = 13;
    const LlmResult result = client.request(request);

    AIC_EXPECT_EQ(result.cacheReadTokens, 5118,
        "cache_read_input_tokens must be on LlmResult - a value reachable only through the "
        "concrete adapter pointer is a value the worker boundary drops");
    AIC_EXPECT_EQ(result.cacheCreationTokens, 0,
        "a steady-state hit writes nothing");
    AIC_EXPECT_EQ(result.tokensIn, 4500, "the existing token accounting must be undisturbed");
    AIC_EXPECT_EQ(result.tokensOut, 78, "the existing token accounting must be undisturbed");
    setKey("");
    return true;
}

// The cold first request of a run, read off a real response body rather than constructed. This is
// the observation that separates "warming normally" from "never cached", and before v1.8.18 the
// adapter parsed no field that could carry it.
AIC_TEST(ClaudeParsesCacheCreationTokensOnAColdFirstRequest) {
    setKey(kSentinelKey);
    ClaudeLlmClient client(testConfig());
    auto fake = std::make_unique<FakeHttpClient>();
    // Wrote 5,118, read 0 - what the first request of every run reports.
    fake->replies.push_back({true, 200, claudeEnvelope(orderText(Posture::Ingress), 0, 5118)});
    (void)attachFake(client, std::move(fake));

    LlmRequest request;
    request.prompt = "PREFIX-BYTES|SUFFIX";
    request.prefixLength = 13;
    const LlmResult result = client.request(request);

    AIC_EXPECT_EQ(result.cacheCreationTokens, 5118,
        "cache_creation_input_tokens was parsed by NOTHING before v1.8.18 (PRD C4), which is what "
        "made a correct guard impossible: this value is the only thing distinguishing a cold "
        "start from a block that never cached");
    AIC_EXPECT_EQ(result.cacheReadTokens, 0, "a cold request reads nothing");

    // And end to end: the classifier must call this cold, not a shortfall.
    AIC_EXPECT_TRUE(
        classifyCacheState(result.cacheReadTokens, result.cacheCreationTokens, "claude-haiku-4-5")
            == CacheState::ColdWrite,
        "a real cold-start response body must classify as ColdWrite");
    setKey("");
    return true;
}

// A response whose `usage` block carries neither cache field - which is what a non-caching
// deployment returns, and what the shortfall looks like on the wire.
AIC_TEST(ClaudeMissingCacheFieldsReadAsZeroAndClassifyAsShortfall) {
    setKey(kSentinelKey);
    ClaudeLlmClient client(testConfig());
    auto fake = std::make_unique<FakeHttpClient>();

    // Hand-built: a `usage` with the token counts and NO cache keys at all. The parser must treat
    // an absent key as zero rather than leaving the field uninitialised or failing the request -
    // token accounting must never be the thing that fails an otherwise good order.
    JsonValue block = JsonValue::object();
    (void)block.setString("type", "text");
    (void)block.setString("text", orderText(Posture::Ingress));
    JsonValue content = JsonValue::array();
    (void)content.pushBack(block);
    JsonValue usage = JsonValue::object();
    (void)usage.setInt64("input_tokens", 4500);
    (void)usage.setInt64("output_tokens", 78);
    JsonValue root = JsonValue::object();
    (void)root.setString("type", "message");
    (void)root.set("content", content);
    (void)root.setString("stop_reason", "end_turn");
    (void)root.set("usage", usage);
    fake->replies.push_back({true, 200, root.toString()});
    (void)attachFake(client, std::move(fake));

    LlmRequest request;
    request.prompt = "PREFIX-BYTES|SUFFIX";
    request.prefixLength = 13;
    const LlmResult result = client.request(request);

    AIC_EXPECT_EQ(result.cacheReadTokens, 0, "an absent cache key reads as zero");
    AIC_EXPECT_EQ(result.cacheCreationTokens, 0, "an absent cache key reads as zero");
    AIC_EXPECT_EQ(result.tokensIn, 4500, "the rest of usage must still parse");
    AIC_EXPECT_TRUE(
        classifyCacheState(result.cacheReadTokens, result.cacheCreationTokens, "claude-haiku-4-5")
            == CacheState::Shortfall,
        "no cache fields at all is the shortfall - this is precisely the silent failure that "
        "produces no error, no rejection, and a 4.8x bill");
    setKey("");
    return true;
}

// -- TS-31/TS-32/TS-33: the single retry ---------------------------------------------------------

AIC_TEST(ClaudeRetriesOnceOn429ThenSucceeds) {
    setKey(kSentinelKey);
    ClaudeLlmClient client(testConfig());
    auto fake = std::make_unique<FakeHttpClient>();
    fake->replies = {{true, 429, "{}"}, {true, 200, claudeEnvelope(orderText(Posture::Defend))}};
    FakeHttpClient* borrowed = attachFake(client, std::move(fake));

    const LlmResult result = client.request(requestFor("RedSu35_01"));
    AIC_EXPECT_TRUE(borrowed->requests.size() == 2,
        "expected exactly one retry, saw " << borrowed->requests.size() << " requests");
    AIC_EXPECT_TRUE(result.statusCode == 200, "the retry should have succeeded");
    setKey("");
    return true;
}

AIC_TEST(ClaudeRetriesAtMostOnceOn429) {
    setKey(kSentinelKey);
    ClaudeLlmClient client(testConfig());
    auto fake = std::make_unique<FakeHttpClient>();
    fake->replies = {{true, 429, "{}"}};   // repeats
    FakeHttpClient* borrowed = attachFake(client, std::move(fake));

    const LlmResult result = client.request(requestFor("RedSu35_01"));
    AIC_EXPECT_TRUE(borrowed->requests.size() == 2,
        "the retry budget is ONE; saw " << borrowed->requests.size() << " requests");
    AIC_EXPECT_TRUE(result.statusCode == 429, "the second failure is reported, not retried again");
    AIC_EXPECT_TRUE(result.transportDetail.find("retry") != std::string::npos,
        "the detail should say the retry budget was spent: " << result.transportDetail);
    setKey("");
    return true;
}

AIC_TEST(ClaudeRetriesOnceOn500ThenSucceeds) {
    setKey(kSentinelKey);
    ClaudeLlmClient client(testConfig());
    auto fake = std::make_unique<FakeHttpClient>();
    fake->replies = {{true, 503, "{}"}, {true, 200, claudeEnvelope(orderText(Posture::Defend))}};
    FakeHttpClient* borrowed = attachFake(client, std::move(fake));

    const LlmResult result = client.request(requestFor("RedSu35_01"));
    AIC_EXPECT_TRUE(borrowed->requests.size() == 2, "5xx should be retried exactly once");
    AIC_EXPECT_TRUE(result.statusCode == 200, "the retry should have succeeded");
    setKey("");
    return true;
}

// -- TS-34/TS-35/TS-36: transport failure and the TLS diagnosis ----------------------------------

AIC_TEST(ClaudeTlsUnavailableIsDiagnosedDistinctly) {
    setKey(kSentinelKey);
    ClaudeLlmClient client(testConfig());
    auto fake = std::make_unique<FakeHttpClient>();
    // https produces nothing; the plain-http control DOES answer. That asymmetry is the whole
    // signal â€” it is what isolates a missing TLS backend from a network outage.
    fake->replies = {{false, 0, ""}, {true, 200, ""}};
    FakeHttpClient* borrowed = attachFake(client, std::move(fake));

    const LlmResult result = client.request(requestFor("RedSu35_01"));
    AIC_EXPECT_TRUE(!result.completed, "a nullopt from send() is a transport failure");
    AIC_EXPECT_TRUE(result.statusCode == 0, "statusCode is 0 on our side, never read from a response");
    AIC_EXPECT_TRUE(result.transportDetail.find("TLS") != std::string::npos,
        "expected a TLS diagnosis, got: " << result.transportDetail);

    // The control request carries nothing. This is the assertion that keeps AIC-BE-4 from becoming
    // a plaintext egress channel.
    AIC_EXPECT_TRUE(borrowed->requests.size() == 2, "expected the https attempt plus one control");
    AIC_EXPECT_TRUE(borrowed->requests[1].body.empty(),
        "the plain-http control request must carry NO body");
    AIC_EXPECT_TRUE(borrowed->headerValue(1, "x-api-key").empty(),
        "the plain-http control request must carry NO key");
    AIC_EXPECT_TRUE(borrowed->requests[1].url.rfind("http://", 0) == 0,
        "the control request must be plain http, got " << borrowed->requests[1].url);
    setKey("");
    return true;
}

AIC_TEST(ClaudeNetworkOutageIsNotReportedAsATlsProblem) {
    setKey(kSentinelKey);
    ClaudeLlmClient client(testConfig());
    auto fake = std::make_unique<FakeHttpClient>();
    fake->replies = {{false, 0, ""}};   // repeats: neither https nor the control answers
    attachFake(client, std::move(fake));

    const LlmResult result = client.request(requestFor("RedSu35_01"));
    AIC_EXPECT_TRUE(!result.completed, "still a transport failure");
    AIC_EXPECT_TRUE(result.transportDetail.find("TLS") == std::string::npos,
        "an outage must NOT be diagnosed as missing TLS - that sends an operator to reinstall a "
        "runtime that was never the problem: " << result.transportDetail);
    setKey("");
    return true;
}

// -- effort suppression on Haiku -----------------------------------------------------------------
//
// Not a tidy-up: Haiku 4.5 REJECTS `effort`, so forwarding a configured value turns every request
// into a 400 that presents as a total backend outage rather than as a configuration problem.
AIC_TEST(ClaudeSuppressesEffortOnHaiku) {
    setKey(kSentinelKey);
    ClaudeClientConfig config = testConfig();
    config.model = "claude-haiku-4-5";
    config.effort = "high";
    ClaudeLlmClient client(config);
    auto fake = std::make_unique<FakeHttpClient>();
    fake->replies = {{true, 200, claudeEnvelope(orderText(Posture::Defend))}};
    FakeHttpClient* borrowed = attachFake(client, std::move(fake));

    (void)client.request(requestFor("RedSu35_01"));
    const std::optional<JsonValue> body = JsonValue::parse(borrowed->requests[0].body);
    AIC_EXPECT_TRUE(body.has_value(), "request body should be JSON");
    AIC_EXPECT_TRUE(!body->get("output_config").has("effort"),
        "effort must be suppressed on Haiku 4.5, which rejects it");
    setKey("");
    return true;
}

AIC_TEST(ClaudeSendsTheProjectionNotTheCanonicalSchema) {
    setKey(kSentinelKey);
    ClaudeLlmClient client(testConfig());
    auto fake = std::make_unique<FakeHttpClient>();
    fake->replies = {{true, 200, claudeEnvelope(orderText(Posture::Defend))}};
    FakeHttpClient* borrowed = attachFake(client, std::move(fake));

    (void)client.request(requestFor("RedSu35_01"));
    const std::optional<JsonValue> body = JsonValue::parse(borrowed->requests[0].body);
    AIC_EXPECT_TRUE(body.has_value(), "request body should be JSON");
    const JsonValue schema = body->get("output_config").get("format").get("schema");
    AIC_EXPECT_TRUE(body->get("output_config").get("format").get("type").asString() == "json_schema",
        "structured output must be requested as json_schema");
    AIC_EXPECT_TRUE(!containsKeyword(schema, "minimum"),
        "the schema on the wire must be the projection, not the canonical document");
    AIC_EXPECT_TRUE(schema.has("anyOf"), "the projection uses anyOf");
    setKey("");
    return true;
}
