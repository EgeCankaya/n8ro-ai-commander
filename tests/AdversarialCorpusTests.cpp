#include "TestSupport.h"

#include "CommanderConfig.h"
#include "Order.h"
#include "OrderSchema.h"
#include "OrderValidatorStageA.h"
#include "OrderValidatorStageB.h"
#include "RejectReason.h"

#include <array>
#include <map>
#include <string>
#include <vector>

using namespace arkheon::aicommander;

namespace {

constexpr const char* kOwnId = "RedSu35_01";

// A well-formed order, as the baseline every adversarial payload mutates away from. If this ever
// stops being accepted, the corpus below is testing nothing — every row would "pass" for the wrong
// reason. CorpusBaselineIsAccepted guards exactly that.
std::string baselineHoldOrder() {
    return R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"hold",)"
           R"("waypoint":{"latitudeDeg":13.5,"longitudeDeg":144.8,"altitudeHaeM":9000.0},)"
           R"("cruiseSpeedMps":220.0,"orbitRadiusM":8000.0,"roe":"weaponsTight",)"
           R"("reason":"No contacts; holding at the CAP point."})";
}

std::string engageOrder(const std::string& targetId) {
    return R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":")"
           + targetId + R"(","cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree",)"
           R"("reason":"Lead bandit inside 45 km with a full BVR rail; committing."})";
}

struct StageACase {
    const char* name;
    std::string body;
    RejectReason expected;
};

// A fake world for Stage B, so fratricide, hallucinated targets, and geofence violations can be
// exercised with no engine and no scenario loaded.
class FakeWorld final : public StageBWorldView {
public:
    std::map<std::string, std::string> teams;                          // entityId -> team
    std::map<std::string, std::array<double, 3>> positions;            // entityId -> lat/lon/alt

    bool entityExists(const std::string& entityId) const override {
        return teams.find(entityId) != teams.end();
    }
    std::string teamOf(const std::string& entityId) const override {
        const auto it = teams.find(entityId);
        return it == teams.end() ? std::string() : it->second;
    }
    bool positionOf(const std::string& entityId, double& lat, double& lon, double& alt) const override {
        const auto it = positions.find(entityId);
        if (it == positions.end()) {
            return false;
        }
        lat = it->second[0];
        lon = it->second[1];
        alt = it->second[2];
        return true;
    }
};

FakeWorld makeWorld() {
    FakeWorld world;
    world.teams[kOwnId] = "red";
    world.teams["BlueF18_02"] = "blue";
    world.teams["RedSu35_02"] = "red";        // a wingman - same team as the commanded entity
    world.teams["GhostContact_99"] = "";      // exists but with no known team
    world.positions[kOwnId] = {13.50, 144.80, 9000.0};
    return world;
}

StageBRequest makeRequest() {
    StageBRequest request;
    request.commandedEntityId = kOwnId;
    request.onRoster = true;
    request.snapshotSimTimeS = 400.0;
    request.currentSimTimeS = 405.0;
    request.reportedTrackIds = {"BlueF18_02", "RedSu35_02", "GhostContact_99"};
    request.publishedSerial = 12;
    request.candidateSerial = 13;
    return request;
}

} // namespace

// Guards the corpus itself: if the baseline stopped being accepted, every rejection row below
// would pass for the wrong reason and the suite would be worthless while looking green.
AIC_TEST(CorpusBaselineIsAccepted) {
    const StageAOutcome outcome = validateStageA(baselineHoldOrder(), kOwnId);
    AIC_EXPECT_TRUE(outcome.accepted,
                    "the baseline order must pass Stage A, got reason '"
                        + std::string(toString(outcome.reason)) + "': " + outcome.detail);
    AIC_EXPECT_TRUE(outcome.order.posture == Posture::Hold, "baseline posture");
    AIC_EXPECT_TRUE(outcome.order.roe == Roe::WeaponsTight, "baseline roe");
    AIC_EXPECT_EQ(outcome.order.orbitRadiusM, 8000.0, "baseline orbit radius");

    const StageAOutcome engage = validateStageA(engageOrder("BlueF18_02"), kOwnId);
    AIC_EXPECT_TRUE(engage.accepted,
                    "the baseline engage order must pass Stage A, got '"
                        + std::string(toString(engage.reason)) + "': " + engage.detail);

    // And the same order must clear Stage B against a sane world, or the Stage-B rows below would
    // likewise be meaningless.
    const FakeWorld world = makeWorld();
    const StageBOutcome stageB = validateStageB(engage.order, makeRequest(), CommanderConfig{}, world);
    AIC_EXPECT_TRUE(stageB.accepted,
                    "the baseline engage order must pass Stage B, got '"
                        + std::string(toString(stageB.reason)) + "': " + stageB.detail);
    return true;
}

