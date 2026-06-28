#pragma once

#include "audio_core/Markers.hpp"
#include "audio_core/Stft.hpp"

#include <vector>

namespace audio_core {

struct CleanConfig {
    double attenuation = 0.15;
    double speech_floor_db = -40.0;
};

class SpectralCleaner {
public:
    std::vector<float> clean(const std::vector<float>& samples, int sample_rate,
                             const std::vector<Marker>& markers,
                             CleanConfig config = {}) const;
};

}  // namespace audio_core
