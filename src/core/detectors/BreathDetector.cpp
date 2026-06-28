#include "audio_core/detectors/BreathDetector.hpp"

#include "audio_core/RmsAnalyzer.hpp"

#include <algorithm>
#include <cmath>
#include <complex>

namespace audio_core {

BreathDetector::BreathDetector(BreathDetectorConfig config) : config_(config) {}

std::vector<Marker> BreathDetector::detect(const std::vector<float>& samples, int sample_rate) const {
    std::vector<Marker> markers;
    if (samples.empty() || sample_rate <= 0) {
        return markers;
    }

    Stft stft(sample_rate, config_.fft_size, config_.hop_size);
    const auto spec = stft.forward(samples);

    const int window = std::max(1, sample_rate / 20);
    const int hop = std::max(1, window / 2);
    RmsAnalyzer analyzer(sample_rate, window, hop);
    const auto rms_frames = analyzer.analyze(samples);

    auto rms_at = [&](double time_sec) {
        if (rms_frames.empty()) return 0.0;
        std::size_t best = 0;
        double best_dist = std::abs(rms_frames[0].time_sec - time_sec);
        for (std::size_t i = 1; i < rms_frames.size(); ++i) {
            const double d = std::abs(rms_frames[i].time_sec - time_sec);
            if (d < best_dist) {
                best_dist = d;
                best = i;
            }
        }
        return rms_frames[best].rms;
    };

    bool in_breath = false;
    double breath_start = 0.0;

    auto flush = [&](double end_sec) {
        const double duration = end_sec - breath_start;
        if (in_breath && duration >= config_.min_duration_sec && duration <= config_.max_duration_sec) {
            Marker m;
            m.type = MarkerType::Breath;
            m.start_sec = breath_start;
            m.end_sec = end_sec;
            m.score = 1.0;
            markers.push_back(m);
        }
        in_breath = false;
    };

    for (std::size_t i = 0; i < spec.frames.size(); ++i) {
        const auto& frame = spec.frames[i];
        double low_energy = 0.0;
        double high_energy = 0.0;
        const std::size_t split = frame.size() / 3;

        for (std::size_t b = 0; b < frame.size(); ++b) {
            const double mag = std::abs(frame[b]);
            if (b < split) {
                low_energy += mag;
            } else {
                high_energy += mag;
            }
        }

        const double total = low_energy + high_energy + 1e-12;
        const double hf_ratio = high_energy / total;
        const double time_sec = spec.frame_times[i];
        const double speech_rms = rms_at(time_sec);

        const bool breath_like =
            speech_rms < config_.speech_rms_threshold &&
            hf_ratio > config_.hf_ratio_threshold &&
            total > 1e-6;

        if (breath_like) {
            if (!in_breath) {
                in_breath = true;
                breath_start = time_sec;
            }
        } else if (in_breath) {
            flush(time_sec);
        }
    }

    if (in_breath) {
        flush(static_cast<double>(samples.size()) / sample_rate);
    }

    return markers;
}

}  // namespace audio_core
