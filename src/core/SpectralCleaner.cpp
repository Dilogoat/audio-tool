#include "audio_core/SpectralCleaner.hpp"

#include <algorithm>
#include <cmath>

namespace audio_core {

namespace {

struct Span {
    long long start = 0;
    long long end = 0;
};

}  // namespace

std::vector<float> SpectralCleaner::clean(const std::vector<float>& samples, int sample_rate,
                                          const std::vector<Marker>& markers,
                                          CleanConfig config) const {
    std::vector<float> out = samples;
    if (samples.empty() || sample_rate <= 0) {
        return out;
    }

    const long long n = static_cast<long long>(samples.size());
    const double gain = std::clamp(config.attenuation, 0.0, 1.0);
    const long long ramp = (std::max)(0LL, std::llround(config.ramp_ms / 1000.0 * sample_rate));

    std::vector<Span> spans;
    for (const auto& m : markers) {
        if (m.type != MarkerType::Breath && m.type != MarkerType::Click) {
            continue;
        }
        const long long s = std::clamp(std::llround(m.start_sec * sample_rate), 0LL, n);
        const long long e = std::clamp(std::llround(m.end_sec * sample_rate), 0LL, n);
        if (e > s) {
            spans.push_back({s, e});
        }
    }
    if (spans.empty()) {
        return out;
    }

    // Merge spans whose ramps would overlap, so overlapping markers never
    // attenuate the same samples twice.
    std::sort(spans.begin(), spans.end(), [](const Span& a, const Span& b) { return a.start < b.start; });
    std::vector<Span> merged{spans.front()};
    for (std::size_t i = 1; i < spans.size(); ++i) {
        if (spans[i].start - ramp <= merged.back().end + ramp) {
            merged.back().end = (std::max)(merged.back().end, spans[i].end);
        } else {
            merged.push_back(spans[i]);
        }
    }

    const double pi = 3.14159265358979323846;
    auto ramp_gain = [&](long long k) {
        // k = 1 is the sample adjacent to the marker, k = ramp the outermost.
        // Sampling the half-cosine at bin centres keeps every ramp sample
        // strictly between full gain and the attenuated level.
        const double x = (static_cast<double>(ramp - k) + 0.5) / static_cast<double>(ramp);
        const double w = 0.5 - 0.5 * std::cos(pi * x);
        return 1.0 - (1.0 - gain) * w;
    };

    for (const auto& sp : merged) {
        for (long long i = sp.start; i < sp.end; ++i) {
            out[static_cast<std::size_t>(i)] = static_cast<float>(samples[static_cast<std::size_t>(i)] * gain);
        }
        for (long long k = 1; k <= ramp; ++k) {
            const double g = ramp_gain(k);
            const long long before = sp.start - k;
            const long long after = sp.end - 1 + k;
            if (before >= 0) {
                out[static_cast<std::size_t>(before)] =
                    static_cast<float>(samples[static_cast<std::size_t>(before)] * g);
            }
            if (after < n) {
                out[static_cast<std::size_t>(after)] =
                    static_cast<float>(samples[static_cast<std::size_t>(after)] * g);
            }
        }
    }
    return out;
}

}  // namespace audio_core
