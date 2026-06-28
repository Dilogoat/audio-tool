#include "catch_common.hpp"

#include "audio_core/Pipeline.hpp"
#include "audio_core/Session.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

// Unique temp path per name so parallel/repeat runs never collide, with
// best-effort cleanup that also tolerates an assertion failing mid-test.
std::filesystem::path temp_path(const std::string& name) {
    return std::filesystem::temp_directory_path() / ("audio_tool_" + name + ".audiotool.json");
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

struct ScopedFile {
    std::filesystem::path path;
    explicit ScopedFile(std::filesystem::path p) : path(std::move(p)) {}
    ~ScopedFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

audio_core::SessionMarker make_marker(audio_core::MarkerType type, double start, double end,
                                      audio_core::ReviewDecision decision) {
    audio_core::SessionMarker sm;
    sm.marker.type = type;
    sm.marker.start_sec = start;
    sm.marker.end_sec = end;
    sm.marker.score = 0.5;
    sm.decision = decision;
    return sm;
}

}  // namespace

TEST_CASE("Session JSON round trip preserves review decisions", "[session]") {
    audio_core::Session session;
    session.source_path = "episode.wav";
    session.duration_sec = 12.5;
    session.sample_rate = 48000;

    audio_core::SessionMarker sm;
    sm.marker.type = audio_core::MarkerType::Breath;
    sm.marker.start_sec = 1.0;
    sm.marker.end_sec = 1.25;
    sm.marker.score = 0.7;
    sm.decision = audio_core::ReviewDecision::Rejected;
    sm.note = "false positive";
    session.markers.push_back(sm);

    ScopedFile file(temp_path("roundtrip"));
    audio_core::save_session_json(file.path.string(), session);
    const auto loaded = audio_core::load_session_json(file.path.string());

    REQUIRE(loaded.source_path == "episode.wav");
    REQUIRE(loaded.markers.size() == 1);
    REQUIRE(loaded.markers.front().marker.type == audio_core::MarkerType::Breath);
    REQUIRE(loaded.markers.front().decision == audio_core::ReviewDecision::Rejected);
    REQUIRE(loaded.markers.front().note == "false positive");
}

TEST_CASE("Session round trip preserves config and detect flags", "[session]") {
    audio_core::Session session;
    session.source_path = "cfg.wav";
    session.config.detect_pauses = false;
    session.config.detect_clicks = true;
    session.config.detect_breaths = false;
    session.config.clean.attenuation = 0.42;

    ScopedFile file(temp_path("config"));
    audio_core::save_session_json(file.path.string(), session);
    const auto loaded = audio_core::load_session_json(file.path.string());

    REQUIRE(loaded.config.detect_pauses == false);
    REQUIRE(loaded.config.detect_clicks == true);
    REQUIRE(loaded.config.detect_breaths == false);
    REQUIRE(loaded.config.clean.attenuation == Approx(0.42));
}

TEST_CASE("make_session captures markers as pending with metadata", "[session]") {
    audio_core::PipelineResult result;
    result.duration_sec = 5.0;
    result.markers.add({audio_core::MarkerType::Pause, 0.5, 1.0, 0.3});
    result.markers.add({audio_core::MarkerType::Click, 2.0, 2.01, 0.9});

    audio_core::PipelineConfig config;
    const auto session = audio_core::make_session("show.flac", result, 44100, config);

    REQUIRE(session.source_path == "show.flac");
    REQUIRE(session.duration_sec == Approx(5.0));
    REQUIRE(session.sample_rate == 44100);
    REQUIRE(session.markers.size() == 2);
    REQUIRE(session.created_at.empty() == false);
    for (const auto& sm : session.markers) {
        REQUIRE(sm.decision == audio_core::ReviewDecision::Pending);
    }
}

TEST_CASE("exportable_markers keeps pending and accepted, drops rejected", "[session]") {
    audio_core::Session session;
    session.markers.push_back(make_marker(audio_core::MarkerType::Pause, 0.0, 1.0,
                                          audio_core::ReviewDecision::Pending));
    session.markers.push_back(make_marker(audio_core::MarkerType::Click, 2.0, 2.1,
                                          audio_core::ReviewDecision::Accepted));
    session.markers.push_back(make_marker(audio_core::MarkerType::Breath, 3.0, 3.2,
                                          audio_core::ReviewDecision::Rejected));

    const auto exportable = audio_core::exportable_markers(session);
    REQUIRE(exportable.size() == 2);
    REQUIRE(exportable[0].type == audio_core::MarkerType::Pause);
    REQUIRE(exportable[1].type == audio_core::MarkerType::Click);
}

TEST_CASE("markers_with_decision filters by decision", "[session]") {
    audio_core::Session session;
    session.markers.push_back(make_marker(audio_core::MarkerType::Pause, 0.0, 1.0,
                                          audio_core::ReviewDecision::Accepted));
    session.markers.push_back(make_marker(audio_core::MarkerType::Click, 2.0, 2.1,
                                          audio_core::ReviewDecision::Rejected));
    session.markers.push_back(make_marker(audio_core::MarkerType::Breath, 3.0, 3.2,
                                          audio_core::ReviewDecision::Accepted));

    const auto accepted = audio_core::markers_with_decision(session, audio_core::ReviewDecision::Accepted);
    REQUIRE(accepted.size() == 2);
    const auto rejected = audio_core::markers_with_decision(session, audio_core::ReviewDecision::Rejected);
    REQUIRE(rejected.size() == 1);
    REQUIRE(rejected.front().type == audio_core::MarkerType::Click);
}

TEST_CASE("review_decision string mapping is total", "[session]") {
    REQUIRE(audio_core::review_decision_to_string(audio_core::ReviewDecision::Accepted) == "accepted");
    REQUIRE(audio_core::review_decision_to_string(audio_core::ReviewDecision::Rejected) == "rejected");
    REQUIRE(audio_core::review_decision_to_string(audio_core::ReviewDecision::Pending) == "pending");
    REQUIRE(audio_core::review_decision_from_string("accepted") == audio_core::ReviewDecision::Accepted);
    REQUIRE(audio_core::review_decision_from_string("rejected") == audio_core::ReviewDecision::Rejected);
    // Anything unrecognized is treated as pending rather than failing.
    REQUIRE(audio_core::review_decision_from_string("garbage") == audio_core::ReviewDecision::Pending);
}

TEST_CASE("load_session_json rejects a missing version", "[session]") {
    ScopedFile file(temp_path("noversion"));
    write_text(file.path, R"({"source_path":"x.wav","markers":[]})");
    REQUIRE_THROWS(audio_core::load_session_json(file.path.string()));
}

TEST_CASE("load_session_json rejects a newer schema version", "[session]") {
    ScopedFile file(temp_path("newver"));
    write_text(file.path,
               R"({"version":9999,"source_path":"x.wav","markers":[]})");
    REQUIRE_THROWS(audio_core::load_session_json(file.path.string()));
}

TEST_CASE("load_session_json rejects an unknown marker type", "[session]") {
    ScopedFile file(temp_path("badtype"));
    write_text(file.path, R"({
        "version": 1,
        "source_path": "x.wav",
        "markers": [
            {"type": "wibble", "start_sec": 0.0, "end_sec": 1.0, "score": 0.1, "decision": "pending"}
        ]
    })");
    REQUIRE_THROWS(audio_core::load_session_json(file.path.string()));
}

