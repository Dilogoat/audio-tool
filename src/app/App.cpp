#include "App.hpp"

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
#endif

namespace {

std::string open_file_dialog() {
#ifdef _WIN32
    char filename[MAX_PATH] = "";
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Audio Files\0*.wav;*.flac;*.ogg;*.aiff\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Open Audio File";
    if (GetOpenFileNameA(&ofn)) {
        return filename;
    }
#endif
    return {};
}

std::string save_file_dialog() {
#ifdef _WIN32
    char filename[MAX_PATH] = "cleaned.wav";
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "WAV Files\0*.wav\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrTitle = "Export Cleaned Audio";
    if (GetSaveFileNameA(&ofn)) {
        return filename;
    }
#endif
    return {};
}

ImVec4 marker_color(audio_core::MarkerType type) {
    switch (type) {
        case audio_core::MarkerType::Pause:
            return ImVec4(0.4f, 0.7f, 1.0f, 0.35f);
        case audio_core::MarkerType::Click:
            return ImVec4(1.0f, 0.3f, 0.3f, 0.45f);
        case audio_core::MarkerType::Breath:
            return ImVec4(1.0f, 0.8f, 0.2f, 0.4f);
        default:
            return ImVec4(0.5f, 0.5f, 0.5f, 0.2f);
    }
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

        audio_core::PauseDetector pause(pause_cfg_);
        audio_core::TransientDetector trans(transient_cfg_);
        audio_core::BreathDetector breath(breath_cfg_);

        audio_core::Markers result;
        result.merge(pause.detect(local_audio.samples, local_audio.sample_rate));
        result.merge(trans.detect(local_audio.samples, local_audio.sample_rate));
        result.merge(breath.detect(local_audio.samples, local_audio.sample_rate));

        audio_core::RmsAnalyzer rms(local_audio.sample_rate, local_audio.sample_rate / 20,
                                    local_audio.sample_rate / 40);
        const auto rms_data = rms.analyze(local_audio.samples);

        const double duration = static_cast<double>(local_audio.samples.size()) / local_audio.sample_rate;
        audio_core::CompositeScore scorer(score_cfg_);
        const auto scores = scorer.compute(result.all(), duration);

        audio_core::Stft stft(local_audio.sample_rate, 2048, 512);
        const auto spec = stft.forward(local_audio.samples);

        audio_core::SpectralCleaner cleaner;
        const auto cleaned = cleaner.clean(local_audio.samples, local_audio.sample_rate, result.all(), clean_cfg_);

        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            markers_ = std::move(result);
            rms_frames_ = std::move(rms_data);
            score_frames_ = std::move(scores);
            spectrogram_ = std::move(spec);
            cleaned_audio_ = std::move(cleaned);
        }

        analyzing_ = false;
    });
}

void App::start_playback() {
    if (!has_audio_ || playing_) {
        return;
    }

    playing_ = true;
    if (playback_thread_.joinable()) {
        playback_thread_.join();
    }

    playback_thread_ = std::thread([this]() {
        audio_core::AudioData local;
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            local = audio_;
        }

        const double start = playhead_sec_.load();
        const int start_sample = static_cast<int>(start * local.sample_rate);
        std::vector<float> tail(local.samples.begin() + start_sample, local.samples.end());

        PaStream* stream = nullptr;
        Pa_OpenDefaultStream(&stream, 0, 1, paFloat32, local.sample_rate,
                             paFramesPerBufferUnspecified, nullptr, nullptr);
        Pa_StartStream(stream);

        std::size_t offset = 0;
        const int chunk = 1024;
        while (playing_ && offset < tail.size()) {
            const auto count = (std::min<std::size_t>)(chunk, tail.size() - offset);
            Pa_WriteStream(stream, tail.data() + offset, static_cast<unsigned long>(count));
            offset += count;
            playhead_sec_ = start + static_cast<double>(offset) / local.sample_rate;
        }

        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        playing_ = false;
    });
}

void App::stop_playback() {
    playing_ = false;
    if (playback_thread_.joinable()) {
        playback_thread_.join();
    }
}

