#include "TestSupport.h"

#include "EnvelopeFormat.h"
#include "LocalLlmClient.h"
#include "Order.h"
#include "OrderSchema.h"
#include "OrderValidatorStageA.h"
#include "RejectReason.h"

#include <core/json/JsonValue.h>
#include <core/net/IHttpClient.h>

#include <memory>
#include <string>
#include <vector>

using arkheon::aicommander::EnvelopeFormat;
using arkheon::aicommander::LocalClientConfig;
using arkheon::aicommander::LocalLlmClient;
using arkheon::aicommander::Posture;
using arkheon::aicommander::RejectReason;
using arkheon::aicommander::orderJsonSchema;
using arkheon::aicommander::orderJsonSchemaText;
using arkheon::aicommander::orderSchemaBranch;
using arkheon::aicommander::toString;
using arkheon::aicommander::validateStageA;
using n8ro::core::HttpRequest;
using n8ro::core::HttpResponse;
using n8ro::core::JsonValue;

namespace {

// A transport that answers from a script rather than from a socket, so the whole adapter runs in
// the unit suite with no server and no network — which the PRD's CI requirement is explicit about.
class FakeHttpClient final : public n8ro::core::IHttpClient {
public:
    struct Reply {
        bool completed = true;   // false == std::nullopt from send(), i.e. a transport failure
        int statusCode = 200;
        std::string body;
    };

    // Recorded so a test can assert what the adapter actually put on the wire.
    std::vector<HttpRequest> requests;

    // Replies are consumed in order; the last one repeats once the script runs out.
    std::vector<Reply> replies;

    std::optional<HttpResponse> send(const HttpRequest& request) override {
        requests.push_back(request);
        const Reply reply = replies.empty()
            ? Reply{}
            : replies[requests.size() - 1 < replies.size() ? requests.size() - 1 : replies.size() - 1];
        if (!reply.completed) {
            return std::nullopt;
        }
        HttpResponse response;
        response.statusCode = reply.statusCode;
        response.body = reply.body;
        return response;
    }

    [[nodiscard]] std::size_t countRequestsTo(const std::string& needle) const {
        std::size_t count = 0;
        for (const HttpRequest& request : requests) {
            if (request.url.find(needle) != std::string::npos) {
                ++count;
            }
        }
        return count;
    }
};

// The /api/tags body Ollama returns, carrying whatever tags the caller names.
[[nodiscard]] std::string tagsBody(const std::vector<const char*>& names) {
    JsonValue models = JsonValue::array();
    for (const char* name : names) {
        JsonValue entry = JsonValue::object();
        (void)entry.setString("name", name);
        (void)models.pushBack(entry);
    }
    JsonValue root = JsonValue::object();
    (void)root.set("models", models);
    return root.toString();
}

// A well-formed order document for the given posture, as TEXT.
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

// That order wrapped the way Ollama's POST /api/generate wraps it: as a JSON STRING in `response`.
[[nodiscard]] std::string ollamaEnvelope(const std::string& inner) {
    JsonValue root = JsonValue::object();
    (void)root.setString("model", "qwen2.5:7b-instruct-q8_0");
    (void)root.setBool("done", true);
    (void)root.setString("done_reason", "stop");
    (void)root.setInt64("prompt_eval_count", 1200);
    (void)root.setInt64("eval_count", 78);
    (void)root.setString("response", inner);
    return root.toString();
}

[[nodiscard]] LocalClientConfig testConfig() {
    LocalClientConfig config;
    config.baseUrl = "http://localhost:11434";
    config.model = "qwen2.5:7b-instruct-q8_0";
    config.temperature = 0.0;
    config.grammarEnabled = true;
    config.timeoutS = 90;
    return config;
}

// Wires a fake transport into an adapter and hands back a borrowed pointer to it.
FakeHttpClient* attachFake(LocalLlmClient& client, std::unique_ptr<FakeHttpClient> fake) {
    FakeHttpClient* borrowed = fake.get();
    // The factory hands the adapter ownership exactly once; the adapter creates one client and
    // keeps it, which is the property TS-16 checks.
    auto shared = std::make_shared<std::unique_ptr<FakeHttpClient>>(std::move(fake));
    client.setHttpClientFactory([shared]() -> std::unique_ptr<n8ro::core::IHttpClient> {
        return std::unique_ptr<n8ro::core::IHttpClient>(shared->release());
    });
    return borrowed;
}

[[nodiscard]] arkheon::aicommander::LlmRequest requestFor(const std::string& entityId) {
    arkheon::aicommander::LlmRequest request;
    request.prompt = "PREFIX\nSITUATION:\n{}\n\nYour order:\n";
    request.snapshot.entityId = entityId;
    return request;
}

} // namespace

