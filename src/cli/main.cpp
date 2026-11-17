#include "audio_core/AudioFile.hpp"
#include "audio_core/Editor.hpp"
#include "audio_core/Formats.hpp"
#include "audio_core/Logger.hpp"
#include "audio_core/MarkerExports.hpp"
#include "audio_core/Pipeline.hpp"
#include "audio_core/Session.hpp"
#include "audio_core/SpectralCleaner.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;
using audio_core::Logger;

namespace {

constexpr const char* kVersion = "0.3.0";

const std::vector<std::string> kAudioExts = audio_core::SupportedFormats::input_extensions();

struct Options {
    std::string command;
    std::vector<std::string> inputs;
    std::string out_dir;
    std::string config_path;
    std::string log_file;
    audio_core::LogLevel log_level = audio_core::LogLevel::Info;
    bool clean = false;
    bool recursive = false;
    bool force = false;
    bool dry_run = false;
    bool disable_pauses = false;
    bool disable_clicks = false;
    bool disable_breaths = false;

    bool cut = false;
    bool cut_pauses = false;
    bool cut_clicks = false;
    bool cut_breaths = false;
    double crossfade_ms = 5.0;
    bool labels = false;
    bool export_markers_all = false;
    std::vector<audio_core::MarkerExportFormat> export_marker_formats;
    bool once = false;
    int poll_ms = 1000;
    int settle_ms = 3000;
};

void print_usage() {
    std::cout <<
        "Audio Cleanup & Analysis Tool - CLI v" << kVersion << "\n\n"
        "USAGE:\n"
        "  audio_tool_cli <command> [paths...] [options]\n\n"
        "COMMANDS:\n"
        "  analyze <file...>     Analyze one or more audio files\n"
        "  batch <dir...>        Analyze every audio file in a folder\n"
        "  watch <dir>           Watch a folder and auto-process new audio files\n"
        "  init-config <file>    Write a default config/preset JSON\n"
        "  help                  Show this help\n\n"
        "OPTIONS:\n"
        "  --out-dir <dir>       Output directory (default: <input_dir>/audio-tool-out)\n"
        "  --config <file>       Load detector settings from a preset JSON\n"
        "  --clean               Also render a cleaned WAV (attenuates artifacts)\n"
        "  --markers-only        Only write markers + report (default)\n"
        "  --cut <types>         Physically remove regions, re-stitch, write edited WAV.\n"
        "                        Comma list of: pauses,clicks,breaths (e.g. --cut pauses,breaths)\n"
        "  --crossfade-ms <ms>   Crossfade length at cut joins (default: 5)\n"
        "  --labels              Write an Audacity label track (.labels.txt)\n"
        "  --export-markers <list>  Export marker sidecars: all | csv,edl,fcpxml,reaper,audition,audacity\n"
        "  --once                Watch mode: scan once and exit (great for schedulers/tests)\n"
        "  --poll-ms <ms>        Watch mode: polling interval (default: 1000)\n"
        "  --settle-ms <ms>      Watch mode: wait until file age is at least this old (default: 3000)\n"
        "  -r, --recursive       Recurse into subfolders (batch)\n"
        "  --force               Overwrite existing generated outputs\n"
        "  --dry-run             Show what would happen, write nothing\n"
        "  --no-pauses           Disable pause detection\n"
        "  --no-clicks           Disable click/plosive detection\n"
        "  --no-breaths          Disable breath detection\n"
        "  --log-level <lvl>     error | warn | info | debug (default: info)\n"
        "  --log-file <path>     Also write logs to a file\n"
        "  -h, --help            Show this help\n\n"
        "OUTPUTS (per input <name>.<ext>):\n"
        "  <name>.markers.json   Detected events timeline\n"
        "  <name>.report.json    Per-file summary\n"
        "  <name>.cleaned.wav    Cleaned audio (only with --clean)\n"
        "  <name>.edited.wav     Cut + re-stitched audio (only with --cut)\n"
        "  <name>.labels.txt     Audacity labels (only with --labels)\n"
        "  <name>.markers.csv    Universal spreadsheet export\n"
        "  <name>.markers.edl    CMX-style EDL for Resolve/Premiere\n"
        "  <name>.markers.fcpxml FCP7 XML markers for Premiere/Audition\n"
        "  <name>.reaper-markers.csv  Reaper marker/region CSV\n"
        "  <name>.audition-markers.csv Adobe Audition marker CSV\n"
        "  <name>.audiotool.json Session file for GUI review mode\n"
        "  batch-summary.json    Whole-run summary (batch)\n\n"
        "EXIT CODES: 0 ok, 1 partial failure, 2 bad args, 3 fatal\n";
}

bool is_audio_file(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return std::find(kAudioExts.begin(), kAudioExts.end(), ext) != kAudioExts.end();
}

// Parses CLI args. Returns false on a parse error (caller exits with code 2).
bool parse_args(int argc, char** argv, Options& opt, std::string& error) {
    if (argc < 2) {
        error = "no command given";
        return false;
    }

    int i = 1;
    opt.command = argv[i++];

    // Consumes the token *after* the current flag (argv[i] is the flag itself).
    auto need_value = [&](const char* flag) -> std::string {
        if (i + 1 >= argc) {
            error = std::string("missing value for ") + flag;
            return {};
        }
        return argv[++i];
    };

    // Numeric flags: report which flag had a bad value (exit 2) instead of
    // letting std::stoi's "invalid stoi argument" escape.
    auto need_number = [&](const char* flag, auto parse) -> decltype(parse(std::string{})) {
        const std::string v = need_value(flag);
        if (!error.empty()) {
            return {};
        }
        try {
            std::size_t used = 0;
            const auto out = parse(v, &used);
            if (used != v.size()) {
                throw std::invalid_argument(v);
            }
            return out;
        } catch (const std::exception&) {
            error = std::string("invalid number for ") + flag + ": " + v;
            return {};
        }
    };
    auto as_int = [](const std::string& v, std::size_t* used = nullptr) { return std::stoi(v, used); };
    auto as_double = [](const std::string& v, std::size_t* used = nullptr) { return std::stod(v, used); };

    for (; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--out-dir") {
            opt.out_dir = need_value("--out-dir");
        } else if (a == "--config") {
            opt.config_path = need_value("--config");
        } else if (a == "--clean") {
            opt.clean = true;
        } else if (a == "--markers-only") {
            opt.clean = false;
        } else if (a == "--cut") {
            const std::string types = need_value("--cut");
            opt.cut = true;
            std::string token;
            std::string lowered = types;
            std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::size_t pos = 0;
            while (pos <= lowered.size()) {
                const std::size_t comma = lowered.find(',', pos);
                token = lowered.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
                if (token == "pauses" || token == "pause") opt.cut_pauses = true;
                else if (token == "clicks" || token == "click") opt.cut_clicks = true;
                else if (token == "breaths" || token == "breath") opt.cut_breaths = true;
                else if (!token.empty()) {
                    error = "unknown --cut type: " + token + " (use pauses,clicks,breaths)";
                    return false;
                }
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
        } else if (a == "--crossfade-ms") {
            opt.crossfade_ms = need_number("--crossfade-ms", as_double);
        } else if (a == "--labels") {
            opt.labels = true;
        } else if (a == "--export-markers") {
            const std::string formats = need_value("--export-markers");
            if (formats == "all" || formats == "*") {
                opt.export_markers_all = true;
            } else {
                const auto parsed = audio_core::parse_marker_export_formats(formats);
                opt.export_marker_formats.insert(opt.export_marker_formats.end(), parsed.begin(), parsed.end());
            }
        } else if (a == "--once") {
            opt.once = true;
        } else if (a == "--poll-ms") {
            opt.poll_ms = need_number("--poll-ms", as_int);
        } else if (a == "--settle-ms") {
            opt.settle_ms = need_number("--settle-ms", as_int);
        } else if (a == "-r" || a == "--recursive") {
            opt.recursive = true;
        } else if (a == "--force") {
            opt.force = true;
        } else if (a == "--dry-run") {
            opt.dry_run = true;
        } else if (a == "--no-pauses") {
            opt.disable_pauses = true;
        } else if (a == "--no-clicks") {
            opt.disable_clicks = true;
        } else if (a == "--no-breaths") {
            opt.disable_breaths = true;
        } else if (a == "--log-level") {
            opt.log_level = audio_core::log_level_from_string(need_value("--log-level"));
        } else if (a == "--log-file") {
            opt.log_file = need_value("--log-file");
        } else if (a == "-h" || a == "--help") {
            opt.command = "help";
            return true;
        } else if (!a.empty() && a[0] == '-') {
            error = "unknown option: " + a;
            return false;
        } else {
            opt.inputs.push_back(a);
        }
        if (!error.empty()) {
            return false;
        }
    }
    return true;
}

audio_core::PipelineConfig build_config(const Options& opt) {
    audio_core::PipelineConfig cfg;
    if (!opt.config_path.empty()) {
        cfg = audio_core::PipelineConfig::from_json_file(opt.config_path);
        Logger::instance().info("Loaded config: " + opt.config_path);
    }
    if (opt.disable_pauses) cfg.detect_pauses = false;
    if (opt.disable_clicks) cfg.detect_clicks = false;
    if (opt.disable_breaths) cfg.detect_breaths = false;
    return cfg;
}

struct FileOutcome {
    std::string input;
    bool ok = false;
    bool skipped = false;
    std::string error;
    int total_markers = 0;
    double peak_score = 0.0;
};

// One input file and where its outputs go.
struct Job {
    fs::path input;
    fs::path out_dir;
    std::string stem;
};

std::string lowercase(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string local_timestamp() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tm);
    return buf;
}

