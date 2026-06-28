#include "audio_core/AudioFile.hpp"
#include "audio_core/Editor.hpp"
#include "audio_core/Formats.hpp"
#include "audio_core/Logger.hpp"
#include "audio_core/Pipeline.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using audio_core::Logger;

namespace {

constexpr const char* kVersion = "0.2.0";

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
};

void print_usage() {
    std::cout <<
        "Audio Cleanup & Analysis Tool - CLI v" << kVersion << "\n\n"
        "USAGE:\n"
        "  audio_tool_cli <command> [paths...] [options]\n\n"
        "COMMANDS:\n"
        "  analyze <file...>     Analyze one or more audio files\n"
        "  batch <dir...>        Analyze every audio file in a folder\n"
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
            opt.crossfade_ms = std::stod(need_value("--crossfade-ms"));
        } else if (a == "--labels") {
            opt.labels = true;
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

// Processes a single audio file. out_dir must already exist (unless dry-run).
FileOutcome process_file(const fs::path& input, const fs::path& out_dir,
                         const audio_core::Pipeline& pipeline, const Options& opt) {
    FileOutcome outcome;
    outcome.input = input.string();
    Logger& log = Logger::instance();

    try {
        const fs::path in_canon = fs::weakly_canonical(input);
        const std::string stem = input.stem().string();

        const fs::path markers_path = out_dir / (stem + ".markers.json");
        const fs::path report_path = out_dir / (stem + ".report.json");
        const fs::path cleaned_path = out_dir / (stem + ".cleaned.wav");
        const fs::path edited_path = out_dir / (stem + ".edited.wav");
        const fs::path labels_path = out_dir / (stem + ".labels.txt");

        // Safety: never write any output onto the source file.
        for (const fs::path& outp : {markers_path, report_path, cleaned_path, edited_path, labels_path}) {
            if (fs::weakly_canonical(outp) == in_canon) {
                outcome.error = "refusing to overwrite source file";
                log.error(input.filename().string() + ": " + outcome.error);
                return outcome;
            }
        }

        if (!opt.force && !opt.dry_run) {
            const bool exists = fs::exists(markers_path) || fs::exists(report_path) ||
                                (opt.clean && fs::exists(cleaned_path)) ||
                                (opt.cut && fs::exists(edited_path)) ||
                                (opt.labels && fs::exists(labels_path));
            if (exists) {
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

        if (opt.dry_run) {
            log.info(input.filename().string() + ": dry-run, no files written");
            outcome.ok = true;
            return outcome;
        }

        result.markers.save_json(markers_path.string());
        audio_core::save_report_json(report_path.string(), input.string(), result);
        log.debug("wrote " + markers_path.string());
        log.debug("wrote " + report_path.string());

        if (opt.labels) {
            result.markers.save_audacity_labels(labels_path.string());
            log.debug("wrote " + labels_path.string());
        }

        if (opt.clean) {
            audio_core::AudioFile::save_mono(cleaned_path.string(), result.cleaned, audio.sample_rate);
            log.debug("wrote " + cleaned_path.string());
        }

        if (opt.cut) {
            audio_core::EditConfig edit;
            edit.cut_pauses = opt.cut_pauses;
            edit.cut_clicks = opt.cut_clicks;
            edit.cut_breaths = opt.cut_breaths;
            edit.crossfade_ms = opt.crossfade_ms;
            const auto edited = audio_core::apply_cuts(audio.samples, audio.sample_rate,
                                                       result.markers.all(), edit);
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

int run_analyze(const Options& opt) {
    Logger& log = Logger::instance();
    if (opt.inputs.empty()) {
        log.error("analyze: no input files given");
        return 2;
    }

    const audio_core::Pipeline pipeline(build_config(opt));
    int failures = 0;

    for (const auto& in : opt.inputs) {
        const fs::path input(in);
        if (!fs::exists(input)) {
            log.error("not found: " + in);
            ++failures;
            continue;
        }
        const fs::path out_dir = resolve_out_dir(opt, input.parent_path());
        if (!opt.dry_run) {
            fs::create_directories(out_dir);
        }
        const auto outcome = process_file(input, out_dir, pipeline, opt);
        if (!outcome.ok && !outcome.skipped) {
            ++failures;
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
    std::vector<FileOutcome> outcomes;
    const auto t0 = std::chrono::steady_clock::now();

    for (const auto& in : opt.inputs) {
        const fs::path root(in);
        if (!fs::exists(root) || !fs::is_directory(root)) {
            log.error("not a folder: " + in);
            continue;
        }

        const fs::path out_dir = resolve_out_dir(opt, root);
        if (!opt.dry_run) {
            fs::create_directories(out_dir);
        }

        std::vector<fs::path> files;
        if (opt.recursive) {
            for (const auto& e : fs::recursive_directory_iterator(root)) {
                if (e.is_regular_file() && is_audio_file(e.path())) {
                    files.push_back(e.path());
                }
            }
        } else {
            for (const auto& e : fs::directory_iterator(root)) {
                if (e.is_regular_file() && is_audio_file(e.path())) {
                    files.push_back(e.path());
                }
            }
        }
        std::sort(files.begin(), files.end());
        log.info("Found " + std::to_string(files.size()) + " audio file(s) in " + root.string());

        for (const auto& f : files) {
            outcomes.push_back(process_file(f, out_dir, pipeline, opt));
        }

        if (!opt.dry_run) {
            const auto t1 = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

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
            nlohmann::json summary{
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
            log.info("Summary: " + std::to_string(ok) + " ok, " + std::to_string(skipped) +
                     " skipped, " + std::to_string(failed) + " failed -> " + summary_path.string());
        }
    }

    const bool any_failed = std::any_of(outcomes.begin(), outcomes.end(),
                                        [](const FileOutcome& o) { return !o.ok && !o.skipped; });
    return any_failed ? 1 : 0;
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

int main(int argc, char** argv) {
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
    if (opt.command == "init-config") {
        return run_init_config(opt);
    }

    std::cerr << "Error: unknown command '" << opt.command << "'\n\n";
    print_usage();
    return 2;
}