// -- TS-1: the embedded schema is oneOf over two posture-discriminated branches -------------------
//
// This is the test that keeps PRD §Corrections item 13 from silently regressing. A flattened schema
// still validates every legal order, so nothing else in the suite would notice it — the only
// symptom would be a `reject.shape` rate that nobody could explain.
AIC_TEST(SchemaBranchesEncodeExactlyTheA6Rules) {
    const JsonValue& schema = orderJsonSchema();
    AIC_EXPECT_TRUE(schema.has("oneOf"), "embedded schema has no oneOf");
    const JsonValue branches = schema.get("oneOf");
    AIC_EXPECT_TRUE(branches.isArray() && branches.size() >= 2, "oneOf must carry branches");

    std::size_t posturesSeen = 0;
    for (std::size_t i = 0; i < branches.size(); ++i) {
        const JsonValue branch = branches.at(i);
        const JsonValue properties = branch.get("properties");

        AIC_EXPECT_TRUE(branch.get("additionalProperties").isBool()
                            && !branch.get("additionalProperties").asBool(),
            "branch " << i << " must set additionalProperties:false");

        const JsonValue required = branch.get("required");
        AIC_EXPECT_TRUE(required.isArray(), "branch " << i << " has no required array");
        bool requiresTarget = false;
        bool requiresOrbit = false;
        bool requiresWaypoint = false;
        for (std::size_t r = 0; r < required.size(); ++r) {
            const std::string name = required.at(r).asString();
            requiresTarget = requiresTarget || name == "targetEntityId";
            requiresOrbit = requiresOrbit || name == "orbitRadiusM";
            requiresWaypoint = requiresWaypoint || name == "waypoint";
        }
        // A constrained decoder emits what `required` names and nothing else. These two are the
        // fields the model omitted entirely before v1.7.
        AIC_EXPECT_TRUE(requiresTarget, "branch " << i << " must require targetEntityId");
        AIC_EXPECT_TRUE(requiresOrbit, "branch " << i << " must require orbitRadiusM");

        // Every posture in a branch must agree with that branch on every rule. A branch whose
        // postures disagree is a branch that cannot state the rule unconditionally — which is
        // exactly the bug an earlier two-branch split had, grouping `defend` with `engage`.
        const JsonValue postures = properties.get("posture").get("enum");
        AIC_EXPECT_TRUE(postures.size() >= 1, "branch " << i << " declares no posture");
        posturesSeen += postures.size();

        const bool declaresWaypoint = properties.has("waypoint");
        const std::int64_t targetMinLength = properties.get("targetEntityId").get("minLength").asInt64();
        const double orbitMin = properties.get("orbitRadiusM").get("minimum").asDouble();
        const double orbitMax = properties.get("orbitRadiusM").get("maximum").asDouble();

        for (std::size_t p = 0; p < postures.size(); ++p) {
            Posture posture{};
            const std::string name = postures.at(p).asString();
            AIC_EXPECT_TRUE(arkheon::aicommander::tryParsePosture(name, posture),
                "schema posture '" << name << "' has no C++ counterpart");

            const bool needsWaypoint = arkheon::aicommander::postureRequiresWaypoint(posture);
            AIC_EXPECT_TRUE(needsWaypoint == declaresWaypoint,
                "posture '" << name << "' is in a branch whose waypoint rule disagrees with A6");
            AIC_EXPECT_TRUE(needsWaypoint == requiresWaypoint,
                "posture '" << name << "' is in a branch whose required-waypoint disagrees with A6");
            // Not merely optional where forbidden — absent, so additionalProperties:false makes it
            // a prohibition rather than a silence.

            const bool needsTarget = arkheon::aicommander::postureRequiresTarget(posture);
            AIC_EXPECT_TRUE(needsTarget == (targetMinLength >= 1),
                "posture '" << name << "' is in a branch whose targetEntityId rule disagrees with A6"
                            << " (minLength=" << targetMinLength << ")");
            if (!needsTarget) {
                AIC_EXPECT_EQ(properties.get("targetEntityId").get("maxLength").asInt64(),
                    static_cast<std::int64_t>(0),
                    "posture '" << name << "' must be bounded to an EMPTY targetEntityId");
            }

            if (posture == Posture::Hold) {
                AIC_EXPECT_TRUE(orbitMin > 0.0,
                    "hold's branch must bound orbitRadiusM above zero, got minimum " << orbitMin);
            } else {
                AIC_EXPECT_EQ(orbitMax, 0.0,
                    "posture '" << name << "' must be bounded to orbitRadiusM == 0");
            }
        }
    }

    AIC_EXPECT_EQ(posturesSeen, static_cast<std::size_t>(6),
        "the branches together must cover all six postures exactly once");
    return true;
}