// Copies a session file aside before it is overwritten, so review work that
// cannot be carried onto the new detections is never silently destroyed.
fs::path backup_session(const fs::path& session_path, const std::string& stem) {
    const fs::path backup =
        session_path.parent_path() / (stem + ".audiotool." + local_timestamp() + ".bak.json");
    fs::copy_file(session_path, backup, fs::copy_options::overwrite_existing);
    return backup;
}

// Processes a single audio file into job.out_dir using job.stem for every output.
FileOutcome process_file(const Job& job, const audio_core::Pipeline& pipeline, const Options& opt) {
    const fs::path& input = job.input;
    const fs::path& out_dir = job.out_dir;
    const std::string& stem = job.stem;

    FileOutcome outcome;
    outcome.input = input.string();
    Logger& log = Logger::instance();

    try {
        const fs::path in_canon = fs::weakly_canonical(input);

        const fs::path markers_path = out_dir / (stem + ".markers.json");
        const fs::path report_path = out_dir / (stem + ".report.json");
        const fs::path session_path = out_dir / (stem + ".audiotool.json");
        const fs::path cleaned_path = out_dir / (stem + ".cleaned.wav");
        const fs::path edited_path = out_dir / (stem + ".edited.wav");
        const fs::path labels_path = out_dir / (stem + ".labels.txt");

        // Safety: never write any output onto the source file.
        for (const fs::path& outp : {markers_path, report_path, session_path, cleaned_path, edited_path, labels_path}) {
            if (fs::weakly_canonical(outp) == in_canon) {
                outcome.error = "refusing to overwrite source file";
                log.error(input.filename().string() + ": " + outcome.error);
                return outcome;
            }
        }

        if (!opt.force && !opt.dry_run) {
            // Skip only when *every* expected output already exists. Using OR
            // here would let a partial/older run (e.g. markers.json present but
            // the newer .audiotool.json missing) skip forever and never backfill.
            const bool any_missing =
                !fs::exists(markers_path) || !fs::exists(report_path) ||
                !fs::exists(session_path) ||
                (opt.clean && !fs::exists(cleaned_path)) ||
                (opt.cut && !fs::exists(edited_path)) ||
                (opt.labels && !fs::exists(labels_path));
            if (!any_missing) {
                outcome.skipped = true;
                log.warn(input.filename().string() + ": outputs exist, skipping (use --force)");
                return outcome;
            }
        }

        const auto t0 = std::chrono::steady_clock::now();
        const audio_core::AudioData audio = audio_core::AudioFile::load(input.string());
        const auto result = pipeline.analyze(audio, opt.clean);
        const auto t1 = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

        outcome.total_markers = static_cast<int>(result.markers.all().size());
        outcome.peak_score = result.peak_score;

        log.info(input.filename().string() + ": " + std::to_string(result.duration_sec) + "s, " +
                 std::to_string(result.pause_count) + " pauses, " +
                 std::to_string(result.click_count) + " clicks, " +
                 std::to_string(result.breath_count) + " breaths (" + std::to_string(ms) + " ms)");
        if (audio.source_channels > 1) {
            log.debug(input.filename().string() + ": " + std::to_string(audio.source_channels) +
                      " channels mixed down to mono for analysis and audio outputs");
        }

        if (opt.dry_run) {
            log.info(input.filename().string() + ": dry-run, no files written");
            outcome.ok = true;
            return outcome;
        }

        fs::create_directories(out_dir);

        // Build the session, carrying over any human accept/reject decisions
        // from a prior run so re-processing honors them in every export.
        auto session = audio_core::make_session(input.string(), result, audio.sample_rate,
                                                pipeline.config());
        if (fs::exists(session_path)) {
            try {
                const auto prior = audio_core::load_session_json(session_path.string());
                int unmatched = 0;
                const int carried = audio_core::merge_decisions(session, prior, &unmatched);
                if (carried > 0) {
                    log.info(input.filename().string() + ": carried " + std::to_string(carried) +
                             " prior review decision(s)");
                }
                if (unmatched > 0) {
                    const auto backup = backup_session(session_path, stem);
                    log.warn(input.filename().string() + ": " + std::to_string(unmatched) +
                             " reviewed marker(s) no longer match a detection; previous session saved as " +
                             backup.filename().string());
                }
            } catch (const std::exception& ex) {
                std::string where;
                try {
                    where = "; kept a copy as " + backup_session(session_path, stem).filename().string();
                } catch (const std::exception&) {
                }
                log.warn(session_path.filename().string() + ": ignoring unreadable prior session (" +
                         std::string(ex.what()) + ")" + where);
            }
        }

        // Exports reflect the review-filtered set. On a fresh run all markers are
        // Pending (none rejected), so this is identical to the full detected set.
        const auto review_markers = audio_core::exportable_markers(session);
        const bool has_rejections = review_markers.size() != result.markers.all().size();

        audio_core::Markers review_json;
        review_json.merge(review_markers);
        review_json.save_json(markers_path.string());
        audio_core::save_report_json(report_path.string(), input.string(), result);
        audio_core::save_session_json(session_path.string(), session);
        log.debug("wrote " + markers_path.string());
        log.debug("wrote " + report_path.string());
        log.debug("wrote " + session_path.string());

        if (opt.labels) {
            review_json.save_audacity_labels(labels_path.string());
            log.debug("wrote " + labels_path.string());
        }

        if (opt.export_markers_all) {
            audio_core::export_all_marker_formats(out_dir.string(), stem, review_markers,
                                                  result.duration_sec, audio.sample_rate);
            log.debug("wrote pro-editor marker sidecars for " + stem);
        } else if (!opt.export_marker_formats.empty()) {
            for (const auto fmt : opt.export_marker_formats) {
                audio_core::export_markers(fmt, (out_dir / stem).string(), review_markers,
                                           result.duration_sec, audio.sample_rate);
            }
            log.debug("wrote selected marker exports for " + stem);
        }

        if (opt.clean) {
            // Re-clean over the non-rejected markers if any were rejected, so a
            // rejected region is not attenuated in the exported audio.
            if (has_rejections) {
                const auto cleaned = audio_core::SpectralCleaner{}.clean(
                    audio.samples, audio.sample_rate, review_markers, pipeline.config().clean);
                audio_core::AudioFile::save_mono(cleaned_path.string(), cleaned, audio.sample_rate);
            } else {
                audio_core::AudioFile::save_mono(cleaned_path.string(), result.cleaned, audio.sample_rate);
            }
            log.debug("wrote " + cleaned_path.string());
        }

        if (opt.cut) {
            audio_core::EditConfig edit;
            edit.cut_pauses = opt.cut_pauses;
            edit.cut_clicks = opt.cut_clicks;
            edit.cut_breaths = opt.cut_breaths;
            edit.crossfade_ms = opt.crossfade_ms;
            const auto edited = audio_core::apply_cuts(audio.samples, audio.sample_rate,
                                                       review_markers, edit);
            audio_core::AudioFile::save_mono(edited_path.string(), edited.samples, audio.sample_rate);
            log.info(input.filename().string() + ": cut " + std::to_string(edited.regions_cut) +
                     " region(s), removed " + std::to_string(edited.removed_sec) + "s (" +
                     std::to_string(edited.original_sec) + "s -> " + std::to_string(edited.edited_sec) + "s)");
            log.debug("wrote " + edited_path.string());
        }

        outcome.ok = true;
    } catch (const std::exception& ex) {
        outcome.error = ex.what();
        log.error(input.filename().string() + ": " + outcome.error);
    }
    return outcome;
}