TEST_CASE("load_session_json treats an unknown decision as pending", "[session]") {
    ScopedFile file(temp_path("baddecision"));
    write_text(file.path, R"({
        "version": 1,
        "source_path": "x.wav",
        "markers": [
            {"type": "pause", "start_sec": 0.0, "end_sec": 1.0, "score": 0.1, "decision": "maybe"}
        ]
    })");
    const auto loaded = audio_core::load_session_json(file.path.string());
    REQUIRE(loaded.markers.size() == 1);
    REQUIRE(loaded.markers.front().decision == audio_core::ReviewDecision::Pending);
}

TEST_CASE("load_session_json throws on corrupt JSON", "[session]") {
    ScopedFile file(temp_path("corrupt"));
    write_text(file.path, "{ this is not json ");
    REQUIRE_THROWS(audio_core::load_session_json(file.path.string()));
}

TEST_CASE("save_session_json overwrites an existing file", "[session]") {
    ScopedFile file(temp_path("overwrite"));
    write_text(file.path, "stale contents that must be replaced");

    audio_core::Session session;
    session.source_path = "fresh.wav";
    session.markers.push_back(make_marker(audio_core::MarkerType::Pause, 0.0, 1.0,
                                          audio_core::ReviewDecision::Accepted));
    audio_core::save_session_json(file.path.string(), session);

    const auto loaded = audio_core::load_session_json(file.path.string());
    REQUIRE(loaded.source_path == "fresh.wav");
    REQUIRE(loaded.markers.size() == 1);
    // The temp file used for the atomic write must not be left behind.
    REQUIRE_FALSE(std::filesystem::exists(file.path.string() + ".tmp"));
}

TEST_CASE("merge_decisions carries prior decisions onto matching markers", "[session]") {
    audio_core::Session prior;
    prior.markers.push_back(make_marker(audio_core::MarkerType::Pause, 0.5, 1.0,
                                        audio_core::ReviewDecision::Rejected));
    prior.markers.push_back(make_marker(audio_core::MarkerType::Click, 2.0, 2.01,
                                        audio_core::ReviewDecision::Accepted));
    prior.markers.front().note = "human said no";

    // Freshly re-analyzed session: same markers plus a new one, all pending.
    audio_core::Session fresh;
    fresh.markers.push_back(make_marker(audio_core::MarkerType::Pause, 0.5, 1.0,
                                        audio_core::ReviewDecision::Pending));
    fresh.markers.push_back(make_marker(audio_core::MarkerType::Click, 2.0, 2.01,
                                        audio_core::ReviewDecision::Pending));
    fresh.markers.push_back(make_marker(audio_core::MarkerType::Breath, 5.0, 5.3,
                                        audio_core::ReviewDecision::Pending));

    const int carried = audio_core::merge_decisions(fresh, prior);
    REQUIRE(carried == 2);
    REQUIRE(fresh.markers[0].decision == audio_core::ReviewDecision::Rejected);
    REQUIRE(fresh.markers[0].note == "human said no");
    REQUIRE(fresh.markers[1].decision == audio_core::ReviewDecision::Accepted);
    // The new, unmatched marker stays pending.
    REQUIRE(fresh.markers[2].decision == audio_core::ReviewDecision::Pending);
}