AIC_TEST(SchemaBranchesCoverEveryPostureExactlyOnce) {
    for (const Posture posture : {Posture::Ingress, Posture::Engage, Posture::Crank,
                                  Posture::Defend, Posture::Hold, Posture::Rtb}) {
        const JsonValue enumeration = orderSchemaBranch(posture).get("properties").get("posture").get("enum");
        bool found = false;
        for (std::size_t i = 0; i < enumeration.size(); ++i) {
            found = found || enumeration.at(i).asString() == toString(posture);
        }
        AIC_EXPECT_TRUE(found,
            "orderSchemaBranch(" << toString(posture) << ") does not accept that posture");
    }
    return true;
}

// -- TS-2: a legal order for each posture validates against its own branch -----------------------
AIC_TEST(EveryPostureValidatesAgainstItsBranch) {
    for (const Posture posture : {Posture::Ingress, Posture::Engage, Posture::Crank,
                                  Posture::Defend, Posture::Hold, Posture::Rtb}) {
        const std::optional<JsonValue> parsed = JsonValue::parse(orderText(posture));
        AIC_EXPECT_TRUE(parsed.has_value(), "could not parse the generated order");
        std::string error;
        AIC_EXPECT_TRUE(parsed->validateAgainstSchema(orderSchemaBranch(posture), &error),
            "posture " << toString(posture) << " failed its own branch: " << error);
    }
    return true;
}

// -- TS-3: does the SDK validator handle the full oneOf document? --------------------------------
//
// Recorded rather than assumed. Stage-A check A4 validates against the selected BRANCH precisely so
// it does not depend on the answer; this test exists so the answer is written down somewhere, and
// it passes either way.
AIC_TEST(FullOneOfDocumentValidationIsRecorded) {
    const std::optional<JsonValue> parsed = JsonValue::parse(orderText(Posture::Engage));
    AIC_EXPECT_TRUE(parsed.has_value(), "could not parse the generated order");
    std::string error;
    const bool supported = parsed->validateAgainstSchema(orderJsonSchema(), &error);
    // Both outcomes are legal. What is NOT legal is Stage A depending on it, which is why A4 takes
    // the branch. If this ever fails to compile or throws, that is a different finding entirely.
    (void)supported;
    return true;
}

// -- TS-4..TS-8: A2, the Ollama envelope ---------------------------------------------------------
AIC_TEST(EnvelopeUnwrapsAndTheInnerOrderIsAccepted) {
    const std::string body = ollamaEnvelope(orderText(Posture::Engage));
    const auto outcome = validateStageA(body, "RedSu35_01", EnvelopeFormat::OllamaGenerate);
    AIC_EXPECT_TRUE(outcome.accepted, "envelope-wrapped order was rejected: " << outcome.detail);
    AIC_EXPECT_EQ(std::string(toString(outcome.order.posture)), std::string("engage"), "posture");
    AIC_EXPECT_EQ(outcome.order.targetEntityId, std::string("BlueF16_01"), "target");
    return true;
}

