#pragma once

#include "audio_core/Markers.hpp"

#include <vector>

namespace audio_core {

struct TransientDetectorConfig {
    int window_ms = 5;
    double spike_ratio = 4.0;
    double min_local_rms = 0.005;
    double max_duration_sec = 0.03;
};

class TransientDetector {
public:
    explicit TransientDetector(TransientDetectorConfig config = {});

    std::vector<Marker> detect(const std::vector<float>& samples, int sample_rate) const;

private:
    TransientDetectorConfig config_;
};

}  // namespace audio_core
