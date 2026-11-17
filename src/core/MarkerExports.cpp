#include "audio_core/MarkerExports.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace audio_core {

namespace {

std::string pad2(int v) {
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << v;
    return oss.str();
}

std::string pad3(int v) {
    std::ostringstream oss;
    oss << std::setw(3) << std::setfill('0') << v;
    return oss.str();
}

// Every timestamp is rounded once as a total count (milliseconds or frames)
// and then split into fields. Rounding a fractional field on its own lets it
// overflow, e.g. 59.9996 s -> "0:00:59.1000" or a frame number equal to fps.
std::string seconds_to_hms(double sec) {
    long long total_ms = std::llround((std::max)(0.0, sec) * 1000.0);
    const long long ms = total_ms % 1000;
    total_ms /= 1000;
    const long long seconds = total_ms % 60;
    total_ms /= 60;
    const long long minutes = total_ms % 60;
    const long long hours = total_ms / 60;
    std::ostringstream oss;
    oss << hours << ':' << pad2(static_cast<int>(minutes)) << ':' << pad2(static_cast<int>(seconds)) << '.'
        << pad3(static_cast<int>(ms));
    return oss.str();
}

// Audition's "decimal" time format: m:ss.mmm, or h:mm:ss.mmm past an hour.
std::string seconds_to_audition(double sec) {
    long long total_ms = std::llround((std::max)(0.0, sec) * 1000.0);
    const long long ms = total_ms % 1000;
    total_ms /= 1000;
    const long long seconds = total_ms % 60;
    total_ms /= 60;
    const long long minutes = total_ms % 60;
    const long long hours = total_ms / 60;
    std::ostringstream oss;
    if (hours > 0) {
        oss << hours << ':' << pad2(static_cast<int>(minutes));
    } else {
        oss << minutes;
    }
    oss << ':' << pad2(static_cast<int>(seconds)) << '.' << pad3(static_cast<int>(ms));
    return oss.str();
}

// Nominal (integer) frame base for a rate: 29.97 -> 30, 23.976 -> 24, 25 -> 25.
int timecode_base(double fps) { return (std::max)(1, static_cast<int>(std::lround(fps))); }

bool is_ntsc_rate(double fps) { return std::fabs(fps - timecode_base(fps)) > 1e-3; }

long long seconds_to_frames(double sec, double fps) {
    return static_cast<long long>(std::llround((std::max)(0.0, sec) * fps));
}

// Non-drop-frame SMPTE timecode. At NTSC rates (29.97) frames are counted at
// the real rate but labelled with a base-30 counter, which is exactly how an
// NLE reads NDF timecode back, so markers land at the right real time.
std::string frames_to_smpte(long long total, double fps) {
    const int base = timecode_base(fps);
    const long long ff = total % base;
    total /= base;
    const long long ss = total % 60;
    total /= 60;
    const long long mm = total % 60;
    const long long hh = total / 60;
    std::ostringstream oss;
    oss << pad2(static_cast<int>(hh)) << ':' << pad2(static_cast<int>(mm)) << ':'
        << pad2(static_cast<int>(ss)) << ':' << pad2(static_cast<int>(ff));
    return oss.str();
}

std::string seconds_to_smpte(double sec, double fps) { return frames_to_smpte(seconds_to_frames(sec, fps), fps); }

const char* resolve_color(MarkerType type) {
    switch (type) {
        case MarkerType::Pause:
            return "ResolveColorBlue";
        case MarkerType::Click:
            return "ResolveColorRed";
        case MarkerType::Breath:
            return "ResolveColorYellow";
        default:
            return "ResolveColorGreen";
    }
}

std::string marker_label(const Marker& m, int index) {
    return marker_type_to_string(m.type) + '_' + std::to_string(index + 1);
}

void write_or_throw(const std::string& path, const std::string& body) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to write marker export: " + path);
    }
    out << body;
}

}  // namespace

std::string marker_export_extension(MarkerExportFormat format) {
    switch (format) {
        case MarkerExportFormat::Json:
            return ".markers.json";
        case MarkerExportFormat::Csv:
            return ".markers.csv";
        case MarkerExportFormat::Edl:
            return ".markers.edl";
        case MarkerExportFormat::FcpXml:
            return ".markers.fcpxml";
        case MarkerExportFormat::Reaper:
            return ".reaper-markers.csv";
        case MarkerExportFormat::Audition:
            return ".audition-markers.csv";
        case MarkerExportFormat::Audacity:
            return ".labels.txt";
    }
    return ".markers.txt";
}

