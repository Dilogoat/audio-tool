#include "catch_common.hpp"
#include <algorithm>
#include <iterator>

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

namespace {

std::string read_all(const std::filesystem::path& p) {
    // Text mode: sidecars are written with CRLF on Windows, which every target
    // app accepts; compare content with normalised line endings.
    std::ifstream in(p);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

audio_core::Marker marker_at(audio_core::MarkerType type, double start, double end) {
    audio_core::Marker m;
    m.type = type;
    m.start_sec = start;
    m.end_sec = end;
    m.score = 1.0;
    return m;
}

struct TempDir {
    std::filesystem::path path;
    explicit TempDir(const std::string& name) : path(std::filesystem::temp_directory_path() / name) {
        std::filesystem::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

}  // namespace

TEST_CASE("Timecodes never overflow their fields when rounding", "[markers][export]") {
    TempDir dir("audio_tool_tc_overflow");
    // 23.989116 s at 29.97 fps was written as "00:00:23:30" (frame 30 of a
    // 0-29 range); 59.9996 s was written as "0:00:59.1000".
    const std::vector<audio_core::Marker> markers = {
        marker_at(audio_core::MarkerType::Pause, 23.339410, 23.989116),
        marker_at(audio_core::MarkerType::Pause, 59.9996, 60.5),
    };
    const auto base = (dir.path / "ep").string();
    audio_core::export_markers(audio_core::MarkerExportFormat::Csv, base, markers, 100.0);
    audio_core::export_markers(audio_core::MarkerExportFormat::Audition, base, markers, 100.0);

    const auto csv = read_all(dir.path / "ep.markers.csv");
    REQUIRE(csv.find(":30,") == std::string::npos);
    // 23.989116 s * 29.97 = 718.95 -> frame 719 -> 23 s + 29 f (base 30).
    REQUIRE(csv.find("00:00:23:29") != std::string::npos);

    const auto audition = read_all(dir.path / "ep.audition-markers.csv");
    REQUIRE(audition.find(".1000") == std::string::npos);
    REQUIRE(audition.find("1:00.000") != std::string::npos);
}

TEST_CASE("Audition export is tab-separated", "[markers][export]") {
    TempDir dir("audio_tool_audition_tabs");
    audio_core::export_markers(audio_core::MarkerExportFormat::Audition, (dir.path / "ep").string(),
                               {marker_at(audio_core::MarkerType::Breath, 3725.5, 3726.0)});
    const auto text = read_all(dir.path / "ep.audition-markers.csv");
    REQUIRE(text.rfind("Name\tStart\tDuration\tTime Format\tType\tDescription\n", 0) == 0);
    REQUIRE(text.find("breath_1\t1:02:05.500\t0:00.500\tdecimal\tCue\t") != std::string::npos);
}

TEST_CASE("FCP XML uses an NTSC timebase that matches its frame counts", "[markers][export]") {
    TempDir dir("audio_tool_fcpxml");
    // Real case from validation: a breath at 2971.18 s was written as frame
    // 89046 under <timebase>29</timebase>, which an NLE places ~99 s late.
    audio_core::export_markers(audio_core::MarkerExportFormat::FcpXml, (dir.path / "ep").string(),
                               {marker_at(audio_core::MarkerType::Breath, 2971.18, 2971.35)}, 3200.0);
    const auto xml = read_all(dir.path / "ep.markers.fcpxml");
    REQUIRE(xml.find("<timebase>30</timebase><ntsc>TRUE</ntsc>") != std::string::npos);
    REQUIRE(xml.find("<in>89046</in>") != std::string::npos);  // 89046 / 29.97 = 2971.18 s
    // Markers belong to the sequence (before <media>), not inside <track>.
    REQUIRE(xml.find("<marker>") < xml.find("<media>"));
}

TEST_CASE("FCP XML keeps integer rates non-NTSC", "[markers][export]") {
    TempDir dir("audio_tool_fcpxml25");
    audio_core::export_markers(audio_core::MarkerExportFormat::FcpXml, (dir.path / "ep").string(),
                               {marker_at(audio_core::MarkerType::Pause, 2.0, 3.0)}, 10.0, 48000, 25.0);
    const auto xml = read_all(dir.path / "ep.markers.fcpxml");
    REQUIRE(xml.find("<timebase>25</timebase><ntsc>FALSE</ntsc>") != std::string::npos);
    REQUIRE(xml.find("<in>50</in>") != std::string::npos);
}

TEST_CASE("EDL events have real record times and Resolve marker lines", "[markers][export]") {
    TempDir dir("audio_tool_edl");
    audio_core::export_markers(audio_core::MarkerExportFormat::Edl, (dir.path / "ep").string(),
                               {marker_at(audio_core::MarkerType::Click, 10.0, 10.5)}, 60.0, 48000, 25.0);
    const auto edl = read_all(dir.path / "ep.markers.edl");
    REQUIRE(edl.find("00:00:10:00 00:00:10:13 00:00:10:00 00:00:10:13") != std::string::npos);
    REQUIRE(edl.find(" |C:ResolveColorRed |M:click_1 |D:13") != std::string::npos);
    REQUIRE(edl.find("00:00:00:00 00:00:00:00") == std::string::npos);
}

TEST_CASE("Reaper export uses region IDs and leaves Color for colours", "[markers][export]") {
    TempDir dir("audio_tool_reaper");
    audio_core::export_markers(audio_core::MarkerExportFormat::Reaper, (dir.path / "ep").string(),
                               {marker_at(audio_core::MarkerType::Pause, 61.25, 62.0)});
    const auto csv = read_all(dir.path / "ep.reaper-markers.csv");
    REQUIRE(csv.find("R1,pause_1,0:01:01.250,0:01:02.000,0:00:00.750,\n") != std::string::npos);
}

TEST_CASE("Audacity labels keep sub-millisecond precision on long files", "[markers][export]") {
    TempDir dir("audio_tool_labels");
    audio_core::export_markers(audio_core::MarkerExportFormat::Audacity, (dir.path / "ep").string(),
                               {marker_at(audio_core::MarkerType::Breath, 2971.183412, 2971.351)});
    const auto labels = read_all(dir.path / "ep.labels.txt");
    REQUIRE(labels == "2971.183412\t2971.351000\tbreath\n");
}
