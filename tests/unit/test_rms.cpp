#include "catch_common.hpp"

#include "audio_core/RmsAnalyzer.hpp"

#include <cmath>
#include <vector>

TEST_CASE("RMS of constant signal", "[rms]") {
    std::vector<float> samples(1000, 0.5f);
    const double rms = audio_core::RmsAnalyzer::compute_rms(samples.data(), samples.size());
    REQUIRE(rms == Approx(0.5).margin(1e-6));
}

TEST_CASE("RMS of silence is zero", "[rms]") {
    std::vector<float> samples(512, 0.0f);
    const double rms = audio_core::RmsAnalyzer::compute_rms(samples.data(), samples.size());
    REQUIRE(rms == Approx(0.0).margin(1e-12));
}

TEST_CASE("Windowed RMS envelope length", "[rms]") {
    const int sr = 44100;
    std::vector<float> samples(sr, 0.25f);
    audio_core::RmsAnalyzer analyzer(sr, 1024, 512);
    const auto frames = analyzer.analyze(samples);
    REQUIRE(frames.size() > 0);
    REQUIRE(frames.front().rms == Approx(0.25).margin(1e-3));
}
