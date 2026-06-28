#include "audio_core/SpectralCleaner.hpp"

#include <algorithm>
#include <cmath>

namespace audio_core {

std::vector<float> SpectralCleaner::clean(const std::vector<float>& samples, int sample_rate,
                                          const std::vector<Marker>& markers,
                                          CleanConfig config) const {
    if (samples.empty()) {
        return samples;
    }

    Stft stft(sample_rate, 2048, 512);
    auto spec = stft.forward(samples);

    auto is_marked = [&](double time_sec) {
        for (const auto& m : markers) {
            if (time_sec >= m.start_sec && time_sec <= m.end_sec &&
                (m.type == MarkerType::Breath || m.type == MarkerType::Click)) {
                return true;
            }
        }
        return false;
    };

    for (std::size_t i = 0; i < spec.frames.size(); ++i) {
        if (!is_marked(spec.frame_times[i])) {
            continue;
        }
        for (auto& bin : spec.frames[i]) {
            bin *= config.attenuation;
        }
    }

    auto reconstructed = stft.inverse(spec);
    reconstructed.resize(samples.size());
    return reconstructed;
}

}  // namespace audio_core
