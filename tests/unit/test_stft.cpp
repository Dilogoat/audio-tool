#include "catch_common.hpp"
#include <cmath>
#include <complex>

#include "audio_core/Framing.hpp"
#include "audio_core/Stft.hpp"

#include <cmath>
#include <vector>

TEST_CASE("Hann window endpoints are zero-ish", "[stft]") {
    const auto w = audio_core::hann_window(1024);
    REQUIRE(w.front() == Approx(0.0).margin(1e-6));
    REQUIRE(w.back() == Approx(0.0).margin(1e-6));
    REQUIRE(w[w.size() / 2] == Approx(1.0).margin(1e-3));
}

TEST_CASE("STFT round trip preserves energy approximately", "[stft]") {
    const int sr = 44100;
    std::vector<float> samples(static_cast<std::size_t>(sr / 4));
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = static_cast<float>(0.3 * std::sin(2.0 * M_PI * 440.0 * i / sr));
    }

    audio_core::Stft stft(sr, 1024, 256);
    const auto spec = stft.forward(samples);
    const auto reconstructed = stft.inverse(spec);

    double in_energy = 0.0;
    double out_energy = 0.0;
    const std::size_t count = std::min(samples.size(), reconstructed.size());
    for (std::size_t i = 0; i < count; ++i) {
        in_energy += samples[i] * samples[i];
        out_energy += reconstructed[i] * reconstructed[i];
    }
    REQUIRE(out_energy > 0.0);
    REQUIRE(out_energy / in_energy == Approx(1.0).margin(0.35));
}

TEST_CASE("for_each_frame streams the same frames forward() stores", "[stft]") {
    const int sr = 16000;
    std::vector<float> samples(5000);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = static_cast<float>(std::sin(0.01 * static_cast<double>(i)));
    }
    audio_core::Stft stft(sr, 512, 128);
    const auto spec = stft.forward(samples);
    std::size_t n = 0;
    stft.for_each_frame(samples, [&](std::size_t start, const std::vector<std::complex<double>>& bins) {
        REQUIRE(n < spec.frames.size());
        REQUIRE(static_cast<double>(start) / sr == Approx(spec.frame_times[n]));
        REQUIRE(std::abs(bins[10] - spec.frames[n][10]) < 1e-9);
        ++n;
    });
    REQUIRE(n == spec.frames.size());
}
