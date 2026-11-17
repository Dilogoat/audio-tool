#include "catch_common.hpp"

#include "audio_core/AudioFile.hpp"
#include "audio_core/Pipeline.hpp"

#include <filesystem>
#include <vector>

namespace {

audio_core::AudioData make_audio_with_pause() {
    audio_core::AudioData a;
    a.sample_rate = 44100;
    a.channels = 1;
    a.samples.assign(static_cast<std::size_t>(a.sample_rate) * 3, 0.2f);
    for (int i = a.sample_rate; i < a.sample_rate * 2; ++i) {
        a.samples[static_cast<std::size_t>(i)] = 0.0f;
    }
    return a;
}

}  // namespace

TEST_CASE("Pipeline detects a pause and fills counts", "[pipeline]") {
    audio_core::Pipeline pipeline;
    const auto result = pipeline.analyze(make_audio_with_pause(), false);

    REQUIRE(result.duration_sec == Approx(3.0).margin(0.05));
    REQUIRE(result.pause_count >= 1);
    REQUIRE_FALSE(result.cleaned_produced);
}

TEST_CASE("Pipeline produces cleaned audio when requested", "[pipeline]") {
    audio_core::Pipeline pipeline;
    const auto result = pipeline.analyze(make_audio_with_pause(), true);

    REQUIRE(result.cleaned_produced);
    REQUIRE(result.cleaned.size() > 0);
}

TEST_CASE("Pipeline respects disabled detectors", "[pipeline]") {
    audio_core::PipelineConfig cfg;
    cfg.detect_pauses = false;
    cfg.detect_clicks = false;
    cfg.detect_breaths = false;

    audio_core::Pipeline pipeline(cfg);
    const auto result = pipeline.analyze(make_audio_with_pause(), false);
    REQUIRE(result.markers.all().empty());
}

TEST_CASE("PipelineConfig JSON round trip", "[pipeline][config]") {
    audio_core::PipelineConfig cfg;
    cfg.pause.rms_threshold = 0.0321;
    cfg.detect_breaths = false;

    const auto path = std::filesystem::temp_directory_path() / "audio_tool_pipeline_cfg.json";
    cfg.to_json_file(path.string());
    const auto loaded = audio_core::PipelineConfig::from_json_file(path.string());

    REQUIRE(loaded.pause.rms_threshold == Approx(0.0321));
    REQUIRE_FALSE(loaded.detect_breaths);
    std::filesystem::remove(path);
}

TEST_CASE("save_report_json writes a valid file", "[pipeline][report]") {
    audio_core::Pipeline pipeline;
    const auto result = pipeline.analyze(make_audio_with_pause(), false);

    const auto path = std::filesystem::temp_directory_path() / "audio_tool_report.json";
    audio_core::save_report_json(path.string(), "input.wav", result);
    REQUIRE(std::filesystem::exists(path));
    REQUIRE(std::filesystem::file_size(path) > 0);
    std::filesystem::remove(path);
}

TEST_CASE("Pipeline returns markers in chronological order", "[pipeline]") {
    const int sr = 44100;
    std::vector<float> samples(static_cast<std::size_t>(sr) * 4, 0.02f);
    // Pause at 1-2 s, click at 3 s.
    for (int i = sr; i < 2 * sr; ++i) samples[static_cast<std::size_t>(i)] = 0.0f;
    for (int i = -5; i <= 5; ++i) samples[static_cast<std::size_t>(3 * sr + i)] = 0.95f;
    audio_core::AudioData audio;
    audio.sample_rate = sr;
    audio.samples = samples;
    audio_core::PipelineConfig cfg;
    cfg.transient.spike_ratio = 2.0;
    const auto result = audio_core::Pipeline(cfg).analyze(audio, false);
    const auto& all = result.markers.all();
    REQUIRE(all.size() >= 2);
    for (std::size_t i = 1; i < all.size(); ++i) {
        REQUIRE(all[i - 1].start_sec <= all[i].start_sec);
    }
}
