#pragma once

#include "audio_core/AudioFile.hpp"
#include "audio_core/CompositeScore.hpp"
#include "audio_core/Markers.hpp"
#include "audio_core/SpectralCleaner.hpp"
#include "audio_core/detectors/BreathDetector.hpp"
#include "audio_core/detectors/PauseDetector.hpp"
#include "audio_core/detectors/TransientDetector.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace audio_core {

// Full configuration for an analysis run. Shared by the GUI and the CLI so a
// preset behaves identically in both. Serializable to/from JSON.
struct PipelineConfig {
    PauseDetectorConfig pause{};
    TransientDetectorConfig transient{};
    BreathDetectorConfig breath{};
    CompositeScoreConfig score{};
    CleanConfig clean{};

    bool detect_pauses = true;
    bool detect_clicks = true;
    bool detect_breaths = true;

    static PipelineConfig from_json_file(const std::string& path);
    static PipelineConfig from_json(const nlohmann::json& root);
    nlohmann::json to_json() const;
    void to_json_file(const std::string& path) const;
};

struct PipelineResult {
    Markers markers{};
    std::vector<ScoreFrame> scores{};

    int sample_rate = 0;
    std::size_t sample_count = 0;
    double duration_sec = 0.0;

    int pause_count = 0;
    int click_count = 0;
    int breath_count = 0;
    double peak_score = 0.0;

    bool cleaned_produced = false;
    std::vector<float> cleaned{};
};

class Pipeline {
public:
    explicit Pipeline(PipelineConfig config = {});

    const PipelineConfig& config() const { return config_; }
    PipelineConfig& config() { return config_; }

    // Runs all enabled detectors and the composite score. If produce_cleaned is
    // true, also renders a cleaned copy via SpectralCleaner.
    PipelineResult analyze(const AudioData& audio, bool produce_cleaned) const;

private:
    PipelineConfig config_;
};

// Writes a machine-readable per-file report (input metadata + marker summary).
void save_report_json(const std::string& path, const std::string& input_path,
                      const PipelineResult& result);

}  // namespace audio_core
