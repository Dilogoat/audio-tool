#include "catch_common.hpp"

#include "audio_core/detectors/BreathDetector.hpp"

#include <vector>

TEST_CASE("Breath detector finds high-frequency noise segment", "[breath]") {
    const int sr = 44100;
    std::vector<float> samples(sr * 2, 0.0f);

    for (int i = sr / 2; i < sr + sr / 2; ++i) {
        const float t = static_cast<float>(i) / sr;
        samples[static_cast<std::size_t>(i)] = 0.05f * std::sin(2.0f * 3.14159f * 8000.0f * t) +
                                               0.04f * std::sin(2.0f * 3.14159f * 12000.0f * t);
    }

    audio_core::BreathDetector detector({0.08, 0.35, 0.05, 2.0, 1024, 256});
    const auto markers = detector.detect(samples, sr);
    REQUIRE(markers.size() >= 1);
}
