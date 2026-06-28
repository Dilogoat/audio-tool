#include "audio_core/RmsAnalyzer.hpp"

#include <cmath>
#include <stdexcept>

namespace audio_core {

RmsAnalyzer::RmsAnalyzer(int sample_rate, int window_samples, int hop_samples)
    : sample_rate_(sample_rate), window_samples_(window_samples), hop_samples_(hop_samples) {
    if (sample_rate_ <= 0 || window_samples_ <= 0 || hop_samples_ <= 0) {
        throw std::invalid_argument("Invalid RMS analyzer parameters");
    }
}

double RmsAnalyzer::compute_rms(const float* data, std::size_t count) {
    if (count == 0) {
        return 0.0;
    }
    double sum = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double v = static_cast<double>(data[i]);
        sum += v * v;
    }
    return std::sqrt(sum / static_cast<double>(count));
}

std::vector<RmsFrame> RmsAnalyzer::analyze(const std::vector<float>& samples) const {
    std::vector<RmsFrame> frames;
    if (samples.empty()) {
        return frames;
    }

    for (std::size_t start = 0; start + static_cast<std::size_t>(window_samples_) <= samples.size();
         start += static_cast<std::size_t>(hop_samples_)) {
        RmsFrame frame;
        frame.time_sec = static_cast<double>(start) / sample_rate_;
        frame.rms = compute_rms(samples.data() + start, static_cast<std::size_t>(window_samples_));
        frames.push_back(frame);
    }

    return frames;
}

}  // namespace audio_core
