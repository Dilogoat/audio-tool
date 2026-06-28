#include "App.hpp"

#include "audio_core/Formats.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <portaudio.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <objbase.h>
#endif

namespace {

#ifdef _WIN32
// The common file dialog must run in a single-threaded COM apartment (STA).
// PortAudio's Pa_Initialize() puts the main thread into an MTA, which makes the
// Explorer dialog fail to enumerate files (only folders appear). Running the
// dialog on a dedicated STA thread avoids that conflict.
std::string run_file_dialog(bool save, const char* filter, const char* title,
                            const char* default_name, const char* default_ext) {
    std::string result;
    std::thread worker([&]() {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

        char filename[MAX_PATH] = "";
        if (save && default_name) {
            std::strncpy(filename, default_name, sizeof(filename) - 1);
        }

        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = title;
        ofn.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR |
                    (save ? OFN_OVERWRITEPROMPT : (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST));
        if (save) {
            ofn.lpstrDefExt = default_ext;
        }

        const BOOL ok = save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);
        if (ok) {
            result = filename;
        }

        if (SUCCEEDED(hr)) {
            CoUninitialize();
        }
    });
    worker.join();
    return result;
}
#endif

std::string open_file_dialog() {
#ifdef _WIN32
    static const std::string open_filter = audio_core::SupportedFormats::win32_open_filter();
    return run_file_dialog(false, open_filter.c_str(), "Open Audio File", nullptr, nullptr);
#else
    return {};
#endif
}

std::string save_file_dialog() {
#ifdef _WIN32
    return run_file_dialog(true, "WAV Files (*.wav)\0*.wav\0All Files (*.*)\0*.*\0\0",
                           "Export Cleaned Audio", "cleaned.wav", "wav");
#else
    return {};
#endif
}

std::string open_session_dialog() {
#ifdef _WIN32
    return run_file_dialog(false,
                           "Audio Tool Session (*.audiotool.json)\0*.audiotool.json\0"
                           "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0\0",
                           "Open Session", nullptr, nullptr);
#else
    return {};
#endif
}

ImVec4 marker_color(audio_core::MarkerType type) {
    switch (type) {
        case audio_core::MarkerType::Pause:
            return ImVec4(0.38f, 0.65f, 1.0f, 0.28f);
        case audio_core::MarkerType::Click:
            return ImVec4(1.0f, 0.32f, 0.36f, 0.40f);
        case audio_core::MarkerType::Breath:
            return ImVec4(1.0f, 0.78f, 0.22f, 0.34f);
        default:
            return ImVec4(0.5f, 0.5f, 0.5f, 0.20f);
    }
}

ImVec4 marker_swatch(audio_core::MarkerType type) {
    ImVec4 c = marker_color(type);
    c.w = 1.0f;
    return c;
}

void apply_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ChildRounding = 6.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.TabRounding = 4.0f;
    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(9, 5);
    style.ItemSpacing = ImVec2(9, 8);
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.ScrollbarSize = 12.0f;

    ImVec4* c = style.Colors;
    const ImVec4 bg = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    const ImVec4 panel = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
    const ImVec4 panel_hi = ImVec4(0.17f, 0.19f, 0.23f, 1.00f);
    const ImVec4 accent = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    const ImVec4 accent_hi = ImVec4(0.36f, 0.67f, 1.00f, 1.00f);

    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = panel;
    c[ImGuiCol_PopupBg] = panel;
    c[ImGuiCol_FrameBg] = panel_hi;
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.25f, 0.30f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.30f, 0.36f, 1.00f);
    c[ImGuiCol_TitleBg] = bg;
    c[ImGuiCol_TitleBgActive] = bg;
    c[ImGuiCol_Button] = panel_hi;
    c[ImGuiCol_ButtonHovered] = accent;
    c[ImGuiCol_ButtonActive] = accent_hi;
    c[ImGuiCol_Header] = panel_hi;
    c[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.25f, 0.30f, 1.00f);
    c[ImGuiCol_HeaderActive] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accent_hi;
    c[ImGuiCol_CheckMark] = accent_hi;
    c[ImGuiCol_Separator] = ImVec4(0.24f, 0.26f, 0.30f, 1.00f);
    c[ImGuiCol_Text] = ImVec4(0.90f, 0.92f, 0.95f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.52f, 0.56f, 1.00f);
    c[ImGuiCol_ScrollbarBg] = bg;
    c[ImGuiCol_ScrollbarGrab] = panel_hi;
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.33f, 0.38f, 1.00f);
}

}  // namespace

