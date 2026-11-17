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

    const int window = std::max(1, sample_rate / 20);
    const int hop = std::max(1, window / 2);
    RmsAnalyzer analyzer(sample_rate, window, hop);
    const auto rms_frames = analyzer.analyze(samples);

    // RMS frames are evenly spaced (frame k covers [k*hop, k*hop + window)), so
    // the frame centred nearest a sample position is a direct index — O(1)
    // instead of scanning every RMS frame for every STFT frame.
    auto rms_at_sample = [&](double centre_sample) {
        if (rms_frames.empty()) return 0.0;
        const double k = std::round((centre_sample - window / 2.0) / hop);
        const auto idx = static_cast<std::size_t>(
            std::clamp(k, 0.0, static_cast<double>(rms_frames.size() - 1)));
        return rms_frames[idx].rms;
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

    const double half_fft = config_.fft_size / 2.0;
    const double total_sec = static_cast<double>(samples.size()) / sample_rate;

    // Stream frames rather than materialising the whole spectrogram (which is
    // several GB for an hour of audio). Times use the window centre, which is
    // where a frame's energy actually sits.
    Stft stft(sample_rate, config_.fft_size, config_.hop_size);
    stft.for_each_frame(samples, [&](std::size_t start, const std::vector<std::complex<double>>& frame) {
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
        const double centre_sample = static_cast<double>(start) + half_fft;
        const double time_sec = std::min(centre_sample / sample_rate, total_sec);
        const double speech_rms = rms_at_sample(centre_sample);

        const bool breath_like = speech_rms < config_.speech_rms_threshold &&
                                 hf_ratio > config_.hf_ratio_threshold && total > 1e-6;

        if (breath_like) {
            if (!in_breath) {
                in_breath = true;
                breath_start = time_sec;
            }
        } else if (in_breath) {
            flush(time_sec);
        }
    });

    if (in_breath) {
        flush(total_sec);
    }

    return markers;
}

}  // namespace audio_core
