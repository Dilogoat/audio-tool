#pragma once

#include "audio_core/Markers.hpp"

#include <vector>

namespace audio_core {

struct CleanConfig {
    // Linear gain applied inside breath/click markers (0.15 ~= -16.5 dB).
    double attenuation = 0.15;
    // Length of the raised-cosine fade into and out of each attenuated region.
    // The ramps sit just outside the marker so the marked span is fully attenuated.
    double ramp_ms = 5.0;
};

// Attenuates breath and click regions. The gain is applied uniformly across
// frequency, so it is implemented as an exact time-domain gain envelope rather
// than an STFT round-trip: it lines up with marker boundaries to the sample,
// fully covers short clicks, and uses no memory beyond the output buffer.
class SpectralCleaner {
public:
    std::vector<float> clean(const std::vector<float>& samples, int sample_rate,
                             const std::vector<Marker>& markers,
                             CleanConfig config = {}) const;
};

}  // namespace audio_core