App::App() = default;

App::~App() { shutdown(); }

bool App::init() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(1400, 900, "Audio Cleanup & Analysis Tool", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui::StyleColorsDark();
    apply_theme();
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    if (Pa_Initialize() != paNoError) {
        std::cerr << "PortAudio init failed\n";
    }

    return true;
}

void App::shutdown() {
    stop_playback();
    if (analysis_thread_.joinable()) {
        analysis_thread_.join();
    }

    ImPlot::DestroyContext();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
    Pa_Terminate();
}

void App::run_analysis() {
    if (!has_audio_ || analyzing_) {
        return;
    }

    analyzing_ = true;
    if (analysis_thread_.joinable()) {
        analysis_thread_.join();
    }

    analysis_thread_ = std::thread([this]() {
        audio_core::AudioData local_audio;
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            local_audio = audio_;
        }

        // Shared detection path (identical to the CLI).
        audio_core::Pipeline pipeline(current_config());
        auto result = pipeline.analyze(local_audio, true);

        // RMS envelope and spectrogram are computed here for display only.
        audio_core::RmsAnalyzer rms(local_audio.sample_rate, local_audio.sample_rate / 20,
                                    local_audio.sample_rate / 40);
        const auto rms_data = rms.analyze(local_audio.samples);

        audio_core::Stft stft(local_audio.sample_rate, 2048, 512);
        const auto spec = stft.forward(local_audio.samples);

        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            markers_ = std::move(result.markers);
            review_decisions_.assign(markers_.all().size(), audio_core::ReviewDecision::Pending);
            rms_frames_ = std::move(rms_data);
            score_frames_ = std::move(result.scores);
            spectrogram_ = std::move(spec);
            cleaned_audio_ = std::move(result.cleaned);
            edited_cache_dirty_ = true;
            edited_audio_.clear();
        }

        analyzing_ = false;
    });
}

void App::rebuild_edited_cache() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (!has_audio_) {
        edited_audio_.clear();
        edited_cache_dirty_ = false;
        return;
    }
    const auto session = build_session_snapshot();
    const auto markers = audio_core::exportable_markers(session);
    const auto edited = audio_core::apply_cuts(audio_.samples, audio_.sample_rate, markers, edit_cfg_);
    edited_audio_ = std::move(edited.samples);
    edited_cache_dirty_ = false;
}

void App::start_playback_segment(double start_sec, double end_sec) {
    if (!has_audio_) {
        return;
    }
    stop_playback();
    playhead_sec_ = start_sec;
    playing_ = true;

    if (playback_thread_.joinable()) {
        playback_thread_.join();
    }

    playback_thread_ = std::thread([this, start_sec, end_sec]() {
        audio_core::AudioData local;
        std::vector<float> source;
        int sample_rate = 44100;
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            local = audio_;
            sample_rate = local.sample_rate;
            if (playback_source_ == PlaybackSource::Cleaned) {
                const auto session = build_session_snapshot();
                const auto markers = audio_core::exportable_markers(session);
                source = audio_core::SpectralCleaner{}.clean(local.samples, local.sample_rate,
                                                             markers, clean_cfg_);
            } else if (playback_source_ == PlaybackSource::Edited) {
                if (edited_cache_dirty_) {
                    const auto session = build_session_snapshot();
                    const auto markers = audio_core::exportable_markers(session);
                    const auto edited = audio_core::apply_cuts(local.samples, local.sample_rate,
                                                               markers, edit_cfg_);
                    edited_audio_ = edited.samples;
                    edited_cache_dirty_ = false;
                }
                source = edited_audio_.empty() ? local.samples : edited_audio_;
            } else {
                source = local.samples;
            }
        }

        const double duration = source.empty() ? 0.0 : static_cast<double>(source.size()) / sample_rate;
        const double clip_start = (std::max)(0.0, start_sec);
        const double clip_end = (std::min)(duration, end_sec);
        if (clip_end <= clip_start || source.empty()) {
            playing_ = false;
            return;
        }

        const int start_sample = static_cast<int>(clip_start * sample_rate);
        const int end_sample = static_cast<int>(clip_end * sample_rate);
        std::vector<float> segment(source.begin() + start_sample,
                                   source.begin() + (std::min)(end_sample, static_cast<int>(source.size())));

        PaStream* stream = nullptr;
        Pa_OpenDefaultStream(&stream, 0, 1, paFloat32, sample_rate, paFramesPerBufferUnspecified, nullptr, nullptr);
        Pa_StartStream(stream);

        std::size_t offset = 0;
        const int chunk = 1024;
        while (playing_ && offset < segment.size()) {
            const auto count = (std::min<std::size_t>)(chunk, segment.size() - offset);
            Pa_WriteStream(stream, segment.data() + offset, static_cast<unsigned long>(count));
            offset += count;
            playhead_sec_ = clip_start + static_cast<double>(offset) / sample_rate;
        }

        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        playing_ = false;
    });
}

