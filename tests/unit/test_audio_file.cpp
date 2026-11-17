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

TEST_CASE("AudioFile mixes stereo down and reports both channel counts", "[audio]") {
    const auto path = std::filesystem::temp_directory_path() / "audio_tool_stereo.wav";
    audio_core::AudioData stereo;
    stereo.sample_rate = 8000;
    stereo.channels = 2;
    stereo.samples = {0.5f, -0.5f, 0.25f, 0.75f, 0.0f, 0.0f, 1.0f, 0.0f};
    audio_core::AudioFile::save(path.string(), stereo);

    const auto loaded = audio_core::AudioFile::load(path.string());
    std::filesystem::remove(path);
    REQUIRE(loaded.channels == 1);
    REQUIRE(loaded.source_channels == 2);
    REQUIRE(loaded.samples.size() == 4);
    REQUIRE(loaded.samples[1] == Approx(0.5f).margin(1e-3));
}

TEST_CASE("AudioFile clips out-of-range samples instead of wrapping", "[audio]") {
    const auto path = std::filesystem::temp_directory_path() / "audio_tool_clip.wav";
    audio_core::AudioFile::save_mono(path.string(), {1.5f, -1.5f, 0.5f}, 8000);
    const auto loaded = audio_core::AudioFile::load(path.string());
    std::filesystem::remove(path);
    REQUIRE(loaded.samples.size() == 3);
    REQUIRE(loaded.samples[0] > 0.99f);
    REQUIRE(loaded.samples[1] < -0.99f);
    REQUIRE(loaded.samples[2] == Approx(0.5f).margin(1e-3));
}

TEST_CASE("AudioFile rejects a sample count that does not fit the channel count", "[audio]") {
    audio_core::AudioData bad;
    bad.channels = 2;
    bad.samples = {0.1f, 0.2f, 0.3f};
    const auto path = std::filesystem::temp_directory_path() / "audio_tool_bad.wav";
    REQUIRE_THROWS(audio_core::AudioFile::save(path.string(), bad));
    REQUIRE_FALSE(std::filesystem::exists(path));
}
