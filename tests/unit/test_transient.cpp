#include "catch_common.hpp"

#include "audio_core/detectors/TransientDetector.hpp"

#include <vector>

TEST_CASE("Transient detector finds click burst", "[transient]") {
    const int sr = 44100;
    std::vector<float> samples(sr, 0.02f);
    const int click_pos = sr / 2;
    for (int i = -5; i <= 5; ++i) {
        samples[static_cast<std::size_t>(click_pos + i)] = 0.95f;
    }

    audio_core::TransientDetector detector({5, 2.0, 0.001, 0.05});
    const auto markers = detector.detect(samples, sr);
    REQUIRE(markers.size() >= 1);
    REQUIRE(markers.front().start_sec < 1.2);
    REQUIRE(markers.front().end_sec > 0.4);
}
