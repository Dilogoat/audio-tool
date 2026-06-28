#pragma once

#include "audio_core/AudioFile.hpp"
#include "audio_core/CompositeScore.hpp"
#include "audio_core/Editor.hpp"
#include "audio_core/Markers.hpp"
#include "audio_core/Pipeline.hpp"
#include "audio_core/RmsAnalyzer.hpp"
#include "audio_core/SpectralCleaner.hpp"
#include "audio_core/Stft.hpp"
#include "audio_core/detectors/BreathDetector.hpp"
#include "audio_core/detectors/PauseDetector.hpp"
#include "audio_core/detectors/TransientDetector.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct GLFWwindow;

class App {
public:
    App();
    ~App();

    bool init();
    void run();
    void shutdown();

private:
    struct UiSnapshot;

    void process_frame();
    void render_ui();
    void render_toolbar();
    void render_sidebar(const UiSnapshot& snap);
    void render_plots(const UiSnapshot& snap);
    void open_audio();
    void run_analysis();
    void start_playback();
    void stop_playback();

    GLFWwindow* window_ = nullptr;
    std::string file_path_;
    audio_core::AudioData audio_;
    audio_core::Markers markers_;
    std::vector<audio_core::RmsFrame> rms_frames_;
    std::vector<audio_core::ScoreFrame> score_frames_;
    audio_core::Spectrogram spectrogram_;
    std::vector<float> cleaned_audio_;

    bool has_audio_ = false;
    bool analyzing_ = false;
    std::thread analysis_thread_;
    std::mutex data_mutex_;

    double view_start_sec_ = 0.0;
    double view_end_sec_ = 10.0;
    bool follow_playhead_ = false;
    int marker_remove_request_ = -1;

    audio_core::PauseDetectorConfig pause_cfg_{};
    audio_core::TransientDetectorConfig transient_cfg_{};
    audio_core::BreathDetectorConfig breath_cfg_{};
    audio_core::CompositeScoreConfig score_cfg_{};
    audio_core::CleanConfig clean_cfg_{};
    audio_core::EditConfig edit_cfg_{};

    std::atomic<bool> playing_{false};
    std::atomic<double> playhead_sec_{0.0};
    std::thread playback_thread_;
};