AIC_TEST(EnvelopeThatIsNotAnObjectRejectsEnvelope) {
    for (const char* body : {"not json at all", "[1,2,3]", "\"a bare string\"", "42"}) {
        const auto outcome = validateStageA(body, "RedSu35_01", EnvelopeFormat::OllamaGenerate);
        AIC_EXPECT_FALSE(outcome.accepted, "body '" << body << "' should not be accepted");
        AIC_EXPECT_EQ(std::string(toString(outcome.reason)), std::string("envelope"),
            "body '" << body << "' should reject 'envelope', not 'parse'");
    }
    return true;
}

AIC_TEST(EnvelopeWithoutAResponseStringRejectsEnvelope) {
    // No `response` at all.
    {
        JsonValue root = JsonValue::object();
        (void)root.setBool("done", true);
        const auto outcome = validateStageA(root.toString(), "RedSu35_01", EnvelopeFormat::OllamaGenerate);
        AIC_EXPECT_FALSE(outcome.accepted, "an envelope with no 'response' must be rejected");
        AIC_EXPECT_EQ(std::string(toString(outcome.reason)), std::string("envelope"), "reason");
    }
    // `response` present but not a string — an object, which is what a caller who assumed the
    // order came back parsed would produce.
    {
        JsonValue root = JsonValue::object();
        (void)root.set("response", JsonValue::object());
        const auto outcome = validateStageA(root.toString(), "RedSu35_01", EnvelopeFormat::OllamaGenerate);
        AIC_EXPECT_FALSE(outcome.accepted, "a non-string 'response' must be rejected");
        AIC_EXPECT_EQ(std::string(toString(outcome.reason)), std::string("envelope"), "reason");
    }
    return true;
}

AIC_TEST(EnvelopeCarryingAnErrorFieldRejectsEnvelopeAndQuotesIt) {
    JsonValue root = JsonValue::object();
    (void)root.setString("error", "model 'qwen2.5:7b' not found");
    const auto outcome = validateStageA(root.toString(), "RedSu35_01", EnvelopeFormat::OllamaGenerate);
    AIC_EXPECT_FALSE(outcome.accepted, "a server-reported error must be rejected");
    AIC_EXPECT_EQ(std::string(toString(outcome.reason)), std::string("envelope"), "reason");
    AIC_EXPECT_TRUE(outcome.detail.find("not found") != std::string::npos,
        "the server's own message must survive into the detail, got: " << outcome.detail);
    return true;
}

AIC_TEST(EnvelopeWithAnEmptyResponseRejectsEnvelopeNotParse) {
    JsonValue root = JsonValue::object();
    (void)root.setString("response", "");
    const auto outcome = validateStageA(root.toString(), "RedSu35_01", EnvelopeFormat::OllamaGenerate);
    AIC_EXPECT_FALSE(outcome.accepted, "an empty 'response' must be rejected");
    // The backend delivered no order. Calling that `parse` would point the runbook at the model.
    AIC_EXPECT_EQ(std::string(toString(outcome.reason)), std::string("envelope"), "reason");
    return true;
}

// -- An order that OMITS the conditional fields is still the same order ---------------------------
//
// The branch schema requires targetEntityId and orbitRadiusM so a constrained decoder emits them.
// What Stage A accepts is a different question, governed by AIC-ORD-1's field table, which gives
// both a value in the inapplicable case. Holding acceptance to the emission rule would reject every
// `stub` order and every order in a log recorded by an earlier build - breaking AIC-DET-2's replay
// guarantee silently. This asserts the asymmetry is deliberate and stays.
AIC_TEST(OmittedConditionalFieldsAreStillAcceptedByStageA) {
    JsonValue order = JsonValue::object();
    (void)order.setInt64("schemaVersion", 1);
    (void)order.setString("entityId", "RedSu35_01");
    (void)order.setString("posture", "ingress");
    (void)order.setDouble("cruiseSpeedMps", 250.0);
    (void)order.setString("roe", "weaponsTight");
    (void)order.setString("reason", "Pre-v1.7 shape: no targetEntityId, no orbitRadiusM.");
    JsonValue waypoint = JsonValue::object();
    (void)waypoint.setDouble("latitudeDeg", 13.5);
    (void)waypoint.setDouble("longitudeDeg", 144.8);
    (void)waypoint.setDouble("altitudeHaeM", 10000.0);
    (void)order.set("waypoint", waypoint);

    const auto outcome = validateStageA(order.toString(), "RedSu35_01");
    AIC_EXPECT_TRUE(outcome.accepted,
        "an order omitting the inapplicable conditional fields must still be accepted: "
            << outcome.detail);
    AIC_EXPECT_TRUE(outcome.order.targetEntityId.empty(), "omitted target reads as empty");
    AIC_EXPECT_EQ(outcome.order.orbitRadiusM, 0.0, "omitted orbit radius reads as zero");
    return true;
}

