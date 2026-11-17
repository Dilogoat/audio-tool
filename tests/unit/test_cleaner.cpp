#include "catch_common.hpp"

#include "audio_core/SpectralCleaner.hpp"

#include <vector>

namespace {

audio_core::Marker make(audio_core::MarkerType type, double start, double end) {
    audio_core::Marker m;
    m.type = type;
    m.start_sec = start;
    m.end_sec = end;
    m.score = 1.0;
    return m;
}

}  // namespace

TEST_CASE("Cleaner fully attenuates a short click marker, sample-aligned", "[cleaner]") {
    const int sr = 48000;
    const std::vector<float> samples(sr, 0.5f);
    // 30 ms click marker at 0.5 s: the old STFT cleaner keyed off frame start
    // times and only attenuated ~1 of 4 overlapping frames here.
    const auto out = audio_core::SpectralCleaner{}.clean(
        samples, sr, {make(audio_core::MarkerType::Click, 0.500, 0.530)}, {0.15, 5.0});

    REQUIRE(out.size() == samples.size());
    for (int i = 24000; i < 25440; ++i) {
        REQUIRE(out[static_cast<std::size_t>(i)] == Approx(0.5f * 0.15f));
    }
    // Ramps sit outside the marker and are 5 ms (240 samples) long.
    REQUIRE(out[23999] < 0.5f);
    REQUIRE(out[23999] > 0.5f * 0.15f);
    REQUIRE(out[24000 - 241] == Approx(0.5f));
    REQUIRE(out[25440 + 241] == Approx(0.5f));
    REQUIRE(out[0] == Approx(0.5f));
    REQUIRE(out[static_cast<std::size_t>(sr - 1)] == Approx(0.5f));
}

TEST_CASE("Cleaner does not attenuate overlapping markers twice", "[cleaner]") {
    const int sr = 8000;
    const std::vector<float> samples(sr, 1.0f);
    const auto out = audio_core::SpectralCleaner{}.clean(
        samples, sr,
        {make(audio_core::MarkerType::Breath, 0.2, 0.5), make(audio_core::MarkerType::Click, 0.3, 0.35)},
        {0.25, 0.0});
    REQUIRE(out[static_cast<std::size_t>(0.32 * sr)] == Approx(0.25f));
    REQUIRE(out[static_cast<std::size_t>(0.45 * sr)] == Approx(0.25f));
    REQUIRE(out[static_cast<std::size_t>(0.6 * sr)] == Approx(1.0f));
}

TEST_CASE("Cleaner ignores pause markers", "[cleaner]") {
    const int sr = 8000;
    const std::vector<float> samples(sr, 0.3f);
    const auto out =
        audio_core::SpectralCleaner{}.clean(samples, sr, {make(audio_core::MarkerType::Pause, 0.1, 0.9)});
    REQUIRE(out == samples);
}
