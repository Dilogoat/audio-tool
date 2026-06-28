#include "catch_common.hpp"

#include "audio_core/detectors/PauseDetector.hpp"

#include <vector>

TEST_CASE("Pause detector finds sustained silence", "[pause]") {
    const int sr = 44100;
    std::vector<float> samples(sr * 3, 0.2f);
    for (int i = sr; i < sr * 2; ++i) {
        samples[static_cast<std::size_t>(i)] = 0.0f;
    }

    audio_core::PauseDetector detector({0.01, 0.5});
    const auto markers = detector.detect(samples, sr);
    REQUIRE(markers.size() >= 1);
    REQUIRE(markers.front().start_sec < 1.2);
    REQUIRE(markers.front().end_sec > 1.8);
}

TEST_CASE("Pause boundary at 1.9s is not detected with 2.0s minimum", "[pause][boundary]") {
    const int sr = 44100;
    std::vector<float> samples(sr * 4, 0.2f);
    const int pause_samples = static_cast<int>(1.9 * sr);
    for (int i = 0; i < pause_samples; ++i) {
        samples[static_cast<std::size_t>(i)] = 0.0f;
    }

    audio_core::PauseDetector detector({0.01, 2.0});
    const auto markers = detector.detect(samples, sr);
    REQUIRE(markers.empty());
}

TEST_CASE("Pause boundary above 2.0s is detected", "[pause][boundary]") {
    const int sr = 44100;
    std::vector<float> samples(sr * 4, 0.2f);
    const int pause_samples = static_cast<int>(2.2 * sr);
    for (int i = 0; i < pause_samples; ++i) {
        samples[static_cast<std::size_t>(i)] = 0.0f;
    }

    audio_core::PauseDetector detector({0.01, 2.0});
    const auto markers = detector.detect(samples, sr);
    REQUIRE(markers.size() >= 1);
    REQUIRE(markers.front().start_sec < 0.2);
    REQUIRE(markers.front().end_sec > 2.0);
}
