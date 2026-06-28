#include "catch_common.hpp"

#include "audio_core/MarkerExports.hpp"

#include <filesystem>
#include <fstream>
#include <string>

TEST_CASE("Marker exports write CSV and Audition sidecars", "[markers][export]") {
    std::vector<audio_core::Marker> markers;
    audio_core::Marker m;
    m.type = audio_core::MarkerType::Pause;
    m.start_sec = 1.25;
    m.end_sec = 2.5;
    m.score = 0.42;
    markers.push_back(m);

    const auto dir = std::filesystem::temp_directory_path() / "audio_tool_exports";
    std::filesystem::create_directories(dir);
    const std::string stem = "episode";

    audio_core::export_markers(audio_core::MarkerExportFormat::Csv,
                               (dir / stem).string(), markers, 10.0, 48000);
    audio_core::export_markers(audio_core::MarkerExportFormat::Audition,
                               (dir / stem).string(), markers, 10.0, 48000);

    const auto csv_path = dir / (stem + ".markers.csv");
    const auto audition_path = dir / (stem + ".audition-markers.csv");
    REQUIRE(std::filesystem::exists(csv_path));
    REQUIRE(std::filesystem::exists(audition_path));

    std::ifstream csv(csv_path);
    std::string header;
    std::getline(csv, header);
    REQUIRE(header.find("start_sec") != std::string::npos);

    std::ifstream audition(audition_path);
    std::string audition_header;
    std::getline(audition, audition_header);
    REQUIRE(audition_header.find("Time Format") != std::string::npos);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST_CASE("parse_marker_export_formats accepts aliases", "[markers][export]") {
    const auto formats = audio_core::parse_marker_export_formats("csv,audition,fcpxml");
    REQUIRE(formats.size() == 3);
}