void App::start_playback() {
    if (!has_audio_ || playing_) {
        return;
    }
    const double end = static_cast<double>(audio_.samples.size()) / audio_.sample_rate;
    start_playback_segment(playhead_sec_.load(), end);
}

void App::preview_marker_at(int index) {
    std::vector<audio_core::Marker> local_markers;
    double duration = 0.0;
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        if (index < 0 || index >= static_cast<int>(markers_.all().size())) {
            return;
        }
        local_markers = markers_.all();
        duration = audio_.samples.empty()
                       ? 0.0
                       : static_cast<double>(audio_.samples.size()) / audio_.sample_rate;
    }
    const auto& m = local_markers[static_cast<std::size_t>(index)];
    const double pre = 2.0;
    const double post = 2.0;
    const double start = (std::max)(0.0, m.start_sec - pre);
    const double end = (std::min)(duration, m.end_sec + post);
    start_playback_segment(start, end);
}

audio_core::PipelineConfig App::current_config() const {
    audio_core::PipelineConfig cfg;
    cfg.pause = pause_cfg_;
    cfg.transient = transient_cfg_;
    cfg.breath = breath_cfg_;
    cfg.score = score_cfg_;
    cfg.clean = clean_cfg_;
    cfg.detect_pauses = detect_pauses_;
    cfg.detect_clicks = detect_clicks_;
    cfg.detect_breaths = detect_breaths_;
    return cfg;
}

audio_core::Session App::build_session_snapshot() const {
    audio_core::Session session;
    session.source_path = file_path_;
    session.duration_sec = audio_.samples.empty()
                               ? 0.0
                               : static_cast<double>(audio_.samples.size()) / audio_.sample_rate;
    session.sample_rate = audio_.sample_rate;
    session.config = current_config();
    const auto& all = markers_.all();
    session.markers.reserve(all.size());
    for (std::size_t i = 0; i < all.size(); ++i) {
        const auto decision = i < review_decisions_.size()
                                  ? review_decisions_[i]
                                  : audio_core::ReviewDecision::Pending;
        session.markers.push_back({all[i], decision, {}});
    }
    return session;
}

void App::stop_playback() {
    playing_ = false;
    if (playback_thread_.joinable()) {
        playback_thread_.join();
    }
}

struct App::UiSnapshot {
    audio_core::AudioData audio;
    audio_core::Markers markers;
    std::vector<audio_core::RmsFrame> rms;
    std::vector<audio_core::ScoreFrame> scores;
    std::vector<audio_core::ReviewDecision> review;
    audio_core::Spectrogram spec;
    double duration = 0.0;
};

void App::open_audio() {
    const auto path = open_file_dialog();
    if (path.empty()) {
        return;
    }
    try {
        std::lock_guard<std::mutex> lock(data_mutex_);
        file_path_ = path;
        audio_ = audio_core::AudioFile::load(path);
        has_audio_ = true;
        markers_ = {};
        review_decisions_.clear();
        rms_frames_.clear();
        score_frames_.clear();
        spectrogram_ = {};
        cleaned_audio_.clear();
        edited_audio_.clear();
        edited_cache_dirty_ = true;
        view_start_sec_ = 0.0;
        view_end_sec_ = (std::min)(30.0, static_cast<double>(audio_.samples.size()) / audio_.sample_rate);
        playhead_sec_ = 0.0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
    }
}