fs::path resolve_out_dir(const Options& opt, const fs::path& default_parent) {
    if (!opt.out_dir.empty()) {
        return fs::path(opt.out_dir);
    }
    return default_parent / "audio-tool-out";
}

// Assigns every input an output folder and stem. With a root (batch/watch),
// subfolders are mirrored under out_dir so a/ep1.wav and b/ep1.wav no longer
// write the same files. Inputs that would still share a stem in one output
// folder (ep1.wav + ep1.mp3) get the extension appended: ep1_wav, ep1_mp3.
std::vector<Job> plan_jobs(const std::vector<fs::path>& inputs, const fs::path& root, const fs::path& out_dir) {
    std::vector<Job> jobs;
    jobs.reserve(inputs.size());
    for (const auto& in : inputs) {
        Job job;
        job.input = in;
        job.out_dir = out_dir;
        if (!root.empty()) {
            std::error_code ec;
            const fs::path rel = fs::relative(in.parent_path(), root, ec);
            if (!ec && !rel.empty() && rel != "." && *rel.begin() != "..") {
                job.out_dir = out_dir / rel;
            }
        }
        job.stem = in.stem().string();
        jobs.push_back(std::move(job));
    }

    std::map<std::string, int> counts;
    auto key_of = [](const Job& j) { return lowercase((j.out_dir / j.stem).generic_string()); };
    for (const auto& j : jobs) {
        ++counts[key_of(j)];
    }
    for (auto& j : jobs) {
        if (counts[key_of(j)] > 1) {
            std::string ext = j.input.extension().string();
            if (!ext.empty() && ext.front() == '.') {
                ext.erase(0, 1);
            }
            j.stem += "_" + lowercase(ext);
        }
    }
    return jobs;
}

