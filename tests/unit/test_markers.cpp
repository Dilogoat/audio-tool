#include "catch_common.hpp"

#include "audio_core/Markers.hpp"

#include <filesystem>
#include <fstream>

TEST_CASE("Markers JSON round trip", "[markers]") {
    audio_core::Markers markers;
    audio_core::Marker m;
    m.type = audio_core::MarkerType::Click;
    m.start_sec = 1.0;
    m.end_sec = 1.05;
    m.score = 0.9;
    markers.add(m);

    const auto path = std::filesystem::temp_directory_path() / "audio_tool_markers_test.json";
    markers.save_json(path.string());
    const auto loaded = audio_core::Markers::load_json(path.string());
    REQUIRE(loaded.size() == 1);
    REQUIRE(loaded.front().type == audio_core::MarkerType::Click);
    REQUIRE(loaded.front().score == Approx(0.9));
    std::filesystem::remove(path);
}