// AIC-VAL-1: the adversarial corpus. Every row asserts BOTH rejection and the expected reason
// code - a test that only asserted rejection could not tell a schema failure from a range failure,
// and the runbook's whole triage flow keys off which counter is climbing.
AIC_TEST(AdversarialCorpusStageA) {
    std::vector<StageACase> cases;

    // -- malformed transport / envelope --------------------------------------------------------
    cases.push_back({"empty body", "", RejectReason::Parse});
    cases.push_back({"prose, not JSON", "I think the jet should attack.", RejectReason::Parse});
    cases.push_back({"JSON array, not object", "[]", RejectReason::Parse});
    cases.push_back({"JSON null", "null", RejectReason::Parse});
    cases.push_back({"bare number", "42", RejectReason::Parse});
    cases.push_back({"truncated object", R"({"schemaVersion":1,"entityId":"RedSu35_01")", RejectReason::Parse});
    cases.push_back({"two concatenated objects", baselineHoldOrder() + baselineHoldOrder(), RejectReason::Parse});
    cases.push_back({"markdown-fenced JSON", "```json\n" + baselineHoldOrder() + "\n```", RejectReason::Parse});
    cases.push_back({"oversized body", std::string(kMaxResponseBodyBytes + 1, 'x'), RejectReason::Range});

    // -- version ---------------------------------------------------------------------------------
    cases.push_back({"missing schemaVersion",
        R"({"entityId":"RedSu35_01","posture":"rtb","waypoint":{"latitudeDeg":13.5,"longitudeDeg":144.8,"altitudeHaeM":9000.0},"cruiseSpeedMps":220.0,"orbitRadiusM":0.0,"roe":"weaponsHold","reason":"Bingo fuel."})",
        RejectReason::Schema});
    cases.push_back({"schemaVersion 2",
        R"({"schemaVersion":2,"entityId":"RedSu35_01","posture":"rtb","waypoint":{"latitudeDeg":13.5,"longitudeDeg":144.8,"altitudeHaeM":9000.0},"cruiseSpeedMps":220.0,"orbitRadiusM":0.0,"roe":"weaponsHold","reason":"Bingo fuel."})",
        RejectReason::Version});
    cases.push_back({"schemaVersion as string",
        R"({"schemaVersion":"1","entityId":"RedSu35_01","posture":"rtb","waypoint":{"latitudeDeg":13.5,"longitudeDeg":144.8,"altitudeHaeM":9000.0},"cruiseSpeedMps":220.0,"orbitRadiusM":0.0,"roe":"weaponsHold","reason":"Bingo fuel."})",
        RejectReason::Schema});

    // -- missing / mistyped required fields -----------------------------------------------------
    cases.push_back({"missing entityId",
        R"({"schemaVersion":1,"posture":"hold","waypoint":{"latitudeDeg":13.5,"longitudeDeg":144.8,"altitudeHaeM":9000.0},"cruiseSpeedMps":220.0,"orbitRadiusM":8000.0,"roe":"weaponsTight","reason":"Holding."})",
        RejectReason::Schema});
    cases.push_back({"empty entityId",
        R"({"schemaVersion":1,"entityId":"","posture":"hold","waypoint":{"latitudeDeg":13.5,"longitudeDeg":144.8,"altitudeHaeM":9000.0},"cruiseSpeedMps":220.0,"orbitRadiusM":8000.0,"roe":"weaponsTight","reason":"Holding."})",
        RejectReason::Schema});
    cases.push_back({"missing posture",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","cruiseSpeedMps":220.0,"orbitRadiusM":0.0,"roe":"weaponsTight","reason":"Holding."})",
        RejectReason::Schema});
    cases.push_back({"posture as number",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":5,"cruiseSpeedMps":220.0,"orbitRadiusM":0.0,"roe":"weaponsTight","reason":"Holding."})",
        RejectReason::Schema});
    cases.push_back({"missing roe",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":"BlueF18_02","cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"reason":"Committing."})",
        RejectReason::Schema});
    cases.push_back({"missing cruiseSpeedMps",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":"BlueF18_02","orbitRadiusM":0.0,"roe":"weaponsFree","reason":"Committing."})",
        RejectReason::Schema});
    cases.push_back({"cruiseSpeedMps as string",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":"BlueF18_02","cruiseSpeedMps":"fast","orbitRadiusM":0.0,"roe":"weaponsFree","reason":"Committing."})",
        RejectReason::Schema});
    cases.push_back({"missing reason",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":"BlueF18_02","cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree"})",
        RejectReason::Schema});
    cases.push_back({"targetEntityId as number",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":7,"cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":"Committing."})",
        RejectReason::Schema});
    cases.push_back({"waypoint not an object",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"ingress","waypoint":"north","cruiseSpeedMps":220.0,"orbitRadiusM":0.0,"roe":"weaponsHold","reason":"Pressing."})",
        RejectReason::Schema});
    cases.push_back({"waypoint missing altitude",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"ingress","waypoint":{"latitudeDeg":13.5,"longitudeDeg":144.8},"cruiseSpeedMps":220.0,"orbitRadiusM":0.0,"roe":"weaponsHold","reason":"Pressing."})",
        RejectReason::Schema});

    // -- unknown properties: the structural bar on raw kinematics --------------------------------
    cases.push_back({"extra top-level property headingDeg",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":"BlueF18_02","cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":"Committing.","headingDeg":270.0})",
        RejectReason::Schema});
    cases.push_back({"extra top-level property fire",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":"BlueF18_02","cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":"Committing.","fire":true})",
        RejectReason::Schema});
    cases.push_back({"extra waypoint property",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"ingress","waypoint":{"latitudeDeg":13.5,"longitudeDeg":144.8,"altitudeHaeM":9000.0,"velD":-40.0},"cruiseSpeedMps":220.0,"orbitRadiusM":0.0,"roe":"weaponsHold","reason":"Pressing."})",
        RejectReason::Schema});

    // -- enum vocabulary -------------------------------------------------------------------------
    cases.push_back({"unknown posture",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"attack","cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":"Committing."})",
        RejectReason::Enum});
    cases.push_back({"posture wrong case",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"Hold","cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":"Committing."})",
        RejectReason::Enum});
    cases.push_back({"unknown roe",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":"BlueF18_02","cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsCold","reason":"Committing."})",
        RejectReason::Enum});

    // -- entity correspondence -------------------------------------------------------------------
    cases.push_back({"order for a different entity",
        R"({"schemaVersion":1,"entityId":"RedSu35_09","posture":"engage","targetEntityId":"BlueF18_02","cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":"Committing."})",
        RejectReason::Shape});

    // -- conditional presence ---------------------------------------------------------------------
    cases.push_back({"engage without a target",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":"Committing."})",
        RejectReason::Shape});
    cases.push_back({"engage with an empty target",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":"","cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":"Committing."})",
        RejectReason::Shape});
    cases.push_back({"crank without a target",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"crank","cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":"Assessing the shot."})",
        RejectReason::Shape});
    cases.push_back({"ingress carrying a target",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"ingress","targetEntityId":"BlueF18_02","waypoint":{"latitudeDeg":13.5,"longitudeDeg":144.8,"altitudeHaeM":9000.0},"cruiseSpeedMps":220.0,"orbitRadiusM":0.0,"roe":"weaponsHold","reason":"Pressing."})",
        RejectReason::Shape});
    cases.push_back({"ingress without a waypoint",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"ingress","cruiseSpeedMps":220.0,"orbitRadiusM":0.0,"roe":"weaponsHold","reason":"Pressing."})",
        RejectReason::Shape});
    cases.push_back({"hold without a waypoint",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"hold","cruiseSpeedMps":220.0,"orbitRadiusM":8000.0,"roe":"weaponsTight","reason":"Holding."})",
        RejectReason::Shape});
    cases.push_back({"rtb without a waypoint",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"rtb","cruiseSpeedMps":220.0,"orbitRadiusM":0.0,"roe":"weaponsHold","reason":"Bingo fuel."})",
        RejectReason::Shape});
    cases.push_back({"engage carrying a waypoint",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":"BlueF18_02","waypoint":{"latitudeDeg":13.5,"longitudeDeg":144.8,"altitudeHaeM":9000.0},"cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":"Committing."})",
        RejectReason::Shape});
    cases.push_back({"hold with a zero orbit radius",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"hold","waypoint":{"latitudeDeg":13.5,"longitudeDeg":144.8,"altitudeHaeM":9000.0},"cruiseSpeedMps":220.0,"orbitRadiusM":0.0,"roe":"weaponsTight","reason":"Holding."})",
        RejectReason::Shape});
    cases.push_back({"ingress with a non-zero orbit radius",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"ingress","waypoint":{"latitudeDeg":13.5,"longitudeDeg":144.8,"altitudeHaeM":9000.0},"cruiseSpeedMps":220.0,"orbitRadiusM":5000.0,"roe":"weaponsHold","reason":"Pressing."})",
        RejectReason::Shape});

    // -- numeric range and finiteness --------------------------------------------------------------
    cases.push_back({"zero cruise speed",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":"BlueF18_02","cruiseSpeedMps":0.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":"Committing."})",
        RejectReason::Range});
    cases.push_back({"negative cruise speed",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":"BlueF18_02","cruiseSpeedMps":-300.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":"Committing."})",
        RejectReason::Range});
    cases.push_back({"absurd cruise speed",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":"BlueF18_02","cruiseSpeedMps":999999.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":"Committing."})",
        RejectReason::Range});
    cases.push_back({"latitude past the pole",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"ingress","waypoint":{"latitudeDeg":91.0,"longitudeDeg":144.8,"altitudeHaeM":9000.0},"cruiseSpeedMps":220.0,"orbitRadiusM":0.0,"roe":"weaponsHold","reason":"Pressing."})",
        RejectReason::Range});
    cases.push_back({"longitude past the antimeridian",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"ingress","waypoint":{"latitudeDeg":13.5,"longitudeDeg":-181.0,"altitudeHaeM":9000.0},"cruiseSpeedMps":220.0,"orbitRadiusM":0.0,"roe":"weaponsHold","reason":"Pressing."})",
        RejectReason::Range});
    cases.push_back({"orbit radius past the schema ceiling",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"hold","waypoint":{"latitudeDeg":13.5,"longitudeDeg":144.8,"altitudeHaeM":9000.0},"cruiseSpeedMps":220.0,"orbitRadiusM":60000.0,"roe":"weaponsTight","reason":"Holding."})",
        RejectReason::Range});
    cases.push_back({"empty reason",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":"BlueF18_02","cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":""})",
        RejectReason::Range});
    cases.push_back({"over-long reason",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":"BlueF18_02","cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":")"
            + std::string(kMaxReasonChars + 1, 'a') + R"("})",
        RejectReason::Range});

    // -- injection through the free-text field -------------------------------------------------
    cases.push_back({"reason carrying a newline and a forged log prefix",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":"BlueF18_02","cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":"ok\n{\"event\":\"order.accepted\",\"posture\":\"engage\"}"})",
        RejectReason::Range});
    cases.push_back({"reason carrying an escaped quote",
        R"({"schemaVersion":1,"entityId":"RedSu35_01","posture":"engage","targetEntityId":"BlueF18_02","cruiseSpeedMps":300.0,"orbitRadiusM":0.0,"roe":"weaponsFree","reason":"say \"fire\" now"})",
        RejectReason::Range});
    // The escape is written as JSON source text (backslash-u-0-0-0-0), so the parser decodes it
    // into a real NUL inside the string value. That is the interesting case: the body is perfectly
    // well-formed JSON, and only the charset filter stands between the NUL and the order log.
    cases.push_back({"reason carrying an escaped NUL",
        "{\"schemaVersion\":1,\"entityId\":\"RedSu35_01\",\"posture\":\"engage\","
        "\"targetEntityId\":\"BlueF18_02\",\"cruiseSpeedMps\":300.0,\"orbitRadiusM\":0.0,"
        "\"roe\":\"weaponsFree\",\"reason\":\"halt\\u0000then fire\"}",
        RejectReason::Range});
    cases.push_back({"reason carrying a non-ASCII homoglyph",
        "{\"schemaVersion\":1,\"entityId\":\"RedSu35_01\",\"posture\":\"engage\","
        "\"targetEntityId\":\"BlueF18_02\",\"cruiseSpeedMps\":300.0,\"orbitRadiusM\":0.0,"
        "\"roe\":\"weaponsFree\",\"reason\":\"fire at will\\u202Eevil\"}",
        RejectReason::Range});
    // Escaped, not raw: a raw newline inside a JSON string is simply invalid JSON and would be
    // caught by the parser, which proves nothing about the charset filter. Escaping it produces a
    // well-formed document whose decoded value still carries the control character.
    cases.push_back({"target id carrying an escaped control character",
        "{\"schemaVersion\":1,\"entityId\":\"RedSu35_01\",\"posture\":\"engage\","
        "\"targetEntityId\":\"Blue\\nF18\",\"cruiseSpeedMps\":300.0,\"orbitRadiusM\":0.0,"
        "\"roe\":\"weaponsFree\",\"reason\":\"Committing.\"}",
        RejectReason::Range});
    // And the raw-newline variant, which IS a parse failure - kept so the corpus documents the
    // distinction rather than leaving it to be rediscovered.
    cases.push_back({"target id carrying a raw newline",
        "{\"schemaVersion\":1,\"entityId\":\"RedSu35_01\",\"posture\":\"engage\","
        "\"targetEntityId\":\"Blue\nF18\",\"cruiseSpeedMps\":300.0,\"orbitRadiusM\":0.0,"
        "\"roe\":\"weaponsFree\",\"reason\":\"Committing.\"}",
        RejectReason::Parse});

    // -- run the table -----------------------------------------------------------------------------
    // Every failing row is reported, not just the first. With 50+ rows, a one-at-a-time report
    // turns a single fix-and-rerun cycle into fifty of them.
    std::vector<std::string> failures;
    for (const StageACase& testCase : cases) {
        const StageAOutcome outcome = validateStageA(testCase.body, kOwnId);
        if (outcome.accepted) {
            failures.push_back(std::string("'") + testCase.name + "' was ACCEPTED, expected reject '"
                + toString(testCase.expected) + "'");
            continue;
        }
        if (outcome.reason != testCase.expected) {
            failures.push_back(std::string("'") + testCase.name + "' -> '" + toString(outcome.reason)
                + "', expected '" + toString(testCase.expected) + "' (" + outcome.detail + ")");
        }
        // No Stage-A check may ever produce a Stage-B reason: that would mean a semantic check had
        // leaked onto the worker, where the SDK collaborators it needs are illegal to touch.
        if (isStageBReason(outcome.reason)) {
            failures.push_back(std::string("'") + testCase.name + "' produced Stage-B reason '"
                + toString(outcome.reason) + "' from Stage A");
        }
    }

    if (!failures.empty()) {
        std::string report = std::to_string(failures.size()) + " of " + std::to_string(cases.size())
            + " Stage-A corpus rows failed:";
        for (const std::string& failure : failures) {
            report += "\n    - " + failure;
        }
        AIC_FAIL(report);
    }

    // The corpus size is itself a requirement (AIC-VAL-1: "at least 40 adversarial payloads").
    // Asserting it here stops the suite being quietly thinned later.
    AIC_EXPECT_TRUE(cases.size() >= 40,
                    "the Stage-A corpus must carry at least 40 payloads, has "
                        + std::to_string(cases.size()));
    return true;
}

