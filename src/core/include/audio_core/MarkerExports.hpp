#pragma once

#include "audio_core/Markers.hpp"

#include <string>
#include <vector>

namespace audio_core {

enum class MarkerExportFormat {
    Json,
    Csv,
    Edl,
    FcpXml,
    Reaper,
    Audition,
    Audacity,
};

std::string marker_export_extension(MarkerExportFormat format);

// Writes one marker sidecar next to `base_path` (stem used, extension chosen by format).
void export_markers(MarkerExportFormat format, const std::string& base_path,
                    const std::vector<Marker>& markers, double duration_sec = 0.0,
                    int sample_rate = 44100, double fps = 29.97);

// Writes every supported sidecar format into `out_dir` using `stem` as the basename.
void export_all_marker_formats(const std::string& out_dir, const std::string& stem,
                               const std::vector<Marker>& markers, double duration_sec = 0.0,
                               int sample_rate = 44100);

std::vector<MarkerExportFormat> parse_marker_export_formats(const std::string& csv_list);

}  // namespace audio_core
