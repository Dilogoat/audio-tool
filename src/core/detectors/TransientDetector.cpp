#include "audio_core/detectors/TransientDetector.hpp"

#include "audio_core/RmsAnalyzer.hpp"

#include <algorithm>
#include <cmath>

namespace audio_core {

TransientDetector::TransientDetector(TransientDetectorConfig config) : config_(config) {}

std::vector<Marker> TransientDetector::detect(const std::vector<float>& samples, int sample_rate) const {
    std::vector<Marker> markers;
    if (samples.empty() || sample_rate <= 0) {
        return markers;
    }

    const int window = std::max(1, sample_rate * config_.window_ms / 1000);
    const int hop = std::max(1, window / 2);
    RmsAnalyzer analyzer(sample_rate, window, hop);
    const auto frames = analyzer.analyze(samples);
    if (frames.size() < 5) {
        return markers;
    }

    for (std::size_t i = 2; i + 2 < frames.size(); ++i) {
        const double local =
            (frames[i - 2].rms + frames[i - 1].rms + frames[i + 1].rms + frames[i + 2].rms) / 4.0;
        const double ratio = local > 1e-9 ? frames[i].rms / local : frames[i].rms / config_.min_local_rms;

        if (frames[i].rms >= config_.min_local_rms && ratio >= config_.spike_ratio) {
            Marker m;
            m.type = MarkerType::Click;
            m.start_sec = std::max(0.0, frames[i].time_sec - config_.max_duration_sec / 2.0);
            m.end_sec = frames[i].time_sec + config_.max_duration_sec / 2.0;
            m.score = std::min(1.0, ratio / (config_.spike_ratio * 2.0));
            markers.push_back(m);
        }
    }

    return markers;
}

}  // namespace audio_core