// Stage B's rows need live state, so they run against the fake world rather than as raw bodies.
AIC_TEST(AdversarialCorpusStageB) {
    const CommanderConfig config;   // defaults: 400 m/s, 100-20000 m HAE, 200 km geofence
    const FakeWorld world = makeWorld();

    struct Row {
        const char* name;
        RejectReason expected;
        Order order;
        StageBRequest request;
    };

    const auto engage = [](const std::string& target) {
        Order order;
        order.entityId = kOwnId;
        order.posture = Posture::Engage;
        order.targetEntityId = target;
        order.cruiseSpeedMps = 300.0;
        order.roe = Roe::WeaponsFree;
        order.reason = "Committing.";
        return order;
    };
    const auto ingress = [](double lat, double lon, double alt, double speed) {
        Order order;
        order.entityId = kOwnId;
        order.posture = Posture::Ingress;
        order.latitudeDeg = lat;
        order.longitudeDeg = lon;
        order.altitudeHaeM = alt;
        order.cruiseSpeedMps = speed;
        order.roe = Roe::WeaponsHold;
        order.reason = "Pressing.";
        return order;
    };

    std::vector<Row> rows;

    { Row r{"not on the roster", RejectReason::Roster, engage("BlueF18_02"), makeRequest()};
      r.request.onRoster = false; rows.push_back(r); }

    { Row r{"commanded entity no longer exists", RejectReason::Roster, engage("BlueF18_02"), makeRequest()};
      r.order.entityId = "RedSu35_77"; r.request.commandedEntityId = "RedSu35_77"; rows.push_back(r); }

    { Row r{"stale snapshot", RejectReason::Stale, engage("BlueF18_02"), makeRequest()};
      r.request.currentSimTimeS = r.request.snapshotSimTimeS + config.maxOrderAgeS + 1.0;
      rows.push_back(r); }

    { Row r{"snapshot from the future (scenario reloaded)", RejectReason::Stale, engage("BlueF18_02"), makeRequest()};
      r.request.currentSimTimeS = r.request.snapshotSimTimeS - 5.0; rows.push_back(r); }

    { Row r{"superseded by a newer published order", RejectReason::Superseded, engage("BlueF18_02"), makeRequest()};
      r.request.candidateSerial = r.request.publishedSerial; rows.push_back(r); }

    { Row r{"hallucinated target, never reported", RejectReason::Track, engage("BlueF18_09"), makeRequest()};
      rows.push_back(r); }

    { Row r{"target reported but does not exist", RejectReason::Track, engage("PhantomJet_01"), makeRequest()};
      r.request.reportedTrackIds.push_back("PhantomJet_01"); rows.push_back(r); }

    { Row r{"no tracks reported at all", RejectReason::Track, engage("BlueF18_02"), makeRequest()};
      r.request.reportedTrackIds.clear(); rows.push_back(r); }

    { Row r{"friendly target (fratricide)", RejectReason::Fratricide, engage("RedSu35_02"), makeRequest()};
      rows.push_back(r); }

    { Row r{"target of unknown team", RejectReason::Fratricide, engage("GhostContact_99"), makeRequest()};
      rows.push_back(r); }

    { Row r{"speed over the safety envelope", RejectReason::Clamp,
            ingress(13.51, 144.81, 9000.0, config.maxSpeedMps + 1.0), makeRequest()};
      rows.push_back(r); }

    { Row r{"altitude below the floor", RejectReason::Clamp,
            ingress(13.51, 144.81, config.minAltitudeHaeM - 1.0, 220.0), makeRequest()};
      rows.push_back(r); }

    { Row r{"altitude above the ceiling", RejectReason::Clamp,
            ingress(13.51, 144.81, config.maxAltitudeHaeM + 1.0, 220.0), makeRequest()};
      rows.push_back(r); }

    // ~1100 km away, well beyond the 200 km default geofence.
    { Row r{"waypoint beyond the geofence", RejectReason::Geofence,
            ingress(23.50, 144.80, 9000.0, 220.0), makeRequest()};
      rows.push_back(r); }

    std::vector<std::string> failures;
    for (const Row& row : rows) {
        const StageBOutcome outcome = validateStageB(row.order, row.request, config, world);
        if (outcome.accepted) {
            failures.push_back(std::string("'") + row.name + "' was ACCEPTED, expected reject '"
                + toString(row.expected) + "'");
            continue;
        }
        if (outcome.reason != row.expected) {
            failures.push_back(std::string("'") + row.name + "' -> '" + toString(outcome.reason)
                + "', expected '" + toString(row.expected) + "' (" + outcome.detail + ")");
        }
    }

    if (!failures.empty()) {
        std::string report = std::to_string(failures.size()) + " of " + std::to_string(rows.size())
            + " Stage-B corpus rows failed:";
        for (const std::string& failure : failures) {
            report += "\n    - " + failure;
        }
        AIC_FAIL(report);
    }
    AIC_EXPECT_TRUE(rows.size() >= 14, "the Stage-B corpus must carry at least 14 rows");
    return true;
}

