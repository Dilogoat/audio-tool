#include "catch_common.hpp"

#include "audio_core/Editor.hpp"
#include "audio_core/Markers.hpp"

#include <vector>

TEST_CASE("apply_cuts removes a pause region", "[editor]") {
    const int sr = 44100;
    std::vector<float> samples(static_cast<std::size_t>(sr) * 3, 0.2f);

    std::vector<audio_core::Marker> markers;
    audio_core::Marker m;
    m.type = audio_core::MarkerType::Pause;
    m.start_sec = 1.0;
    m.end_sec = 2.0;
    markers.push_back(m);

    audio_core::EditConfig cfg;
    cfg.cut_pauses = true;
    cfg.crossfade_ms = 5.0;

    const auto result = audio_core::apply_cuts(samples, sr, markers, cfg);
    REQUIRE(result.regions_cut == 1);
    REQUIRE(result.removed_sec == Approx(1.0).margin(0.05));
    REQUIRE(result.edited_sec < result.original_sec);
    REQUIRE(result.samples.size() < samples.size());
}

TEST_CASE("apply_cuts is a no-op when nothing selected", "[editor]") {
    const int sr = 44100;
    std::vector<float> samples(static_cast<std::size_t>(sr), 0.2f);

    std::vector<audio_core::Marker> markers;
    audio_core::Marker m;
    m.type = audio_core::MarkerType::Pause;
    m.start_sec = 0.2;
    m.end_sec = 0.5;
    markers.push_back(m);

    audio_core::EditConfig cfg;
    cfg.cut_pauses = false;  // not selected

    const auto result = audio_core::apply_cuts(samples, sr, markers, cfg);
    REQUIRE(result.regions_cut == 0);
    REQUIRE(result.samples.size() == samples.size());
}

TEST_CASE("apply_cuts merges overlapping regions", "[editor]") {
    const int sr = 8000;
    std::vector<float> samples(static_cast<std::size_t>(sr) * 4, 0.1f);

    std::vector<audio_core::Marker> markers;
    audio_core::Marker a;
    a.type = audio_core::MarkerType::Breath;
    a.start_sec = 1.0;
    a.end_sec = 2.0;
    audio_core::Marker b;
    b.type = audio_core::MarkerType::Breath;
    b.start_sec = 1.5;
    b.end_sec = 2.5;
    markers.push_back(a);
    markers.push_back(b);

    audio_core::EditConfig cfg;
    cfg.cut_breaths = true;
    cfg.crossfade_ms = 0.0;

    const auto result = audio_core::apply_cuts(samples, sr, markers, cfg);
    REQUIRE(result.regions_cut == 1);
    REQUIRE(result.removed_sec == Approx(1.5).margin(0.05));
}
