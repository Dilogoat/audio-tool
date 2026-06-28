#include "catch_common.hpp"

#include "audio_core/CompositeScore.hpp"
#include "audio_core/Markers.hpp"

TEST_CASE("Composite score weights clicks highest", "[composite]") {
    std::vector<audio_core::Marker> markers;
    audio_core::Marker click;
    click.type = audio_core::MarkerType::Click;
    click.start_sec = 1.0;
    click.end_sec = 1.1;
    click.score = 1.0;
    markers.push_back(click);

    audio_core::Marker breath;
    breath.type = audio_core::MarkerType::Breath;
    breath.start_sec = 2.0;
    breath.end_sec = 2.2;
    breath.score = 1.0;
    markers.push_back(breath);

    audio_core::CompositeScore scorer;
    const auto frames = scorer.compute(markers, 5.0);
    REQUIRE(!frames.empty());

    double click_peak = 0.0;
    double breath_peak = 0.0;
    for (const auto& f : frames) {
        if (f.time_sec >= 1.0 && f.time_sec <= 1.1) {
            click_peak = std::max(click_peak, f.score);
        }
        if (f.time_sec >= 2.0 && f.time_sec <= 2.2) {
            breath_peak = std::max(breath_peak, f.score);
        }
    }
    REQUIRE(click_peak > breath_peak);
}
