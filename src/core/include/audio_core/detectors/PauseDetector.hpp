#pragma once

#include "audio_core/Markers.hpp"
#include "audio_core/RmsAnalyzer.hpp"

#include <vector>

namespace audio_core {

struct PauseDetectorConfig {
    double rms_threshold = 0.012;
    double min_duration_sec = 0.6;
};

class PauseDetector {
public:
    explicit PauseDetector(PauseDetectorConfig config = {});

    std::vector<Marker> detect(const std::vector<float>& samples, int sample_rate) const;

private:
    PauseDetectorConfig config_;
};

}  // namespace audio_core