void export_markers(MarkerExportFormat format, const std::string& base_path,
                    const std::vector<Marker>& markers, double duration_sec, int sample_rate,
                    double fps) {
    (void)sample_rate;
    const std::string path = base_path + marker_export_extension(format);

    switch (format) {
        case MarkerExportFormat::Json: {
            Markers container;
            container.merge(markers);
            container.save_json(path);
            return;
        }
        case MarkerExportFormat::Csv: {
            std::ostringstream out;
            out << "id,type,label,start_sec,end_sec,duration_sec,start_smpte,end_smpte,score\n";
            for (std::size_t i = 0; i < markers.size(); ++i) {
                const auto& m = markers[i];
                const double dur = (std::max)(0.0, m.end_sec - m.start_sec);
                out << (i + 1) << ','
                    << marker_type_to_string(m.type) << ','
                    << marker_label(m, static_cast<int>(i)) << ','
                    << std::fixed << std::setprecision(6) << m.start_sec << ','
                    << m.end_sec << ','
                    << dur << ','
                    << seconds_to_smpte(m.start_sec, fps) << ','
                    << seconds_to_smpte(m.end_sec, fps) << ','
                    << m.score << '\n';
            }
            write_or_throw(path, out.str());
            return;
        }
        case MarkerExportFormat::Edl: {
            // CMX3600-style marker list. Record in/out mirror the source times on
            // a timeline starting at 00:00:00:00 (zero-length record events are
            // rejected by some NLEs); the "|C: |M: |D:" line is the form
            // DaVinci Resolve reads for "Import > Timeline Markers from EDL".
            std::ostringstream out;
            out << "TITLE: audio-tool markers\n";
            out << "FCM: NON-DROP FRAME\n\n";
            for (std::size_t i = 0; i < markers.size(); ++i) {
                const auto& m = markers[i];
                const long long in_f = seconds_to_frames(m.start_sec, fps);
                const long long out_f = (std::max)(in_f + 1, seconds_to_frames(m.end_sec, fps));
                const std::string tc_in = frames_to_smpte(in_f, fps);
                const std::string tc_out = frames_to_smpte(out_f, fps);
                out << pad3(static_cast<int>(i + 1)) << "  AX       V     C        "
                    << tc_in << ' ' << tc_out << ' ' << tc_in << ' ' << tc_out << '\n';
                out << " |C:" << resolve_color(m.type) << " |M:" << marker_label(m, static_cast<int>(i))
                    << " |D:" << (out_f - in_f) << '\n';
                out << "* FROM CLIP NAME: " << marker_label(m, static_cast<int>(i)) << '\n';
                out << "* COMMENT: type=" << marker_type_to_string(m.type)
                    << " score=" << m.score << "\n\n";
            }
            if (markers.empty()) {
                out << "* No markers detected\n";
            }
            if (duration_sec > 0.0) {
                out << "* SOURCE DURATION: " << seconds_to_hms(duration_sec) << '\n';
            }
            write_or_throw(path, out.str());
            return;
        }
        case MarkerExportFormat::FcpXml: {
            // FCP7 xmeml: 29.97 is expressed as timebase 30 + ntsc TRUE, and
            // frame counts are taken at the real rate. Markers are children of
            // <sequence> (xmeml does not define markers on <track>).
            const long long seq_frames = seconds_to_frames(
                duration_sec > 0.0 ? duration_sec : (markers.empty() ? 1.0 : markers.back().end_sec + 1.0),
                fps);
            std::ostringstream out;
            out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            out << "<!DOCTYPE xmeml>\n";
            out << "<xmeml version=\"4\">\n";
            out << "  <sequence>\n";
            out << "    <name>audio-tool markers</name>\n";
            out << "    <duration>" << seq_frames << "</duration>\n";
            out << "    <rate><timebase>" << timecode_base(fps) << "</timebase><ntsc>"
                << (is_ntsc_rate(fps) ? "TRUE" : "FALSE") << "</ntsc></rate>\n";
            for (std::size_t i = 0; i < markers.size(); ++i) {
                const auto& m = markers[i];
                const long long in_frame = seconds_to_frames(m.start_sec, fps);
                const long long out_frame = (std::max)(in_frame + 1LL, seconds_to_frames(m.end_sec, fps));
                out << "    <marker>\n";
                out << "      <name>" << marker_label(m, static_cast<int>(i)) << "</name>\n";
                out << "      <comment>" << marker_type_to_string(m.type) << "</comment>\n";
                out << "      <in>" << in_frame << "</in>\n";
                out << "      <out>" << out_frame << "</out>\n";
                out << "    </marker>\n";
            }
            out << "    <media>\n";
            out << "      <audio>\n";
            out << "        <track/>\n";
            out << "      </audio>\n";
            out << "    </media>\n";
            out << "  </sequence>\n";
            out << "</xmeml>\n";
            write_or_throw(path, out.str());
            return;
        }
        case MarkerExportFormat::Reaper: {
            // Region/Marker Manager CSV: "#" is R<n> for regions; Color is a hex
            // RGB value or empty (it previously carried the marker type). Times
            // are h:mm:ss.mmm so they parse as time whatever the ruler format.
            std::ostringstream out;
            out << "#,Name,Start,End,Length,Color\n";
            for (std::size_t i = 0; i < markers.size(); ++i) {
                const auto& m = markers[i];
                const double dur = (std::max)(0.0, m.end_sec - m.start_sec);
                out << 'R' << (i + 1) << ','
                    << marker_label(m, static_cast<int>(i)) << ','
                    << seconds_to_hms(m.start_sec) << ','
                    << seconds_to_hms(m.end_sec) << ','
                    << seconds_to_hms(dur) << ",\n";
            }
            write_or_throw(path, out.str());
            return;
        }
        case MarkerExportFormat::Audition: {
            // Audition's marker list (Import Markers) is tab-separated despite
            // the .csv extension.
            std::ostringstream out;
            out << "Name\tStart\tDuration\tTime Format\tType\tDescription\n";
            for (std::size_t i = 0; i < markers.size(); ++i) {
                const auto& m = markers[i];
                const double dur = (std::max)(0.0, m.end_sec - m.start_sec);
                out << marker_label(m, static_cast<int>(i)) << '\t'
                    << seconds_to_audition(m.start_sec) << '\t'
                    << seconds_to_audition(dur) << '\t'
                    << "decimal\tCue\t"
                    << marker_type_to_string(m.type) << " score=" << m.score << '\n';
            }
            write_or_throw(path, out.str());
            return;
        }
        case MarkerExportFormat::Audacity: {
            Markers container;
            container.merge(markers);
            container.save_audacity_labels(path);
            return;
        }
    }
}

