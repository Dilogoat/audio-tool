#pragma once

#include "audio_core/Markers.hpp"
#include "audio_core/Pipeline.hpp"

#include <string>
#include <vector>

namespace audio_core {

// Current on-disk schema version for .audiotool.json session files. Bump this
// whenever the serialized layout changes in a non-backward-compatible way.
constexpr int kSessionVersion = 1;

enum class ReviewDecision {
    Pending,
    Accepted,
    Rejected,
};

struct SessionMarker {
    Marker marker;
    ReviewDecision decision = ReviewDecision::Pending;
    std::string note;
};

struct Session {
    std::string source_path;
    std::string created_at;
    double duration_sec = 0.0;
    int sample_rate = 44100;
    PipelineConfig config{};
    std::vector<SessionMarker> markers;
};

std::string review_decision_to_string(ReviewDecision decision);
ReviewDecision review_decision_from_string(const std::string& value);

Session make_session(const std::string& source_path, const PipelineResult& result,
                     int sample_rate, const PipelineConfig& config);

void save_session_json(const std::string& path, const Session& session);
Session load_session_json(const std::string& path);

std::vector<Marker> markers_with_decision(const Session& session, ReviewDecision decision);
std::vector<Marker> exportable_markers(const Session& session);

// Copies review decisions/notes from a previously saved session onto a freshly
// built one, matching markers by type and timing. Lets re-runs (GUI or CLI)
// preserve a human's accept/reject choices instead of resetting to Pending.
// Returns the number of markers whose decision was carried over.
int merge_decisions(Session& target, const Session& source);

}  // namespace audio_core