// -- TS-9: Raw is unchanged ----------------------------------------------------------------------
AIC_TEST(RawEnvelopeStillTreatsTheBodyAsTheOrder) {
    const std::string body = orderText(Posture::Hold);
    const auto viaDefault = validateStageA(body, "RedSu35_01");
    const auto viaExplicit = validateStageA(body, "RedSu35_01", EnvelopeFormat::Raw);
    AIC_EXPECT_TRUE(viaDefault.accepted, "raw order rejected via the default: " << viaDefault.detail);
    AIC_EXPECT_TRUE(viaExplicit.accepted, "raw order rejected explicitly: " << viaExplicit.detail);

    // And the Ollama unwrap must NOT accept a bare order — otherwise the two formats would be
    // interchangeable and the parameter would be decoration.
    const auto viaOllama = validateStageA(body, "RedSu35_01", EnvelopeFormat::OllamaGenerate);
    AIC_EXPECT_FALSE(viaOllama.accepted, "a bare order is not an Ollama envelope");
    AIC_EXPECT_EQ(std::string(toString(viaOllama.reason)), std::string("envelope"), "reason");
    return true;
}

// -- TS-10: the request the adapter puts on the wire ---------------------------------------------
AIC_TEST(LocalAdapterBuildsThePinnedRequest) {
    LocalLlmClient client(testConfig());
    auto fake = std::make_unique<FakeHttpClient>();
    fake->replies = {
        {true, 200, tagsBody({"qwen2.5:7b-instruct-q8_0", "llama3.1:8b"})},   // preflight
        {true, 200, ollamaEnvelope(orderText(Posture::Engage))},               // the order
    };
    FakeHttpClient* transport = attachFake(client, std::move(fake));

    const arkheon::aicommander::LlmResult result = client.request(requestFor("RedSu35_01"));
    AIC_EXPECT_TRUE(result.completed, "request should complete: " << result.transportDetail);
    AIC_EXPECT_EQ(result.statusCode, 200, "status");

    AIC_EXPECT_EQ(transport->requests.size(), static_cast<std::size_t>(2),
        "expected one preflight and one generate");
    const HttpRequest& generate = transport->requests[1];
    AIC_EXPECT_TRUE(generate.method == n8ro::core::HttpMethod::Post, "must POST");
    AIC_EXPECT_EQ(generate.url, std::string("http://localhost:11434/api/generate"), "url");
    AIC_EXPECT_EQ(generate.timeoutS, 90, "HttpRequest::timeoutS must carry commander.requestTimeoutS");
    AIC_EXPECT_EQ(generate.contentType, std::string("application/json"), "content type");

    const std::optional<JsonValue> body = JsonValue::parse(generate.body);
    AIC_EXPECT_TRUE(body.has_value(), "request body is not JSON");
    AIC_EXPECT_EQ(body->get("model").asString(), std::string("qwen2.5:7b-instruct-q8_0"), "model tag");
    AIC_EXPECT_TRUE(body->get("prompt").asString().find("Your order:") != std::string::npos,
        "the rendered prompt must be sent verbatim");
    AIC_EXPECT_TRUE(body->has("stream") && !body->get("stream").asBool(), "stream must be false");
    AIC_EXPECT_EQ(body->get("options").get("temperature").asDouble(), 0.0, "temperature");

    // The load-bearing one: the SAME schema object AIC-ORD-1 embeds, not a hand-copied variant.
    AIC_EXPECT_TRUE(body->has("format"), "format must be sent when grammarEnabled");
    AIC_EXPECT_EQ(body->get("format").toString(), orderJsonSchemaText(),
        "format must be the embedded order schema, byte for byte");
    return true;
}

