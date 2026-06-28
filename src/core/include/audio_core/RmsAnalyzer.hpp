#pragma once

#include <cstddef>
#include <vector>

namespace audio_core {

struct RmsFrame {
    double time_sec = 0.0;
    double rms = 0.0;
};

class RmsAnalyzer {
public:
    RmsAnalyzer(int sample_rate, int window_samples, int hop_samples);

    std::vector<RmsFrame> analyze(const std::vector<float>& samples) const;
    static double compute_rms(const float* data, std::size_t count);

private:
    int sample_rate_;
    int window_samples_;
    int hop_samples_;
};

}  // namespace audio_core
