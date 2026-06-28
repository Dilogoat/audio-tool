#include "catch_common.hpp"

#include "audio_core/AudioFile.hpp"
#include "audio_core/Formats.hpp"

#include <filesystem>
#include <sndfile.h>

namespace {

bool can_write_mp3() {
    SF_INFO info{};
    info.samplerate = 44100;
    info.channels = 1;
    info.format = SF_FORMAT_MPEG | SF_FORMAT_MPEG_LAYER_III;
    const auto path = std::filesystem::temp_directory_path() / "audio_tool_mp3_probe.tmp";
    SNDFILE* file = sf_open(path.string().c_str(), SFM_WRITE, &info);
    if (file) {
        sf_close(file);
        std::filesystem::remove(path);
        return true;
    }
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return false;
}

}  // namespace

TEST_CASE("SupportedFormats recognizes common extensions", "[formats]") {
    REQUIRE(audio_core::SupportedFormats::is_input_extension(".wav"));
    REQUIRE(audio_core::SupportedFormats::is_input_extension(".mp3"));
    REQUIRE(audio_core::SupportedFormats::is_input_extension(".opus"));
    REQUIRE_FALSE(audio_core::SupportedFormats::is_input_extension(".txt"));
}

TEST_CASE("AudioFile loads MP3 when libsndfile MPEG support is enabled", "[formats][audio]") {
    if (!can_write_mp3()) {
        SKIP("libsndfile was built without MP3 write support");
    }

    const auto path = std::filesystem::temp_directory_path() / "audio_tool_fmt_test.mp3";
    {
        SF_INFO info{};
        info.samplerate = 8000;
        info.channels = 1;
        info.format = SF_FORMAT_MPEG | SF_FORMAT_MPEG_LAYER_III;
        SNDFILE* file = sf_open(path.string().c_str(), SFM_WRITE, &info);
        REQUIRE(file != nullptr);

        std::vector<float> tone(static_cast<std::size_t>(info.samplerate), 0.15f);
        const sf_count_t written = sf_writef_float(file, tone.data(), static_cast<sf_count_t>(tone.size()));
        sf_close(file);
        REQUIRE(written == static_cast<sf_count_t>(tone.size()));
    }

    const auto data = audio_core::AudioFile::load(path.string());
    REQUIRE(data.sample_rate == 8000);
    REQUIRE(data.samples.size() == 8000);
    std::error_code ec;
    std::filesystem::remove(path, ec);
}
