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

std::string seconds_to_hms(double sec) {
    if (sec < 0.0) {
        sec = 0.0;
    }
    const int hours = static_cast<int>(sec / 3600.0);
    sec -= hours * 3600.0;
    const int minutes = static_cast<int>(sec / 60.0);
    sec -= minutes * 60.0;
    const int seconds = static_cast<int>(sec);
    const int millis = static_cast<int>(std::round((sec - seconds) * 1000.0));
    std::ostringstream oss;
    oss << hours << ':' << pad2(minutes) << ':' << pad2(seconds) << '.' << pad3(millis);
    return oss.str();
}

std::string seconds_to_smpte(double sec, double fps) {
    if (sec < 0.0) {
        sec = 0.0;
    }
    const int hours = static_cast<int>(sec / 3600.0);
    sec -= hours * 3600.0;
    const int minutes = static_cast<int>(sec / 60.0);
    sec -= minutes * 60.0;
    const int seconds = static_cast<int>(sec);
    const int frames = static_cast<int>(std::round((sec - seconds) * fps));
    std::ostringstream oss;
    oss << pad2(hours) << ':' << pad2(minutes) << ':' << pad2(seconds) << ':' << pad2(frames);
    return oss.str();
}

long long seconds_to_frames(double sec, double fps) {
    return static_cast<long long>(std::llround((std::max)(0.0, sec) * fps));
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
            std::ostringstream out;
            out << "TITLE: audio-tool markers\n";
            out << "FCM: NON-DROP FRAME\n\n";
            int event = 1;
            for (std::size_t i = 0; i < markers.size(); ++i) {
                const auto& m = markers[i];
                const std::string src_in = seconds_to_smpte(m.start_sec, fps);
                const std::string src_out = seconds_to_smpte(m.end_sec, fps);
                out << pad3(event++) << "  AX       V     C        "
                    << src_in << ' ' << src_out << " 00:00:00:00 00:00:00:00\n";
                out << "* FROM CLIP NAME: " << marker_label(m, static_cast<int>(i)) << '\n';
                out << "* COMMENT: type=" << marker_type_to_string(m.type)
                    << " score=" << m.score << '\n';
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
            out << "    <rate><timebase>" << static_cast<int>(fps)
                << "</timebase><ntsc>FALSE</ntsc></rate>\n";
            out << "    <media>\n";
            out << "      <audio>\n";
            out << "        <track>\n";
            for (std::size_t i = 0; i < markers.size(); ++i) {
                const auto& m = markers[i];
                const long long in_frame = seconds_to_frames(m.start_sec, fps);
                const long long out_frame = (std::max)(in_frame + 1LL, seconds_to_frames(m.end_sec, fps));
                out << "          <marker>\n";
                out << "            <name>" << marker_label(m, static_cast<int>(i)) << "</name>\n";
                out << "            <comment>" << marker_type_to_string(m.type) << "</comment>\n";
                out << "            <in>" << in_frame << "</in>\n";
                out << "            <out>" << out_frame << "</out>\n";
                out << "          </marker>\n";
            }
            out << "        </track>\n";
            out << "      </audio>\n";
            out << "    </media>\n";
            out << "  </sequence>\n";
            out << "</xmeml>\n";
            write_or_throw(path, out.str());
            return;
        }
        case MarkerExportFormat::Reaper: {
            std::ostringstream out;
            out << "#,Name,Start,End,Length,Color\n";
            for (std::size_t i = 0; i < markers.size(); ++i) {
                const auto& m = markers[i];
                const double dur = (std::max)(0.0, m.end_sec - m.start_sec);
                out << (i + 1) << ','
                    << marker_label(m, static_cast<int>(i)) << ','
                    << std::fixed << std::setprecision(6) << m.start_sec << ','
                    << m.end_sec << ','
                    << dur << ','
                    << marker_type_to_string(m.type) << '\n';
            }
            write_or_throw(path, out.str());
            return;
        }
        case MarkerExportFormat::Audition: {
            std::ostringstream out;
            out << "Name,Start,Duration,Time Format,Type,Description\n";
            for (std::size_t i = 0; i < markers.size(); ++i) {
                const auto& m = markers[i];
                const double dur = (std::max)(0.0, m.end_sec - m.start_sec);
                out << marker_label(m, static_cast<int>(i)) << ','
                    << seconds_to_hms(m.start_sec) << ','
                    << seconds_to_hms(dur) << ','
                    << "decimal,Cue,"
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