void App::load_session() {
    const auto path = open_session_dialog();
    if (path.empty()) {
        return;
    }
    stop_playback();
    try {
        const auto session = audio_core::load_session_json(path);
        // Load the referenced source audio before mutating any state, so a
        // moved/missing source leaves the current session untouched.
        audio_core::AudioData loaded = audio_core::AudioFile::load(session.source_path);

        std::lock_guard<std::mutex> lock(data_mutex_);
        file_path_ = session.source_path;
        audio_ = std::move(loaded);
        has_audio_ = true;

        pause_cfg_ = session.config.pause;
        transient_cfg_ = session.config.transient;
        breath_cfg_ = session.config.breath;
        score_cfg_ = session.config.score;
        clean_cfg_ = session.config.clean;
        detect_pauses_ = session.config.detect_pauses;
        detect_clicks_ = session.config.detect_clicks;
        detect_breaths_ = session.config.detect_breaths;

        std::vector<audio_core::Marker> restored;
        restored.reserve(session.markers.size());
        review_decisions_.clear();
        review_decisions_.reserve(session.markers.size());
        for (const auto& sm : session.markers) {
            restored.push_back(sm.marker);
            review_decisions_.push_back(sm.decision);
        }
        markers_ = {};
        markers_.merge(restored);

        // Analysis-derived visuals (RMS, score, spectrogram, cleaned audio) are
        // not persisted in the session; clear them until the user re-runs Analyze.
        rms_frames_.clear();
        score_frames_.clear();
        spectrogram_ = {};
        cleaned_audio_.clear();
        edited_audio_.clear();
        edited_cache_dirty_ = true;
        selected_marker_ = -1;
        decision_request_index_ = -1;
        view_start_sec_ = 0.0;
        view_end_sec_ = (std::min)(30.0, static_cast<double>(audio_.samples.size()) / audio_.sample_rate);
        playhead_sec_ = 0.0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
    }
}