AIC_TEST(LocalAdapterOmitsFormatWhenGrammarIsDisabled) {
    LocalClientConfig config = testConfig();
    config.grammarEnabled = false;
    LocalLlmClient client(config);
    auto fake = std::make_unique<FakeHttpClient>();
    fake->replies = {
        {true, 200, tagsBody({"qwen2.5:7b-instruct-q8_0"})},
        {true, 200, ollamaEnvelope(orderText(Posture::Engage))},
    };
    FakeHttpClient* transport = attachFake(client, std::move(fake));

    (void)client.request(requestFor("RedSu35_01"));
    const std::optional<JsonValue> body = JsonValue::parse(transport->requests[1].body);
    AIC_EXPECT_TRUE(body.has_value(), "request body is not JSON");
    AIC_EXPECT_FALSE(body->has("format"), "format must be absent when grammarEnabled is false");
    return true;
}

// -- TS-11 / TS-12: transport failure vs a returned non-2xx --------------------------------------
AIC_TEST(NulloptFromSendIsATransportFailureNotAStatusCode) {
    LocalLlmClient client(testConfig());
    auto fake = std::make_unique<FakeHttpClient>();
    fake->replies = {
        {true, 200, tagsBody({"qwen2.5:7b-instruct-q8_0"})},
        {false, 0, ""},   // std::nullopt from send()
    };
    (void)attachFake(client, std::move(fake));

    const arkheon::aicommander::LlmResult result = client.request(requestFor("RedSu35_01"));
    AIC_EXPECT_FALSE(result.completed, "a nullopt send() must not report completed");
    AIC_EXPECT_TRUE(!result.transportDetail.empty(), "a transport failure must carry a detail");
    AIC_EXPECT_TRUE(result.body.empty(), "a transport failure has no body");
    return true;
}

AIC_TEST(ReturnedNon2xxKeepsItsStatusAndStaysCompleted) {
    LocalLlmClient client(testConfig());
    auto fake = std::make_unique<FakeHttpClient>();
    fake->replies = {
        {true, 200, tagsBody({"qwen2.5:7b-instruct-q8_0"})},
        {true, 500, "internal error"},
    };
    (void)attachFake(client, std::move(fake));

    const arkheon::aicommander::LlmResult result = client.request(requestFor("RedSu35_01"));
    // completed == true is the whole point: a server that answered is not a server that vanished,
    // and the two need different runbook rows.
    AIC_EXPECT_TRUE(result.completed, "a returned 500 is a completed request");
    AIC_EXPECT_EQ(result.statusCode, 500, "status must survive");
    return true;
}

// -- TS-13 / TS-14 / TS-15: the preflight --------------------------------------------------------
AIC_TEST(PreflightNamesAMissingModelTagAsAConfigurationError) {
    LocalClientConfig config = testConfig();
    config.model = "llama-3.2-3b-instruct-q4_k_m";   // a GGUF FILENAME, the v1.6 mistake
    LocalLlmClient client(config);
    auto fake = std::make_unique<FakeHttpClient>();
    fake->replies = {{true, 200, tagsBody({"qwen2.5:7b-instruct-q8_0", "llama3.1:8b"})}};
    FakeHttpClient* transport = attachFake(client, std::move(fake));

    const arkheon::aicommander::LlmResult result = client.request(requestFor("RedSu35_01"));
    AIC_EXPECT_FALSE(result.completed, "a missing model tag must fail the request");
    AIC_EXPECT_TRUE(result.transportDetail.find("configuration error") != std::string::npos,
        "must read as a configuration error, got: " << result.transportDetail);
    AIC_EXPECT_TRUE(result.transportDetail.find(config.model) != std::string::npos,
        "must name the offending tag, got: " << result.transportDetail);
    AIC_EXPECT_TRUE(result.transportDetail.find("qwen2.5:7b-instruct-q8_0") != std::string::npos,
        "must list what the server does have, got: " << result.transportDetail);
    // And it must never have attempted the generate.
    AIC_EXPECT_EQ(transport->countRequestsTo("/api/generate"), static_cast<std::size_t>(0),
        "no order should be requested from a server that cannot serve the model");
    return true;
}

