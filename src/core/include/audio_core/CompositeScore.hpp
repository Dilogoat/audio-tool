#pragma once

#include "audio_core/Markers.hpp"

#include <vector>

namespace audio_core {

struct ScoreFrame {
    double time_sec = 0.0;
    double score = 0.0;
};

struct CompositeScoreConfig {
    double click_weight = 0.8;
    double breath_weight = 0.5;
    double pause_weight = 0.2;
    double frame_duration_sec = 0.05;
};

class CompositeScore {
public:
    explicit CompositeScore(CompositeScoreConfig config = {});

    std::vector<ScoreFrame> compute(const std::vector<Marker>& markers, double duration_sec) const;

private:
    CompositeScoreConfig config_;
};

}  // namespace audio_core
