#pragma once

#include "audio_core/Markers.hpp"
#include "audio_core/Stft.hpp"

#include <vector>

namespace audio_core {

struct BreathDetectorConfig {
    double speech_rms_threshold = 0.03;
    double hf_ratio_threshold = 0.55;
    double min_duration_sec = 0.08;
    double max_duration_sec = 1.5;
    int fft_size = 2048;
    int hop_size = 512;
};

class BreathDetector {
public:
    explicit BreathDetector(BreathDetectorConfig config = {});

    std::vector<Marker> detect(const std::vector<float>& samples, int sample_rate) const;

private:
    BreathDetectorConfig config_;
};

}  // namespace audio_core
