#include "audio_core/Markers.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace audio_core {

std::string marker_type_to_string(MarkerType type) {
    switch (type) {
        case MarkerType::Pause:
            return "pause";
        case MarkerType::Click:
            return "click";
        case MarkerType::Breath:
            return "breath";
        case MarkerType::Speech:
            return "speech";
    }
    return "unknown";
}

MarkerType marker_type_from_string(const std::string& name) {
    if (name == "pause") return MarkerType::Pause;
    if (name == "click") return MarkerType::Click;
    if (name == "breath") return MarkerType::Breath;
    return MarkerType::Speech;
}

void Markers::add(Marker marker) { markers_.push_back(std::move(marker)); }

void Markers::merge(const std::vector<Marker>& others) {
    markers_.insert(markers_.end(), others.begin(), others.end());
}

std::vector<Marker> Markers::by_type(MarkerType type) const {
    std::vector<Marker> filtered;
    for (const auto& m : markers_) {
        if (m.type == type) {
            filtered.push_back(m);
        }
    }
    return filtered;
}

void Markers::save_json(const std::string& path) const {
    nlohmann::json root;
    root["markers"] = nlohmann::json::array();
    for (const auto& m : markers_) {
        root["markers"].push_back({
            {"type", marker_type_to_string(m.type)},
            {"start_sec", m.start_sec},
            {"end_sec", m.end_sec},
            {"score", m.score},
        });
    }

    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to write markers JSON");
    }
    out << root.dump(2);
}

void Markers::save_audacity_labels(const std::string& path) const {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to write labels file");
    }
    for (const auto& m : markers_) {
        out << m.start_sec << '\t' << m.end_sec << '\t' << marker_type_to_string(m.type) << '\n';
    }
}

std::vector<Marker> Markers::load_json(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Failed to read markers JSON");
    }

    const auto root = nlohmann::json::parse(in);
    std::vector<Marker> markers;
    for (const auto& item : root.at("markers")) {
        Marker m;
        m.type = marker_type_from_string(item.at("type").get<std::string>());
        m.start_sec = item.at("start_sec").get<double>();
        m.end_sec = item.at("end_sec").get<double>();
        m.score = item.value("score", 0.0);
        markers.push_back(m);
    }
    return markers;
}

}  // namespace audio_core
