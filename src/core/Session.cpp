#include "audio_core/Session.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace audio_core {

namespace {

std::string utc_now_iso8601() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

nlohmann::json marker_to_json(const Marker& marker) {
    return {
        {"type", marker_type_to_string(marker.type)},
        {"start_sec", marker.start_sec},
        {"end_sec", marker.end_sec},
        {"score", marker.score},
    };
}

Marker marker_from_json(const nlohmann::json& item) {
    Marker marker;
    const auto type_str = item.at("type").get<std::string>();
    // marker_type_from_string silently maps anything unknown to Speech, which
    // would corrupt a marker list on a typo or a forward-compatible type name.
    // Reject unknown strings explicitly instead.
    if (type_str != "pause" && type_str != "click" && type_str != "breath" &&
        type_str != "speech") {
        throw std::runtime_error("Unknown marker type in session: " + type_str);
    }
    marker.type = marker_type_from_string(type_str);
    marker.start_sec = item.at("start_sec").get<double>();
    marker.end_sec = item.at("end_sec").get<double>();
    marker.score = item.value("score", 0.0);
    return marker;
}

}  // namespace

std::string review_decision_to_string(ReviewDecision decision) {
    switch (decision) {
        case ReviewDecision::Pending:
            return "pending";
        case ReviewDecision::Accepted:
            return "accepted";
        case ReviewDecision::Rejected:
            return "rejected";
    }
    return "pending";
}

ReviewDecision review_decision_from_string(const std::string& value) {
    if (value == "accepted") {
        return ReviewDecision::Accepted;
    }
    if (value == "rejected") {
        return ReviewDecision::Rejected;
    }
    return ReviewDecision::Pending;
}

Session make_session(const std::string& source_path, const PipelineResult& result,
                     int sample_rate, const PipelineConfig& config) {
    Session session;
    session.source_path = source_path;
    session.created_at = utc_now_iso8601();
    session.duration_sec = result.duration_sec;
    session.sample_rate = sample_rate;
    session.config = config;
    session.markers.reserve(result.markers.all().size());
    for (const auto& marker : result.markers.all()) {
        session.markers.push_back({marker, ReviewDecision::Pending, {}});
    }
    return session;
}

void save_session_json(const std::string& path, const Session& session) {
    nlohmann::json root;
    root["version"] = kSessionVersion;
    root["source_path"] = session.source_path;
    root["created_at"] = session.created_at.empty() ? utc_now_iso8601() : session.created_at;
    root["duration_sec"] = session.duration_sec;
    root["sample_rate"] = session.sample_rate;
    root["config"] = session.config.to_json();
    root["markers"] = nlohmann::json::array();
    for (const auto& item : session.markers) {
        auto marker = marker_to_json(item.marker);
        marker["decision"] = review_decision_to_string(item.decision);
        marker["note"] = item.note;
        root["markers"].push_back(marker);
    }

    // Write to a sibling temp file and atomically rename into place so a crash
    // or power loss mid-write can never leave a truncated, unparseable session.
    namespace fs = std::filesystem;
    const fs::path final_path(path);
    const fs::path tmp_path = final_path.parent_path() /
                              (final_path.filename().string() + ".tmp");
    {
        std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw std::runtime_error("Failed to write session JSON");
        }
        out << root.dump(2);
        out.flush();
        if (!out) {
            throw std::runtime_error("Failed while writing session JSON");
        }
    }

    std::error_code ec;
    fs::rename(tmp_path, final_path, ec);
    if (ec) {
        // Some platforms refuse rename onto an existing file; fall back to
        // replace, then clean up the temp on success.
        fs::remove(final_path, ec);
        fs::rename(tmp_path, final_path, ec);
        if (ec) {
            fs::remove(tmp_path, ec);
            throw std::runtime_error("Failed to commit session JSON: " + ec.message());
        }
    }
}