// Lists audio files under root. Never descends into exclude_dir (the output
// folder, which may sit inside root), and ignores this tool's own rendered WAVs
// when they live in exclude_dir, so outputs are never fed back in as inputs.
// Filesystem errors (permission denied, files vanishing mid-scan) are logged
// and skipped rather than thrown.
std::vector<fs::path> collect_audio_files(const fs::path& root, bool recursive, const fs::path& exclude_dir) {
    Logger& log = Logger::instance();
    std::vector<fs::path> files;
    std::error_code ec;
    const fs::path root_canon = fs::weakly_canonical(root, ec);
    fs::path excl;
    if (!exclude_dir.empty()) {
        excl = fs::weakly_canonical(exclude_dir, ec);
    }

    auto is_own_output = [&](const fs::path& p) {
        if (excl.empty()) {
            return false;
        }
        const std::string name = lowercase(p.filename().string());
        if (!ends_with(name, ".cleaned.wav") && !ends_with(name, ".edited.wav")) {
            return false;
        }
        std::error_code e2;
        return fs::weakly_canonical(p.parent_path(), e2) == excl;
    };

    auto consider = [&](const fs::directory_entry& e) {
        std::error_code e2;
        if (e.is_regular_file(e2) && is_audio_file(e.path()) && !is_own_output(e.path())) {
            files.push_back(e.path());
        }
    };

    if (recursive) {
        fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
        const fs::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            std::error_code e2;
            if (it->is_directory(e2)) {
                if (!excl.empty() && excl != root_canon && fs::weakly_canonical(it->path(), e2) == excl) {
                    it.disable_recursion_pending();
                }
                continue;
            }
            consider(*it);
        }
    } else {
        fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
        const fs::directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            consider(*it);
        }
    }
    if (ec) {
        log.warn("scan of " + root.string() + " stopped early: " + ec.message());
    }
    std::sort(files.begin(), files.end());
    return files;
}