void App::render_ui() {
    ImGui::Begin("Controls");
    if (ImGui::Button("Open Audio File")) {
        const auto path = open_file_dialog();
        if (!path.empty()) {
            try {
                std::lock_guard<std::mutex> lock(data_mutex_);
                file_path_ = path;
                audio_ = audio_core::AudioFile::load(path);
                has_audio_ = true;
                markers_ = {};
                rms_frames_.clear();
                score_frames_.clear();
                spectrogram_ = {};
                cleaned_audio_.clear();
                view_start_sec_ = 0.0;
                view_end_sec_ = (std::min)(30.0, static_cast<double>(audio_.samples.size()) / audio_.sample_rate);
                playhead_sec_ = 0.0;
            } catch (const std::exception& ex) {
                std::cerr << ex.what() << '\n';
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Analyze") && has_audio_) {
        run_analysis();
    }
    ImGui::SameLine();
    if (ImGui::Button("Play") && has_audio_) {
        start_playback();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        stop_playback();
    }

    if (has_audio_) {
        ImGui::Text("File: %s", file_path_.c_str());
        ImGui::Text("Duration: %.2f s | Sample rate: %d", static_cast<double>(audio_.samples.size()) / audio_.sample_rate,
                    audio_.sample_rate);
        ImGui::Text("Playhead: %.2f s", playhead_sec_.load());
        if (analyzing_) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Analyzing...");
        }
    }

    ImGui::Separator();
    ImGui::Text("Detector Settings");
    ImGui::SliderFloat("Pause RMS Threshold", reinterpret_cast<float*>(&pause_cfg_.rms_threshold), 0.001f, 0.1f,
                       "%.4f");
    ImGui::SliderFloat("Pause Min Duration (s)", reinterpret_cast<float*>(&pause_cfg_.min_duration_sec), 0.1f, 3.0f);
    ImGui::SliderFloat("Click Spike Ratio", reinterpret_cast<float*>(&transient_cfg_.spike_ratio), 2.0f, 12.0f);
    ImGui::SliderFloat("Breath HF Ratio", reinterpret_cast<float*>(&breath_cfg_.hf_ratio_threshold), 0.3f, 0.9f);
    ImGui::SliderFloat("Clean Attenuation", reinterpret_cast<float*>(&clean_cfg_.attenuation), 0.0f, 1.0f);

    if (ImGui::Button("Export Markers JSON") && has_audio_) {
        try {
            std::lock_guard<std::mutex> lock(data_mutex_);
            const auto out = std::filesystem::path(file_path_).replace_extension(".markers.json");
            markers_.save_json(out.string());
        } catch (const std::exception& ex) {
            std::cerr << ex.what() << '\n';
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Export Cleaned WAV") && has_audio_) {
        const auto path = save_file_dialog();
        if (!path.empty()) {
            try {
                std::lock_guard<std::mutex> lock(data_mutex_);
                const auto& export_audio = cleaned_audio_.empty() ? audio_.samples : cleaned_audio_;
                audio_core::AudioFile::save_mono(path, export_audio, audio_.sample_rate);
            } catch (const std::exception& ex) {
                std::cerr << ex.what() << '\n';
            }
        }
    }

    ImGui::Checkbox("Follow Playhead", &follow_playhead_);
    if (has_audio_) {
        const double duration = static_cast<double>(audio_.samples.size()) / audio_.sample_rate;
        ImGui::SliderFloat("View Start (s)", reinterpret_cast<float*>(&view_start_sec_), 0.0f,
                           static_cast<float>(duration));
        ImGui::SliderFloat("View End (s)", reinterpret_cast<float*>(&view_end_sec_), 0.0f,
                           static_cast<float>(duration));
        if (ImGui::Button("Reset View")) {
            view_start_sec_ = 0.0;
            view_end_sec_ = (std::min)(30.0, duration);
        }
    }
    ImGui::End();

    if (has_audio_) {
        ImGui::Begin("Markers");
        std::lock_guard<std::mutex> lock(data_mutex_);
        int remove_index = -1;
        for (int i = 0; i < static_cast<int>(markers_.all().size()); ++i) {
            const auto& m = markers_.all()[static_cast<std::size_t>(i)];
            ImGui::Text("%s: %.2f-%.2fs (score %.2f)", audio_core::marker_type_to_string(m.type).c_str(),
                        m.start_sec, m.end_sec, m.score);
            ImGui::SameLine();
            const std::string btn = "Remove##" + std::to_string(i);
            if (ImGui::SmallButton(btn.c_str())) {
                remove_index = i;
            }
        }
        if (remove_index >= 0) {
            auto updated = markers_.all();
            updated.erase(updated.begin() + remove_index);
            markers_ = {};
            markers_.merge(updated);
        }
        ImGui::End();
    }

    if (!has_audio_) {
        return;
    }

    audio_core::AudioData local_audio;
    audio_core::Markers local_markers;
    std::vector<audio_core::RmsFrame> local_rms;
    std::vector<audio_core::ScoreFrame> local_scores;
    audio_core::Spectrogram local_spec;
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        local_audio = audio_;
        local_markers = markers_;
        local_rms = rms_frames_;
        local_scores = score_frames_;
        local_spec = spectrogram_;
    }

    const double duration = static_cast<double>(local_audio.samples.size()) / local_audio.sample_rate;
    if (follow_playhead_) {
        const double play = playhead_sec_.load();
        view_start_sec_ = (std::max)(0.0, play - 5.0);
        view_end_sec_ = (std::min)(duration, view_start_sec_ + 10.0);
    }

    ImGui::Begin("Waveform & Markers");
    if (ImPlot::BeginPlot("Waveform", ImVec2(-1, 220))) {
        ImPlot::SetupAxes("Time (s)", "Amplitude", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisLimits(ImAxis_X1, view_start_sec_, view_end_sec_, ImGuiCond_Always);

        const int start_idx = (std::max)(0, static_cast<int>(view_start_sec_ * local_audio.sample_rate));
        const int end_idx = (std::min)(static_cast<int>(local_audio.samples.size()),
                                       static_cast<int>(view_end_sec_ * local_audio.sample_rate));
        const int step = (std::max)(1, (end_idx - start_idx) / 4000);

        std::vector<double> xs;
        std::vector<double> ys;
        for (int i = start_idx; i < end_idx; i += step) {
            xs.push_back(static_cast<double>(i) / local_audio.sample_rate);
            ys.push_back(local_audio.samples[static_cast<std::size_t>(i)]);
        }
        if (!xs.empty()) {
            ImPlot::PlotLine("wave", xs.data(), ys.data(), static_cast<int>(xs.size()));
        }

        for (const auto& m : local_markers.all()) {
            if (m.end_sec < view_start_sec_ || m.start_sec > view_end_sec_) {
                continue;
            }
            const auto c = marker_color(m.type);
            const double xs_fill[2] = {m.start_sec, m.end_sec};
            const double ys_lo[2] = {-1.0, -1.0};
            const double ys_hi[2] = {1.0, 1.0};
            ImPlot::PlotShaded("##marker", xs_fill, ys_lo, ys_hi, 2,
                               ImPlotSpec(ImPlotProp_FillColor, c, ImPlotProp_FillAlpha, c.w));
        }

        const double play_x = playhead_sec_.load();
        const double play_y[2] = {-1.0, 1.0};
        const double play_t[2] = {play_x, play_x};
        ImPlot::PlotLine("playhead", play_t, play_y, 2);

        ImPlot::EndPlot();
    }
    ImGui::End();

    ImGui::Begin("RMS Envelope");
    if (ImPlot::BeginPlot("RMS", ImVec2(-1, 160))) {
        std::vector<double> tx;
        std::vector<double> ry;
        for (const auto& f : local_rms) {
            if (f.time_sec >= view_start_sec_ && f.time_sec <= view_end_sec_) {
                tx.push_back(f.time_sec);
                ry.push_back(f.rms);
            }
        }
        if (!tx.empty()) {
            ImPlot::PlotLine("rms", tx.data(), ry.data(), static_cast<int>(tx.size()));
        }
        ImPlot::DragLineY(0, &pause_cfg_.rms_threshold, ImVec4(1, 0, 0, 1), 1.0f);
        ImPlot::EndPlot();
    }
    ImGui::End();

    ImGui::Begin("Composite Score Heatmap");
    if (ImPlot::BeginPlot("Score", ImVec2(-1, 140))) {
        std::vector<double> sx;
        std::vector<double> sy;
        for (const auto& s : local_scores) {
            if (s.time_sec >= view_start_sec_ && s.time_sec <= view_end_sec_) {
                sx.push_back(s.time_sec);
                sy.push_back(s.score);
            }
        }
        if (!sx.empty()) {
            ImPlot::PlotBars("score", sx.data(), sy.data(), static_cast<int>(sx.size()), 0.04);
        }
        ImPlot::EndPlot();
    }
    ImGui::End();

    ImGui::Begin("Spectrogram");
    if (!local_spec.frames.empty() && ImPlot::BeginPlot("Spec", ImVec2(-1, 260))) {
        const int bins = static_cast<int>(local_spec.frames[0].size());
        const int frames = static_cast<int>(local_spec.frames.size());
        std::vector<float> heat(static_cast<std::size_t>(bins * frames), 0.0f);

        for (int t = 0; t < frames; ++t) {
            for (int b = 0; b < bins; ++b) {
                heat[static_cast<std::size_t>(t * bins + b)] =
                    static_cast<float>(std::log10(1e-12 + std::abs(local_spec.frames[static_cast<std::size_t>(t)][static_cast<std::size_t>(b)])));
            }
        }

        double min_v = heat.front();
        double max_v = heat.front();
        for (float v : heat) {
            min_v = (std::min)(min_v, static_cast<double>(v));
            max_v = (std::max)(max_v, static_cast<double>(v));
        }

        ImPlot::PushColormap(ImPlotColormap_Viridis);
        ImPlot::PlotHeatmap("spec", heat.data(), bins, frames, min_v, max_v, nullptr,
                            ImPlotPoint(0.0, 0.0),
                            ImPlotPoint(duration, static_cast<double>(local_spec.sample_rate) / 2.0));
        ImPlot::PopColormap();
        ImPlot::EndPlot();
    }
    ImGui::End();
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
