#pragma once

#include "audio_core/AudioFile.hpp"
#include "audio_core/CompositeScore.hpp"
#include "audio_core/Editor.hpp"
#include "audio_core/MarkerExports.hpp"
#include "audio_core/Markers.hpp"
#include "audio_core/Pipeline.hpp"
#include "audio_core/RmsAnalyzer.hpp"
#include "audio_core/Session.hpp"
#include "audio_core/SpectralCleaner.hpp"
#include "audio_core/Stft.hpp"
#include "audio_core/detectors/BreathDetector.hpp"
#include "audio_core/detectors/PauseDetector.hpp"
#include "audio_core/detectors/TransientDetector.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
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
    void load_session();
    void run_analysis();
    void start_playback();
    void start_playback_segment(double start_sec, double end_sec);
    void stop_playback();
    void preview_marker_at(int index);
    audio_core::Session build_session_snapshot() const;
    audio_core::PipelineConfig current_config() const;

    enum class PlaybackSource { Original, Cleaned, Edited };

    GLFWwindow* window_ = nullptr;
    std::string file_path_;
    // Immutable once loaded and shared by pointer, so the UI snapshot, the
    // analysis thread and playback never deep-copy the whole recording.
    std::shared_ptr<const audio_core::AudioData> audio_ = std::make_shared<audio_core::AudioData>();
    // Bumped whenever a new file or session is loaded; background work started
    // against an older generation discards its results.
    std::uint64_t audio_generation_ = 0;
    audio_core::Markers markers_;
    std::vector<audio_core::ReviewDecision> review_decisions_;
    std::vector<audio_core::RmsFrame> rms_frames_;
    std::vector<audio_core::ScoreFrame> score_frames_;
    // Display-only spectrogram, decimated to a fixed size at analysis time.
    struct SpectrogramImage {
        std::vector<float> values;  // column-major, row 0 = highest frequency band
        int rows = 0;
        int cols = 0;
        double col_sec = 0.0;  // duration covered by one column
        double max_hz = 0.0;
        float min_v = 0.0f;
        float max_v = 1.0f;
    };
    std::shared_ptr<const SpectrogramImage> spectrogram_;
    static std::shared_ptr<const SpectrogramImage> build_spectrogram_image(const audio_core::AudioData& audio);
    std::shared_ptr<const std::vector<float>> edited_audio_;
    std::string status_error_;

    bool has_audio_ = false;
    std::atomic<bool> analyzing_{false};
    bool edited_cache_dirty_ = true;
    std::thread analysis_thread_;
    std::mutex data_mutex_;

    double view_start_sec_ = 0.0;
    double view_end_sec_ = 10.0;
    bool follow_playhead_ = false;
    int marker_remove_request_ = -1;
    int preview_marker_request_ = -1;
    int selected_marker_ = -1;
    // Review decision changes are requested from the UI thread and applied under
    // data_mutex_ in render_ui, so the analysis thread can never race the write.
    int decision_request_index_ = -1;
    audio_core::ReviewDecision decision_request_value_ = audio_core::ReviewDecision::Pending;
    PlaybackSource playback_source_ = PlaybackSource::Original;

    audio_core::PauseDetectorConfig pause_cfg_{};
    audio_core::TransientDetectorConfig transient_cfg_{};
    audio_core::BreathDetectorConfig breath_cfg_{};
    audio_core::CompositeScoreConfig score_cfg_{};
    audio_core::CleanConfig clean_cfg_{};
    audio_core::EditConfig edit_cfg_{};
    bool detect_pauses_ = true;
    bool detect_clicks_ = true;
    bool detect_breaths_ = true;

    std::atomic<bool> playing_{false};
    std::atomic<double> playhead_sec_{0.0};
    std::thread playback_thread_;
};