// The fratricide check is the one whose failure mode is a scenario-visible harm rather than a bad
// log line, so it gets its own focused case as well as its corpus row.
AIC_TEST(FratricideCheckIsSymmetricAndFailsClosed) {
    const CommanderConfig config;
    FakeWorld world = makeWorld();

    Order order;
    order.entityId = kOwnId;
    order.posture = Posture::Engage;
    order.targetEntityId = "RedSu35_02";
    order.cruiseSpeedMps = 300.0;
    order.roe = Roe::WeaponsFree;
    order.reason = "Committing.";

    StageBRequest request = makeRequest();

    // Same team both ways.
    StageBOutcome outcome = validateStageB(order, request, config, world);
    AIC_EXPECT_TRUE(outcome.reason == RejectReason::Fratricide, "same team must reject fratricide");

    // Own team unknown must ALSO fail closed - "we could not tell whose side it is on" is not a
    // basis for shooting.
    world.teams[kOwnId] = "";
    order.targetEntityId = "BlueF18_02";
    outcome = validateStageB(order, request, config, world);
    AIC_EXPECT_TRUE(outcome.reason == RejectReason::Fratricide,
                    "an unknown own team must fail closed, got '"
                        + std::string(toString(outcome.reason)) + "'");
    return true;
}

// "Reject, don't repair": an out-of-envelope value must not be silently clamped to the bound. A
// repaired order is an order nobody specified.
AIC_TEST(StageBRejectsRatherThanClamping) {
    const CommanderConfig config;
    const FakeWorld world = makeWorld();

    Order order;
    order.entityId = kOwnId;
    order.posture = Posture::Ingress;
    order.latitudeDeg = 13.51;
    order.longitudeDeg = 144.81;
    order.altitudeHaeM = 9000.0;
    order.cruiseSpeedMps = config.maxSpeedMps + 50.0;
    order.roe = Roe::WeaponsHold;
    order.reason = "Pressing.";

    const Order before = order;
    const StageBOutcome outcome = validateStageB(order, makeRequest(), config, world);

    AIC_EXPECT_FALSE(outcome.accepted, "an over-envelope speed must be rejected");
    AIC_EXPECT_EQ(order.cruiseSpeedMps, before.cruiseSpeedMps,
                  "the order must not have been mutated - validation never repairs");
    return true;
}