AIC_TEST(PreflightDistinguishesAnUnreachableServer) {
    LocalLlmClient client(testConfig());
    auto fake = std::make_unique<FakeHttpClient>();
    fake->replies = {{false, 0, ""}};   // /api/tags itself does not complete
    (void)attachFake(client, std::move(fake));

    const arkheon::aicommander::LlmResult result = client.request(requestFor("RedSu35_01"));
    AIC_EXPECT_FALSE(result.completed, "an unreachable server must fail the request");
    AIC_EXPECT_TRUE(result.transportDetail.find("unreachable") != std::string::npos,
        "must read as unreachable, got: " << result.transportDetail);
    // Distinct from the configuration-error text, or the runbook cannot tell them apart.
    AIC_EXPECT_TRUE(result.transportDetail.find("configuration error") == std::string::npos,
        "must not be confused with a configuration error");
    return true;
}

AIC_TEST(PreflightRunsOnceWhenItSucceeds) {
    LocalLlmClient client(testConfig());
    auto fake = std::make_unique<FakeHttpClient>();
    fake->replies = {
        {true, 200, tagsBody({"qwen2.5:7b-instruct-q8_0"})},
        {true, 200, ollamaEnvelope(orderText(Posture::Engage))},
        {true, 200, ollamaEnvelope(orderText(Posture::Crank))},
        {true, 200, ollamaEnvelope(orderText(Posture::Hold))},
    };
    FakeHttpClient* transport = attachFake(client, std::move(fake));

    for (int i = 0; i < 3; ++i) {
        (void)client.request(requestFor("RedSu35_01"));
    }
    AIC_EXPECT_EQ(transport->countRequestsTo("/api/tags"), static_cast<std::size_t>(1),
        "the preflight is a one-shot, not a per-order cost");
    AIC_EXPECT_EQ(transport->countRequestsTo("/api/generate"), static_cast<std::size_t>(3),
        "every order after the preflight goes straight to /api/generate");
    return true;
}

// -- TS-16: no transport is constructed until the first request ----------------------------------
AIC_TEST(NoHttpClientIsConstructedBeforeTheFirstRequest) {
    LocalLlmClient client(testConfig());
    auto calls = std::make_shared<int>(0);
    client.setHttpClientFactory([calls]() -> std::unique_ptr<n8ro::core::IHttpClient> {
        ++*calls;
        auto fake = std::make_unique<FakeHttpClient>();
        fake->replies = {
            {true, 200, tagsBody({"qwen2.5:7b-instruct-q8_0"})},
            {true, 200, ollamaEnvelope(orderText(Posture::Engage))},
        };
        return fake;
    });

    // The constructor runs on the simulation thread; IHttpClient is documented single-thread-only,
    // so nothing may be created until request() runs on the worker.
    AIC_EXPECT_EQ(*calls, 0, "construction must not create a transport");

    (void)client.request(requestFor("RedSu35_01"));
    AIC_EXPECT_EQ(*calls, 1, "the first request creates exactly one transport");
    (void)client.request(requestFor("RedSu35_01"));
    AIC_EXPECT_EQ(*calls, 1, "subsequent requests reuse it - one IHttpClient per worker");
    return true;
}

// -- the adapter reports its envelope, which is how A2 ever runs ----------------------------------
AIC_TEST(LocalAdapterDeclaresTheOllamaEnvelope) {
    LocalLlmClient client(testConfig());
    AIC_EXPECT_TRUE(client.envelopeFormat() == EnvelopeFormat::OllamaGenerate,
        "the local adapter must declare the Ollama envelope, or A2 never unwraps");
    AIC_EXPECT_EQ(std::string(client.backendName()), std::string("local"), "backend name");
    AIC_EXPECT_EQ(client.modelName(), std::string("qwen2.5:7b-instruct-q8_0"), "model name");
    return true;
}