int run_analyze(const Options& opt) {
    Logger& log = Logger::instance();
    if (opt.inputs.empty()) {
        log.error("analyze: no input files given");
        return 2;
    }

    const audio_core::Pipeline pipeline(build_config(opt));
    int failures = 0;

    // Group inputs by output folder so same-stem inputs sharing a folder are
    // disambiguated rather than overwriting each other.
    std::map<std::string, std::pair<fs::path, std::vector<fs::path>>> by_out;
    for (const auto& in : opt.inputs) {
        const fs::path input(in);
        if (!fs::exists(input)) {
            log.error("not found: " + in);
            ++failures;
            continue;
        }
        const fs::path out_dir = resolve_out_dir(opt, input.parent_path());
        auto& slot = by_out[lowercase(fs::weakly_canonical(out_dir).generic_string())];
        slot.first = out_dir;
        slot.second.push_back(input);
    }

    for (const auto& [key, group] : by_out) {
        for (const auto& job : plan_jobs(group.second, {}, group.first)) {
            const auto outcome = process_file(job, pipeline, opt);
            if (!outcome.ok && !outcome.skipped) {
                ++failures;
            }
        }
    }

    return failures == 0 ? 0 : 1;
}

int run_batch(const Options& opt) {
    Logger& log = Logger::instance();
    if (opt.inputs.empty()) {
        log.error("batch: no input folder given");
        return 2;
    }

    const audio_core::Pipeline pipeline(build_config(opt));
    bool any_failed = false;

    for (const auto& in : opt.inputs) {
        const fs::path root(in);
        if (!fs::exists(root) || !fs::is_directory(root)) {
            log.error("not a folder: " + in);
            any_failed = true;
            continue;
        }

        const auto t0 = std::chrono::steady_clock::now();
        const fs::path out_dir = resolve_out_dir(opt, root);
        if (!opt.dry_run) {
            fs::create_directories(out_dir);
        }

        const auto files = collect_audio_files(root, opt.recursive, out_dir);
        log.info("Found " + std::to_string(files.size()) + " audio file(s) in " + root.string());

        // Outcomes are per input folder so each batch-summary.json only
        // describes the files written next to it.
        std::vector<FileOutcome> outcomes;
        for (const auto& job : plan_jobs(files, opt.recursive ? root : fs::path{}, out_dir)) {
            outcomes.push_back(process_file(job, pipeline, opt));
        }

        int ok = 0, skipped = 0, failed = 0;
        nlohmann::json files_json = nlohmann::json::array();
        for (const auto& o : outcomes) {
            if (o.ok) ++ok;
            else if (o.skipped) ++skipped;
            else ++failed;
            files_json.push_back({
                {"input", o.input},
                {"ok", o.ok},
                {"skipped", o.skipped},
                {"error", o.error},
                {"total_markers", o.total_markers},
                {"peak_composite_score", o.peak_score},
            });
        }
        if (failed > 0) {
            any_failed = true;
        }

        if (!opt.dry_run) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - t0)
                                     .count();
            nlohmann::json summary{
                {"root", root.string()},
                {"processed", ok},
                {"skipped", skipped},
                {"failed", failed},
                {"total", static_cast<int>(outcomes.size())},
                {"elapsed_ms", elapsed},
                {"files", files_json},
            };
            const fs::path summary_path = out_dir / "batch-summary.json";
            std::ofstream out(summary_path);
            out << summary.dump(2);
            if (!out) {
                log.warn("could not write " + summary_path.string());
            }
            log.info("Summary: " + std::to_string(ok) + " ok, " + std::to_string(skipped) +
                     " skipped, " + std::to_string(failed) + " failed -> " + summary_path.string());
        }
    }

    return any_failed ? 1 : 0;
}

