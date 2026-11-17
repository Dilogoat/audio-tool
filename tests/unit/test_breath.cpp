#include "catch_common.hpp"
#include <cmath>

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

TEST_CASE("Breath marker boundaries track the noise segment", "[breath]") {
    const int sr = 44100;
    std::vector<float> samples(sr * 3, 0.0f);
    // Breath-like HF content from 1.0 s to 1.6 s.
    for (int i = sr; i < sr + (sr * 6) / 10; ++i) {
        const float t = static_cast<float>(i) / sr;
        samples[static_cast<std::size_t>(i)] = 0.02f * std::sin(2.0f * 3.14159f * 9000.0f * t) +
                                               0.02f * std::sin(2.0f * 3.14159f * 13000.0f * t);
    }
    audio_core::BreathDetector detector({0.08, 0.35, 0.05, 2.0, 2048, 512});
    const auto markers = detector.detect(samples, sr);
    REQUIRE(markers.size() == 1);
    // Times use the window centre, so boundaries land within one window.
    REQUIRE(markers[0].start_sec == Approx(1.0).margin(2048.0 / sr));
    REQUIRE(markers[0].end_sec == Approx(1.6).margin(2048.0 / sr));
}