// Boundary behaviour: exactly at a bound is inside it, one step past is outside. Off-by-one here
// would either reject legal orders or admit illegal ones, and neither shows up in a mid-range test.
AIC_TEST(StageBBoundariesAreInclusive) {
    const CommanderConfig config;
    const FakeWorld world = makeWorld();

    const auto ingressAt = [](double alt, double speed) {
        Order order;
        order.entityId = kOwnId;
        order.posture = Posture::Ingress;
        order.latitudeDeg = 13.501;
        order.longitudeDeg = 144.801;
        order.altitudeHaeM = alt;
        order.cruiseSpeedMps = speed;
        order.roe = Roe::WeaponsHold;
        order.reason = "Pressing.";
        return order;
    };

    AIC_EXPECT_TRUE(validateStageB(ingressAt(config.minAltitudeHaeM, 220.0), makeRequest(), config, world).accepted,
                    "exactly at the altitude floor must be accepted");
    AIC_EXPECT_TRUE(validateStageB(ingressAt(config.maxAltitudeHaeM, 220.0), makeRequest(), config, world).accepted,
                    "exactly at the altitude ceiling must be accepted");
    AIC_EXPECT_TRUE(validateStageB(ingressAt(9000.0, config.maxSpeedMps), makeRequest(), config, world).accepted,
                    "exactly at the speed bound must be accepted");

    AIC_EXPECT_FALSE(validateStageB(ingressAt(config.minAltitudeHaeM - 0.001, 220.0), makeRequest(), config, world).accepted,
                     "just below the altitude floor must be rejected");
    AIC_EXPECT_FALSE(validateStageB(ingressAt(config.maxAltitudeHaeM + 0.001, 220.0), makeRequest(), config, world).accepted,
                     "just above the altitude ceiling must be rejected");
    AIC_EXPECT_FALSE(validateStageB(ingressAt(9000.0, config.maxSpeedMps + 0.001), makeRequest(), config, world).accepted,
                     "just above the speed bound must be rejected");

    // Staleness, at and past the bound.
    StageBRequest atBound = makeRequest();
    atBound.currentSimTimeS = atBound.snapshotSimTimeS + config.maxOrderAgeS;
    AIC_EXPECT_TRUE(validateStageB(ingressAt(9000.0, 220.0), atBound, config, world).accepted,
                    "an order exactly at maxOrderAgeS must be accepted");

    StageBRequest pastBound = makeRequest();
    pastBound.currentSimTimeS = pastBound.snapshotSimTimeS + config.maxOrderAgeS + 0.001;
    AIC_EXPECT_FALSE(validateStageB(ingressAt(9000.0, 220.0), pastBound, config, world).accepted,
                     "an order just past maxOrderAgeS must be rejected");
    return true;
}