Session load_session_json(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Failed to read session JSON");
    }
    const auto root = nlohmann::json::parse(in);

    // Validate the schema version up front so a newer/hand-edited file fails
    // loudly instead of silently loading with partial defaults.
    const int version = root.value("version", 0);
    if (version <= 0) {
        throw std::runtime_error("Session JSON missing or invalid version field");
    }
    if (version > kSessionVersion) {
        throw std::runtime_error(
            "Session JSON version " + std::to_string(version) +
            " is newer than supported version " + std::to_string(kSessionVersion));
    }

    Session session;
    session.source_path = root.value("source_path", "");
    session.created_at = root.value("created_at", "");
    session.duration_sec = root.value("duration_sec", 0.0);
    session.sample_rate = root.value("sample_rate", 44100);
    if (root.contains("config")) {
        session.config = PipelineConfig::from_json(root.at("config"));
    }
    if (root.contains("markers")) {
        for (const auto& item : root.at("markers")) {
            SessionMarker sm;
            sm.marker = marker_from_json(item);
            sm.decision = review_decision_from_string(item.value("decision", "pending"));
            sm.note = item.value("note", "");
            session.markers.push_back(sm);
        }
    }
    return session;
}

std::vector<Marker> markers_with_decision(const Session& session, ReviewDecision decision) {
    std::vector<Marker> out;
    for (const auto& item : session.markers) {
        if (item.decision == decision) {
            out.push_back(item.marker);
        }
    }
    return out;
}

std::vector<Marker> exportable_markers(const Session& session) {
    std::vector<Marker> out;
    for (const auto& item : session.markers) {
        if (item.decision != ReviewDecision::Rejected) {
            out.push_back(item.marker);
        }
    }
    return out;
}

namespace {

double overlap_ratio(const Marker& a, const Marker& b) {
    const double inter = (std::min)(a.end_sec, b.end_sec) - (std::max)(a.start_sec, b.start_sec);
    if (inter <= 0.0) {
        return 0.0;
    }
    const double uni = (std::max)(a.end_sec, b.end_sec) - (std::min)(a.start_sec, b.start_sec);
    return uni > 0.0 ? inter / uni : 0.0;
}

}  // namespace

int merge_decisions(Session& target, const Session& source, int* unmatched) {
    // Match each reviewed source marker to the same-type target marker it
    // overlaps most (intersection over union >= kMinOverlap), one-to-one.
    // Exact-time matching dropped every decision as soon as a threshold change
    // nudged marker boundaries.
    constexpr double kMinOverlap = 0.5;

    struct Candidate {
        double score;
        std::size_t src;
        std::size_t tgt;
    };
    std::vector<Candidate> candidates;
    int reviewed = 0;
    for (std::size_t si = 0; si < source.markers.size(); ++si) {
        const auto& src = source.markers[si];
        if (src.decision == ReviewDecision::Pending && src.note.empty()) {
            continue;
        }
        ++reviewed;
        for (std::size_t ti = 0; ti < target.markers.size(); ++ti) {
            const auto& tgt = target.markers[ti];
            if (tgt.marker.type != src.marker.type) {
                continue;
            }
            const double score = overlap_ratio(src.marker, tgt.marker);
            if (score >= kMinOverlap) {
                candidates.push_back({score, si, ti});
            }
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
    std::vector<bool> src_used(source.markers.size(), false);
    std::vector<bool> tgt_used(target.markers.size(), false);
    int carried = 0;
    for (const auto& c : candidates) {
        if (src_used[c.src] || tgt_used[c.tgt]) {
            continue;
        }
        src_used[c.src] = true;
        tgt_used[c.tgt] = true;
        target.markers[c.tgt].decision = source.markers[c.src].decision;
        target.markers[c.tgt].note = source.markers[c.src].note;
        ++carried;
    }

    if (unmatched) {
        *unmatched = reviewed - carried;
    }
    return carried;
}

bool has_review_work(const Session& session) {
    for (const auto& item : session.markers) {
        if (item.decision != ReviewDecision::Pending || !item.note.empty()) {
            return true;
        }
    }
    return false;
}

}  // namespace audio_core