void App::render_toolbar() {
    const bool busy = analyzing_;

    if (ImGui::Button("Open", ImVec2(78, 0))) {
        open_audio();
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(busy);
    if (ImGui::Button("Load Session", ImVec2(120, 0))) {
        load_session();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!has_audio_ || busy);
    if (ImGui::Button("Analyze", ImVec2(86, 0))) {
        run_analysis();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!has_audio_);
    if (playing_) {
        if (ImGui::Button("Stop", ImVec2(78, 0))) {
            stop_playback();
        }
    } else {
        if (ImGui::Button("Play", ImVec2(78, 0))) {
            start_playback();
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    ImGui::BeginDisabled(!has_audio_);
    if (ImGui::Button("Export Markers", ImVec2(120, 0))) {
        try {
            std::lock_guard<std::mutex> lock(data_mutex_);
            const auto stem = std::filesystem::path(file_path_).stem().string();
            const auto dir = std::filesystem::path(file_path_).parent_path().string();
            const double duration = audio_.samples.empty()
                                        ? 0.0
                                        : static_cast<double>(audio_.samples.size()) / audio_.sample_rate;
            const auto session = build_session_snapshot();
            const auto export_markers = audio_core::exportable_markers(session);
            audio_core::export_all_marker_formats(dir, stem, export_markers, duration, audio_.sample_rate);
            audio_core::Markers json_markers;
            json_markers.merge(export_markers);
            json_markers.save_json((std::filesystem::path(dir) / (stem + ".markers.json")).string());
        } catch (const std::exception& ex) {
            std::cerr << ex.what() << '\n';
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Session", ImVec2(120, 0))) {
        try {
            std::lock_guard<std::mutex> lock(data_mutex_);
            const auto out = std::filesystem::path(file_path_).replace_extension(".audiotool.json");
            audio_core::save_session_json(out.string(), build_session_snapshot());
        } catch (const std::exception& ex) {
            std::cerr << ex.what() << '\n';
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Export Cleaned WAV", ImVec2(150, 0))) {
        const auto path = save_file_dialog();
        if (!path.empty()) {
            try {
                std::lock_guard<std::mutex> lock(data_mutex_);
                // Re-run cleaning over only the non-rejected markers so rejecting
                // a marker actually excludes its region from the exported audio.
                const auto session = build_session_snapshot();
                const auto markers = audio_core::exportable_markers(session);
                const auto cleaned = audio_core::SpectralCleaner{}.clean(
                    audio_.samples, audio_.sample_rate, markers, clean_cfg_);
                audio_core::AudioFile::save_mono(path, cleaned, audio_.sample_rate);
            } catch (const std::exception& ex) {
                std::cerr << ex.what() << '\n';
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Export Edited WAV", ImVec2(140, 0))) {
        const auto path = save_file_dialog();
        if (!path.empty()) {
            try {
                std::lock_guard<std::mutex> lock(data_mutex_);
                // Only cut non-rejected markers; a rejected marker must never be
                // physically removed from the exported edit.
                const auto session = build_session_snapshot();
                const auto markers = audio_core::exportable_markers(session);
                const auto edited = audio_core::apply_cuts(audio_.samples, audio_.sample_rate,
                                                           markers, edit_cfg_);
                audio_core::AudioFile::save_mono(path, edited.samples, audio_.sample_rate);
            } catch (const std::exception& ex) {
                std::cerr << ex.what() << '\n';
            }
        }
    }
    ImGui::EndDisabled();

    if (busy) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.2f, 1.0f), "Analyzing...");
    }
}

void App::render_sidebar(const UiSnapshot& snap) {
    ImGui::TextDisabled("FILE");
    if (has_audio_) {
        const auto name = std::filesystem::path(file_path_).filename().string();
        ImGui::TextWrapped("%s", name.c_str());
        ImGui::Text("%.2f s  -  %d Hz", snap.duration, snap.audio.sample_rate);
        ImGui::Text("Playhead: %.2f s", playhead_sec_.load());
    } else {
        ImGui::TextDisabled("No file loaded");
    }

    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 6));

    ImGui::TextDisabled("PLAYBACK");
    const char* source_labels[] = {"Original", "Cleaned", "Edited"};
    int source_idx = static_cast<int>(playback_source_);
    if (ImGui::Combo("Source", &source_idx, source_labels, 3)) {
        playback_source_ = static_cast<PlaybackSource>(source_idx);
        stop_playback();
    }
    ImGui::BeginDisabled(!has_audio_ || selected_marker_ < 0);
    if (ImGui::Button("Preview marker", ImVec2(-1, 0))) {
        preview_marker_request_ = selected_marker_;
    }
    ImGui::EndDisabled();
    ImGui::TextDisabled("Double-click a marker to preview +/- 2s.");
    const bool can_review = selected_marker_ >= 0 &&
                            selected_marker_ < static_cast<int>(snap.review.size());
    ImGui::BeginDisabled(!has_audio_ || !can_review);
    if (ImGui::Button("Accept", ImVec2(88, 0)) && can_review) {
        decision_request_index_ = selected_marker_;
        decision_request_value_ = audio_core::ReviewDecision::Accepted;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reject", ImVec2(88, 0)) && can_review) {
        decision_request_index_ = selected_marker_;
        decision_request_value_ = audio_core::ReviewDecision::Rejected;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(72, 0)) && can_review) {
        decision_request_index_ = selected_marker_;
        decision_request_value_ = audio_core::ReviewDecision::Pending;
    }
    ImGui::EndDisabled();

    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 6));

    ImGui::TextDisabled("DETECTOR SETTINGS");
    ImGui::PushItemWidth(-1);
    ImGui::Text("Pause RMS threshold");
    ImGui::SliderFloat("##prms", reinterpret_cast<float*>(&pause_cfg_.rms_threshold), 0.001f, 0.1f, "%.4f");
    ImGui::Text("Pause min duration (s)");
    ImGui::SliderFloat("##pdur", reinterpret_cast<float*>(&pause_cfg_.min_duration_sec), 0.1f, 3.0f, "%.2f");
    ImGui::Text("Click spike ratio");
    ImGui::SliderFloat("##cspk", reinterpret_cast<float*>(&transient_cfg_.spike_ratio), 2.0f, 12.0f, "%.1f");
    ImGui::Text("Breath HF ratio");
    ImGui::SliderFloat("##bhf", reinterpret_cast<float*>(&breath_cfg_.hf_ratio_threshold), 0.3f, 0.9f, "%.2f");
    ImGui::Text("Clean attenuation");
    ImGui::SliderFloat("##clean", reinterpret_cast<float*>(&clean_cfg_.attenuation), 0.0f, 1.0f, "%.2f");
    ImGui::PopItemWidth();

    ImGui::Dummy(ImVec2(0, 6));
    ImGui::TextDisabled("CUT (Export Edited WAV)");
    ImGui::Checkbox("Cut pauses", &edit_cfg_.cut_pauses);
    ImGui::Checkbox("Cut clicks", &edit_cfg_.cut_clicks);
    ImGui::Checkbox("Cut breaths", &edit_cfg_.cut_breaths);
    ImGui::PushItemWidth(-1);
    ImGui::Text("Crossfade (ms)");
    ImGui::SliderFloat("##xfade", reinterpret_cast<float*>(&edit_cfg_.crossfade_ms), 0.0f, 30.0f, "%.1f");
    ImGui::PopItemWidth();

    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Checkbox("Follow playhead", &follow_playhead_);
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset view") && has_audio_) {
        view_start_sec_ = 0.0;
        view_end_sec_ = (std::min)(30.0, snap.duration);
    }

    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 6));

    const auto legend = [](audio_core::MarkerType type, const char* label) {
        ImGui::ColorButton(label, marker_swatch(type),
                           ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(14, 14));
        ImGui::SameLine();
        ImGui::TextUnformatted(label);
    };

    ImGui::TextDisabled("MARKERS  (%d)", static_cast<int>(snap.markers.all().size()));
    int accepted = 0;
    int rejected = 0;
    for (const auto decision : snap.review) {
        if (decision == audio_core::ReviewDecision::Accepted) ++accepted;
        if (decision == audio_core::ReviewDecision::Rejected) ++rejected;
    }
    ImGui::TextDisabled("Review: %d accepted, %d rejected", accepted, rejected);
    legend(audio_core::MarkerType::Pause, "Pause");
    ImGui::SameLine(0, 16);
    legend(audio_core::MarkerType::Click, "Click");
    legend(audio_core::MarkerType::Breath, "Breath");

    ImGui::Dummy(ImVec2(0, 4));
    ImGui::BeginChild("marker_list", ImVec2(0, 0), true);
    if (snap.markers.all().empty()) {
        ImGui::TextDisabled("Run Analyze to detect events.");
    }
    for (int i = 0; i < static_cast<int>(snap.markers.all().size()); ++i) {
        const auto& m = snap.markers.all()[static_cast<std::size_t>(i)];
        ImGui::ColorButton("##c", marker_swatch(m.type),
                           ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(12, 12));
        ImGui::SameLine();
        if (ImGui::Selectable(
                (audio_core::marker_type_to_string(m.type) + "##" + std::to_string(i)).c_str(),
                selected_marker_ == i)) {
            selected_marker_ = i;
            const double len = (std::max)(0.5, (m.end_sec - m.start_sec) * 4.0);
            const double center = (m.start_sec + m.end_sec) * 0.5;
            view_start_sec_ = (std::max)(0.0, center - len * 0.5);
            view_end_sec_ = (std::min)(snap.duration, center + len * 0.5);
            playhead_sec_ = m.start_sec;
            follow_playhead_ = false;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            preview_marker_request_ = i;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%.2f-%.2fs", m.start_sec, m.end_sec);
        if (i < static_cast<int>(snap.review.size())) {
            ImGui::SameLine();
            const auto decision = snap.review[static_cast<std::size_t>(i)];
            if (decision == audio_core::ReviewDecision::Accepted) {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.55f, 1.0f), "accepted");
            } else if (decision == audio_core::ReviewDecision::Rejected) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "rejected");
            }
        }
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 22);
        if (ImGui::SmallButton(("x##rm" + std::to_string(i)).c_str())) {
            marker_remove_request_ = i;
        }
    }
    ImGui::EndChild();
}

