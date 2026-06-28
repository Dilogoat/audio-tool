#include "catch_common.hpp"

#include "audio_core/AudioFile.hpp"
#include "audio_core/RmsAnalyzer.hpp"

#include <filesystem>

TEST_CASE("AudioFile loads generated fixture", "[audio][rms]") {
    const auto path = std::filesystem::path(TEST_FIXTURES_DIR) / "speech_with_pause.wav";
    if (!std::filesystem::exists(path)) {
        SKIP("Fixture not generated. Run scripts/gen_fixtures.py");
    }

    const auto data = audio_core::AudioFile::load(path.string());
    REQUIRE(data.sample_rate == 44100);
    REQUIRE(data.samples.size() > 0);

    audio_core::RmsAnalyzer analyzer(data.sample_rate, data.sample_rate / 20, data.sample_rate / 40);
    const auto frames = analyzer.analyze(data.samples);
    REQUIRE(frames.size() > 10);
}