nlohmann::json load_watch_state(const fs::path& path) {
    if (!fs::exists(path)) {
        return nlohmann::json::object();
    }
    std::ifstream in(path);
    if (!in) {
        return nlohmann::json::object();
    }
    return nlohmann::json::parse(in, nullptr, false);
}

void save_watch_state(const fs::path& path, const nlohmann::json& state) {
    std::ofstream out(path);
    out << state.dump(2);
    if (!out) {
        Logger::instance().warn("watch: could not write " + path.string());
    }
}

long long write_stamp(const fs::path& path) {
    return fs::last_write_time(path).time_since_epoch().count();
}

// A file counts as settled once it has gone untouched for at least settle_ms.
// settle_ms <= 0 means "no wait"; run_watch only allows that for one-shot scans
// (--once), where files are expected to already be complete.
bool file_is_settled(const fs::path& path, int settle_ms) {
    if (settle_ms <= 0) {
        return true;
    }
    const auto age = fs::file_time_type::clock::now() - fs::last_write_time(path);
    return std::chrono::duration_cast<std::chrono::milliseconds>(age).count() >= settle_ms;
}

int run_watch(const Options& opt) {
    Logger& log = Logger::instance();
    if (opt.inputs.empty()) {
        log.error("watch: no input folder given");
        return 2;
    }

    const fs::path root(opt.inputs.front());
    if (!fs::exists(root) || !fs::is_directory(root)) {
        log.error("watch: not a folder: " + root.string());
        return 2;
    }

    Options process_opt = opt;
    process_opt.force = true;
    if (!process_opt.export_markers_all && process_opt.export_marker_formats.empty() && !process_opt.labels) {
        process_opt.export_markers_all = true;
    }

    // In continuous watch mode a zero settle window would let us grab files that
    // are still being copied in. Only honor --settle-ms 0 for one-shot scans
    // (--once), where the caller guarantees the files are already complete.
    if (!process_opt.once && process_opt.settle_ms <= 0) {
        constexpr int kMinContinuousSettleMs = 500;
        log.warn("watch: --settle-ms <= 0 is unsafe in continuous mode; using " +
                 std::to_string(kMinContinuousSettleMs) + " ms (use --once for an immediate one-shot scan)");
        process_opt.settle_ms = kMinContinuousSettleMs;
    }

    const fs::path out_dir = resolve_out_dir(process_opt, root);
    if (!process_opt.dry_run) {
        fs::create_directories(out_dir);
    }
    const fs::path state_path = out_dir / "watch-state.json";
    nlohmann::json state = load_watch_state(state_path);
    if (!state.is_object()) {
        state = nlohmann::json::object();
    }

    const audio_core::Pipeline pipeline(build_config(process_opt));
    log.info("Watching " + root.string() + " -> " + out_dir.string());
    log.info("Settle " + std::to_string(process_opt.settle_ms) + " ms, poll " +
             std::to_string(process_opt.poll_ms) + " ms");

    int failures = 0;
    do {
        const auto files = collect_audio_files(root, process_opt.recursive, out_dir);
        for (const auto& job : plan_jobs(files, process_opt.recursive ? root : fs::path{}, out_dir)) {
            // A file can be renamed, deleted or locked between the scan and
            // here. Treat that as "not ready" and look again next poll instead
            // of letting a filesystem_error end the watcher.
            try {
                if (!file_is_settled(job.input, process_opt.settle_ms)) {
                    continue;
                }
                const fs::path canon = fs::weakly_canonical(job.input);
                const std::string key = canon.string();
                const long long stamp = write_stamp(canon);
                // Skip only if this exact version was already processed successfully.
                // A prior failure (ok=false, not skipped) must be retried, otherwise
                // a transient error would permanently strand the file until its mtime
                // changes.
                if (state.contains(key)) {
                    const auto& entry = state.at(key);
                    const bool done = entry.value("ok", false) || entry.value("skipped", false);
                    if (done && entry.value("stamp", 0LL) == stamp) {
                        continue;
                    }
                }

                log.info("watch: processing " + job.input.filename().string());
                Job canon_job = job;
                canon_job.input = canon;
                auto outcome = process_file(canon_job, pipeline, process_opt);
                state[key] = {
                    {"stamp", stamp},
                    {"last_seen", std::chrono::duration_cast<std::chrono::seconds>(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count()},
                    {"ok", outcome.ok},
                    {"skipped", outcome.skipped},
                    {"error", outcome.error},
                };
                if (!process_opt.dry_run) {
                    save_watch_state(state_path, state);
                }
                if (!outcome.ok && !outcome.skipped) {
                    ++failures;
                }
            } catch (const std::exception& ex) {
                log.warn("watch: " + job.input.filename().string() + ": " + ex.what() + " (will retry)");
            }
        }

        if (process_opt.once) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds((std::max)(100, process_opt.poll_ms)));
    } while (true);

    return failures == 0 ? 0 : 1;
}

