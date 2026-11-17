#pragma once

#include <string>
#include <vector>

namespace audio_core {

enum class MarkerType {
    Pause,
    Click,
    Breath,
    Speech
};

struct Marker {
    MarkerType type = MarkerType::Speech;
    double start_sec = 0.0;
    double end_sec = 0.0;
    double score = 0.0;
};

std::string marker_type_to_string(MarkerType type);
MarkerType marker_type_from_string(const std::string& name);

class Markers {
public:
    void add(Marker marker);
    void merge(const std::vector<Marker>& others);
    const std::vector<Marker>& all() const { return markers_; }
    std::vector<Marker> by_type(MarkerType type) const;
    // Orders markers chronologically (ties by end time), stable across types.
    void sort_by_time();

    void save_json(const std::string& path) const;
    static std::vector<Marker> load_json(const std::string& path);

    // Tab-separated Audacity label track (File > Import > Labels). Also imports
    // cleanly into Reaper and other DAWs that read Audacity labels.
    void save_audacity_labels(const std::string& path) const;

private:
    std::vector<Marker> markers_;
};

}  // namespace audio_core