void App::render_plots(const UiSnapshot& snap) {
    if (!has_audio_) {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + avail.x * 0.5f - 150,
                                   ImGui::GetCursorPosY() + avail.y * 0.5f - 20));
        ImGui::BeginGroup();
        ImGui::TextDisabled("Open an audio file to begin.");
        ImGui::TextDisabled("%s supported.", audio_core::SupportedFormats::display_list().c_str());
        ImGui::EndGroup();
        return;
    }

    if (follow_playhead_) {
        const double span = view_end_sec_ - view_start_sec_;
        const double play = playhead_sec_.load();
        view_start_sec_ = (std::max)(0.0, play - span * 0.5);
        view_end_sec_ = view_start_sec_ + span;
    }

    const double play_x = playhead_sec_.load();
    const auto draw_markers = [&](double y_lo, double y_hi) {
        for (const auto& m : snap.markers.all()) {
            if (m.end_sec < view_start_sec_ || m.start_sec > view_end_sec_) {
                continue;
            }
            const auto c = marker_color(m.type);
            const double xs_fill[2] = {m.start_sec, m.end_sec};
            const double ys_lo[2] = {y_lo, y_lo};
            const double ys_hi[2] = {y_hi, y_hi};
            ImPlot::PlotShaded("##marker", xs_fill, ys_lo, ys_hi, 2,
                               ImPlotSpec(ImPlotProp_FillColor, c, ImPlotProp_FillAlpha, c.w));
        }
    };
    const auto draw_playhead = [&](double y_lo, double y_hi) {
        const double px[2] = {play_x, play_x};
        const double py[2] = {y_lo, y_hi};
        ImPlot::PlotLine("##playhead", px, py, 2,
                         ImPlotSpec(ImPlotProp_LineColor, ImVec4(1.0f, 1.0f, 1.0f, 0.9f)));
    };
    const auto seek_if_clicked = [&]() {
        if (ImPlot::IsPlotHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            ImGui::GetIO().KeyShift) {
            const double x = ImPlot::GetPlotMousePos().x;
            playhead_sec_ = (std::max)(0.0, (std::min)(snap.duration, x));
        }
    };

    const float avail_h = ImGui::GetContentRegionAvail().y;
    const float gap = 6.0f;
    const float wave_h = avail_h * 0.34f;
    const float rms_h = avail_h * 0.18f;
    const float score_h = avail_h * 0.16f;
    const float spec_h = avail_h - wave_h - rms_h - score_h - gap * 3.0f;

    const ImPlotAxisFlags x_link = ImPlotAxisFlags_None;

    // Waveform
    if (ImPlot::BeginPlot("Waveform##wave", ImVec2(-1, wave_h),
                          ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes(nullptr, "Amplitude", x_link, ImPlotAxisFlags_Lock);
        ImPlot::SetupAxisLinks(ImAxis_X1, &view_start_sec_, &view_end_sec_);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -1.05, 1.05, ImGuiCond_Always);

        const int start_idx = (std::max)(0, static_cast<int>(view_start_sec_ * snap.audio.sample_rate));
        const int end_idx = (std::min)(static_cast<int>(snap.audio.samples.size()),
                                       static_cast<int>(view_end_sec_ * snap.audio.sample_rate));
        const int step = (std::max)(1, (end_idx - start_idx) / 4000);

        std::vector<double> xs;
        std::vector<double> ys;
        xs.reserve(4096);
        ys.reserve(4096);
        for (int i = start_idx; i < end_idx; i += step) {
            xs.push_back(static_cast<double>(i) / snap.audio.sample_rate);
            ys.push_back(snap.audio.samples[static_cast<std::size_t>(i)]);
        }

        draw_markers(-1.05, 1.05);
        if (!xs.empty()) {
            ImPlot::PlotLine("waveform", xs.data(), ys.data(), static_cast<int>(xs.size()),
                             ImPlotSpec(ImPlotProp_LineColor, ImVec4(0.45f, 0.78f, 1.0f, 1.0f)));
        }
        draw_playhead(-1.05, 1.05);
        seek_if_clicked();
        ImPlot::EndPlot();
    }

    ImGui::Dummy(ImVec2(0, gap - ImGui::GetStyle().ItemSpacing.y));

    // RMS envelope
    if (ImPlot::BeginPlot("RMS Envelope##rms", ImVec2(-1, rms_h), ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes(nullptr, "RMS", x_link, ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisLinks(ImAxis_X1, &view_start_sec_, &view_end_sec_);

        std::vector<double> tx;
        std::vector<double> ry;
        for (const auto& f : snap.rms) {
            tx.push_back(f.time_sec);
            ry.push_back(f.rms);
        }
        if (!tx.empty()) {
            ImPlot::PlotLine("rms", tx.data(), ry.data(), static_cast<int>(tx.size()),
                             ImPlotSpec(ImPlotProp_LineColor, ImVec4(0.6f, 0.9f, 0.6f, 1.0f)));
        }
        ImPlot::DragLineY(0, &pause_cfg_.rms_threshold, ImVec4(1.0f, 0.4f, 0.4f, 1.0f), 1.5f);
        draw_playhead(0.0, 1.0);
        ImPlot::EndPlot();
    }

    ImGui::Dummy(ImVec2(0, gap - ImGui::GetStyle().ItemSpacing.y));

    // Composite score
    if (ImPlot::BeginPlot("Composite Score##score", ImVec2(-1, score_h), ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes(nullptr, "Score", x_link, ImPlotAxisFlags_Lock);
        ImPlot::SetupAxisLinks(ImAxis_X1, &view_start_sec_, &view_end_sec_);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 1.05, ImGuiCond_Always);

        std::vector<double> sx;
        std::vector<double> sy;
        for (const auto& s : snap.scores) {
            sx.push_back(s.time_sec);
            sy.push_back(s.score);
        }
        if (!sx.empty()) {
            ImPlot::PlotBars("score", sx.data(), sy.data(), static_cast<int>(sx.size()), 0.045,
                             ImPlotSpec(ImPlotProp_FillColor, ImVec4(1.0f, 0.55f, 0.3f, 0.9f)));
        }
        draw_playhead(0.0, 1.05);
        ImPlot::EndPlot();
    }

    ImGui::Dummy(ImVec2(0, gap - ImGui::GetStyle().ItemSpacing.y));

    // Spectrogram
    if (ImPlot::BeginPlot("Spectrogram##spec", ImVec2(-1, (std::max)(spec_h, 80.0f)),
                          ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Time (s)", "Freq (Hz)", x_link, ImPlotAxisFlags_Lock);
        ImPlot::SetupAxisLinks(ImAxis_X1, &view_start_sec_, &view_end_sec_);

        if (!snap.spec.frames.empty()) {
            const int bins = static_cast<int>(snap.spec.frames[0].size());
            const int frames = static_cast<int>(snap.spec.frames.size());
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, static_cast<double>(snap.spec.sample_rate) / 2.0,
                                    ImGuiCond_Always);

            std::vector<float> heat(static_cast<std::size_t>(bins) * frames, 0.0f);
            float min_v = 1e30f;
            float max_v = -1e30f;
            for (int t = 0; t < frames; ++t) {
                for (int b = 0; b < bins; ++b) {
                    const float v = static_cast<float>(std::log10(
                        1e-12 + std::abs(snap.spec.frames[static_cast<std::size_t>(t)][static_cast<std::size_t>(b)])));
                    heat[static_cast<std::size_t>(t) * bins + b] = v;
                    min_v = (std::min)(min_v, v);
                    max_v = (std::max)(max_v, v);
                }
            }

            ImPlot::PushColormap(ImPlotColormap_Viridis);
            ImPlot::PlotHeatmap("spec", heat.data(), bins, frames, min_v, max_v, nullptr,
                                ImPlotPoint(0.0, 0.0),
                                ImPlotPoint(snap.duration, static_cast<double>(snap.spec.sample_rate) / 2.0));
            ImPlot::PopColormap();
        }
        draw_playhead(0.0, static_cast<double>(snap.spec.frames.empty() ? 1 : snap.spec.sample_rate) / 2.0);
        ImPlot::EndPlot();
    }
}

void App::render_ui() {
    UiSnapshot snap;
    int preview_request = -1;
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        if (marker_remove_request_ >= 0 &&
            marker_remove_request_ < static_cast<int>(markers_.all().size())) {
            auto updated = markers_.all();
            updated.erase(updated.begin() + marker_remove_request_);
            markers_ = {};
            markers_.merge(updated);
            if (marker_remove_request_ < static_cast<int>(review_decisions_.size())) {
                review_decisions_.erase(review_decisions_.begin() + marker_remove_request_);
            }
            if (selected_marker_ >= static_cast<int>(markers_.all().size())) {
                selected_marker_ = static_cast<int>(markers_.all().size()) - 1;
            }
            edited_cache_dirty_ = true;
        }
        marker_remove_request_ = -1;
        preview_request = preview_marker_request_;
        preview_marker_request_ = -1;

        // Apply any pending review-decision change under the lock so it can never
        // race the analysis thread, which reassigns review_decisions_ wholesale.
        if (decision_request_index_ >= 0 &&
            decision_request_index_ < static_cast<int>(review_decisions_.size())) {
            review_decisions_[static_cast<std::size_t>(decision_request_index_)] =
                decision_request_value_;
            edited_cache_dirty_ = true;
        }
        decision_request_index_ = -1;

        snap.audio = audio_;
        snap.markers = markers_;
        snap.rms = rms_frames_;
        snap.scores = score_frames_;
        snap.review = review_decisions_;
        snap.spec = spectrogram_;
        snap.duration = audio_.samples.empty()
                            ? 0.0
                            : static_cast<double>(audio_.samples.size()) / audio_.sample_rate;
    }
    if (preview_request >= 0) {
        preview_marker_at(preview_request);
    }

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    ImGui::Begin("##app_root", nullptr, flags);

    render_toolbar();
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 2));

    ImGui::BeginChild("##sidebar", ImVec2(300, 0), true);
    render_sidebar(snap);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##plots", ImVec2(0, 0), false);
    render_plots(snap);
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void App::process_frame() {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    render_ui();
    ImGui::Render();
    int w, h;
    glfwGetFramebufferSize(window_, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
}

void App::run() {
    while (window_ && !glfwWindowShouldClose(window_)) {
        process_frame();
    }
}