void export_all_marker_formats(const std::string& out_dir, const std::string& stem,
                               const std::vector<Marker>& markers, double duration_sec,
                               int sample_rate) {
    const std::vector<MarkerExportFormat> formats = {
        MarkerExportFormat::Csv,
        MarkerExportFormat::Edl,
        MarkerExportFormat::FcpXml,
        MarkerExportFormat::Reaper,
        MarkerExportFormat::Audition,
        MarkerExportFormat::Audacity,
    };
    for (const auto fmt : formats) {
        const auto path = (std::filesystem::path(out_dir) / (stem + marker_export_extension(fmt))).string();
        const std::string base = path.substr(0, path.size() - marker_export_extension(fmt).size());
        export_markers(fmt, base, markers, duration_sec, sample_rate);
    }
}

std::vector<MarkerExportFormat> parse_marker_export_formats(const std::string& csv_list) {
    std::string lowered = csv_list;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowered == "all" || lowered == "*") {
        return {MarkerExportFormat::Csv, MarkerExportFormat::Edl, MarkerExportFormat::FcpXml,
                MarkerExportFormat::Reaper, MarkerExportFormat::Audition,
                MarkerExportFormat::Audacity};
    }

    std::vector<MarkerExportFormat> out;
    std::size_t pos = 0;
    while (pos <= lowered.size()) {
        const std::size_t comma = lowered.find(',', pos);
        const std::string token = lowered.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        if (token == "json") {
            out.push_back(MarkerExportFormat::Json);
        } else if (token == "csv") {
            out.push_back(MarkerExportFormat::Csv);
        } else if (token == "edl") {
            out.push_back(MarkerExportFormat::Edl);
        } else if (token == "fcpxml" || token == "fcp" || token == "xmeml" || token == "premiere") {
            out.push_back(MarkerExportFormat::FcpXml);
        } else if (token == "reaper") {
            out.push_back(MarkerExportFormat::Reaper);
        } else if (token == "audition" || token == "adobe") {
            out.push_back(MarkerExportFormat::Audition);
        } else if (token == "audacity" || token == "labels") {
            out.push_back(MarkerExportFormat::Audacity);
        } else if (!token.empty()) {
            throw std::runtime_error("unknown marker export format: " + token);
        }
        if (comma == std::string::npos) {
            break;
        }
        pos = comma + 1;
    }
    return out;
}

}  // namespace audio_core
