#include "audio_core/detectors/PauseDetector.hpp"

namespace audio_core {

PauseDetector::PauseDetector(PauseDetectorConfig config) : config_(config) {}

std::vector<Marker> PauseDetector::detect(const std::vector<float>& samples, int sample_rate) const {
    std::vector<Marker> markers;
    if (samples.empty() || sample_rate <= 0) {
        return markers;
    }

    const int window = std::max(1, sample_rate / 20);
    const int hop = std::max(1, window / 2);
    RmsAnalyzer analyzer(sample_rate, window, hop);
    const auto frames = analyzer.analyze(samples);

    bool in_pause = false;
    double pause_start = 0.0;

    auto flush_pause = [&](double end_sec) {
        const double duration = end_sec - pause_start;
        if (in_pause && duration + 1e-3 >= config_.min_duration_sec) {
            Marker m;
            m.type = MarkerType::Pause;
            m.start_sec = pause_start;
            m.end_sec = end_sec;
            m.score = 1.0;
            markers.push_back(m);
        }
        in_pause = false;
    };

    for (const auto& frame : frames) {
        if (frame.rms < config_.rms_threshold) {
            if (!in_pause) {
                in_pause = true;
                pause_start = frame.time_sec;
            }
        } else if (in_pause) {
            flush_pause(frame.time_sec);
        }
    }

    if (in_pause) {
        flush_pause(static_cast<double>(samples.size()) / sample_rate);
    }

    return markers;
}

}  // namespace audio_core
