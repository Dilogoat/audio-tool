#include "audio_core/Pipeline.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace audio_core {

namespace {

using nlohmann::json;

json config_to_json(const PipelineConfig& c) {
    return json{
        {"detect", {{"pauses", c.detect_pauses}, {"clicks", c.detect_clicks}, {"breaths", c.detect_breaths}}},
        {"pause", {{"rms_threshold", c.pause.rms_threshold}, {"min_duration_sec", c.pause.min_duration_sec}}},
        {"transient",
         {{"window_ms", c.transient.window_ms},
          {"spike_ratio", c.transient.spike_ratio},
          {"min_local_rms", c.transient.min_local_rms},
          {"max_duration_sec", c.transient.max_duration_sec}}},
        {"breath",
         {{"speech_rms_threshold", c.breath.speech_rms_threshold},
          {"hf_ratio_threshold", c.breath.hf_ratio_threshold},
          {"min_duration_sec", c.breath.min_duration_sec},
          {"max_duration_sec", c.breath.max_duration_sec},
          {"fft_size", c.breath.fft_size},
          {"hop_size", c.breath.hop_size}}},
        {"score",
         {{"click_weight", c.score.click_weight},
          {"breath_weight", c.score.breath_weight},
          {"pause_weight", c.score.pause_weight},
          {"frame_duration_sec", c.score.frame_duration_sec}}},
        {"clean", {{"attenuation", c.clean.attenuation}, {"speech_floor_db", c.clean.speech_floor_db}}},
    };
}

template <typename T>
void read_into(const json& obj, const char* key, T& target) {
    if (obj.contains(key)) {
        target = obj.at(key).get<T>();
    }
}

}  // namespace

PipelineConfig PipelineConfig::from_json_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Failed to open config file: " + path);
    }
    const json root = json::parse(in);
    PipelineConfig c;

    if (root.contains("detect")) {
        const auto& d = root.at("detect");
        read_into(d, "pauses", c.detect_pauses);
        read_into(d, "clicks", c.detect_clicks);
        read_into(d, "breaths", c.detect_breaths);
    }
    if (root.contains("pause")) {
        const auto& p = root.at("pause");
        read_into(p, "rms_threshold", c.pause.rms_threshold);
        read_into(p, "min_duration_sec", c.pause.min_duration_sec);
    }
    if (root.contains("transient")) {
        const auto& t = root.at("transient");
        read_into(t, "window_ms", c.transient.window_ms);
        read_into(t, "spike_ratio", c.transient.spike_ratio);
        read_into(t, "min_local_rms", c.transient.min_local_rms);
        read_into(t, "max_duration_sec", c.transient.max_duration_sec);
    }
    if (root.contains("breath")) {
        const auto& b = root.at("breath");
        read_into(b, "speech_rms_threshold", c.breath.speech_rms_threshold);
        read_into(b, "hf_ratio_threshold", c.breath.hf_ratio_threshold);
        read_into(b, "min_duration_sec", c.breath.min_duration_sec);
        read_into(b, "max_duration_sec", c.breath.max_duration_sec);
        read_into(b, "fft_size", c.breath.fft_size);
        read_into(b, "hop_size", c.breath.hop_size);
    }
    if (root.contains("score")) {
        const auto& s = root.at("score");
        read_into(s, "click_weight", c.score.click_weight);
        read_into(s, "breath_weight", c.score.breath_weight);
        read_into(s, "pause_weight", c.score.pause_weight);
        read_into(s, "frame_duration_sec", c.score.frame_duration_sec);
    }
    if (root.contains("clean")) {
        const auto& cl = root.at("clean");
        read_into(cl, "attenuation", c.clean.attenuation);
        read_into(cl, "speech_floor_db", c.clean.speech_floor_db);
    }

    return c;
}

void PipelineConfig::to_json_file(const std::string& path) const {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to write config file: " + path);
    }
    out << config_to_json(*this).dump(2);
}

Pipeline::Pipeline(PipelineConfig config) : config_(config) {}

PipelineResult Pipeline::analyze(const AudioData& audio, bool produce_cleaned) const {
    PipelineResult result;
    result.sample_rate = audio.sample_rate;
    result.sample_count = audio.samples.size();
    result.duration_sec =
        audio.sample_rate > 0 ? static_cast<double>(audio.samples.size()) / audio.sample_rate : 0.0;

    if (config_.detect_pauses) {
        PauseDetector detector(config_.pause);
        result.markers.merge(detector.detect(audio.samples, audio.sample_rate));
    }
    if (config_.detect_clicks) {
        TransientDetector detector(config_.transient);
        result.markers.merge(detector.detect(audio.samples, audio.sample_rate));
    }
    if (config_.detect_breaths) {
        BreathDetector detector(config_.breath);
        result.markers.merge(detector.detect(audio.samples, audio.sample_rate));
    }

    for (const auto& m : result.markers.all()) {
        switch (m.type) {
            case MarkerType::Pause:
                ++result.pause_count;
                break;
            case MarkerType::Click:
                ++result.click_count;
                break;
            case MarkerType::Breath:
                ++result.breath_count;
                break;
            default:
                break;
        }
    }

    CompositeScore scorer(config_.score);
    result.scores = scorer.compute(result.markers.all(), result.duration_sec);
    for (const auto& s : result.scores) {
        result.peak_score = (std::max)(result.peak_score, s.score);
    }

    if (produce_cleaned) {
        SpectralCleaner cleaner;
        result.cleaned = cleaner.clean(audio.samples, audio.sample_rate, result.markers.all(), config_.clean);
        result.cleaned_produced = true;
    }

    return result;
}

void save_report_json(const std::string& path, const std::string& input_path,
                      const PipelineResult& result) {
    json root{
        {"input", input_path},
        {"sample_rate", result.sample_rate},
        {"sample_count", result.sample_count},
        {"duration_sec", result.duration_sec},
        {"counts",
         {{"pause", result.pause_count},
          {"click", result.click_count},
          {"breath", result.breath_count},
          {"total", static_cast<int>(result.markers.all().size())}}},
        {"peak_composite_score", result.peak_score},
        {"cleaned_produced", result.cleaned_produced},
    };

    json markers = json::array();
    for (const auto& m : result.markers.all()) {
        markers.push_back({
            {"type", marker_type_to_string(m.type)},
            {"start_sec", m.start_sec},
            {"end_sec", m.end_sec},
            {"score", m.score},
        });
    }
    root["markers"] = std::move(markers);

    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to write report: " + path);
    }
    out << root.dump(2);
}

}  // namespace audio_core