int run_init_config(const Options& opt) {
    Logger& log = Logger::instance();
    if (opt.inputs.empty()) {
        log.error("init-config: no output path given");
        return 2;
    }
    try {
        audio_core::PipelineConfig cfg;
        cfg.to_json_file(opt.inputs.front());
        log.info("Wrote default config to " + opt.inputs.front());
        return 0;
    } catch (const std::exception& ex) {
        log.error(ex.what());
        return 3;
    }
}

}  // namespace

int run_main(int argc, char** argv) {
    Options opt;
    std::string error;
    if (!parse_args(argc, argv, opt, error)) {
        if (!error.empty()) {
            std::cerr << "Error: " << error << "\n\n";
        }
        print_usage();
        return 2;
    }

    Logger& log = Logger::instance();
    log.set_level(opt.log_level);
    if (!opt.log_file.empty() && !log.set_file(opt.log_file)) {
        std::cerr << "Warning: could not open log file: " << opt.log_file << "\n";
    }

    if (opt.command == "help") {
        print_usage();
        return 0;
    }
    if (opt.command == "analyze") {
        return run_analyze(opt);
    }
    if (opt.command == "batch") {
        return run_batch(opt);
    }
    if (opt.command == "watch") {
        return run_watch(opt);
    }
    if (opt.command == "init-config") {
        return run_init_config(opt);
    }

    std::cerr << "Error: unknown command '" << opt.command << "'\n\n";
    print_usage();
    return 2;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    // The manifest makes narrow strings UTF-8; make the console agree so
    // non-ASCII file names in log lines print correctly.
    SetConsoleOutputCP(CP_UTF8);
#endif
    try {
        return run_main(argc, argv);
    } catch (const std::exception& ex) {
        // An unparseable --config, an unknown --export-markers format and
        // similar used to escape as an uncaught exception.
        std::cerr << "Error: " << ex.what() << "\n";
        return 3;
    }
}
