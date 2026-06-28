#include "audio_core/CompositeScore.hpp"

#include <algorithm>
#include <cmath>

namespace audio_core {

CompositeScore::CompositeScore(CompositeScoreConfig config) : config_(config) {}

std::vector<ScoreFrame> CompositeScore::compute(const std::vector<Marker>& markers,
                                                double duration_sec) const {
    if (duration_sec <= 0.0) {
        return {};
    }

    const int frame_count =
        std::max(1, static_cast<int>(std::ceil(duration_sec / config_.frame_duration_sec)));
    std::vector<ScoreFrame> frames(static_cast<std::size_t>(frame_count));

    for (int i = 0; i < frame_count; ++i) {
        frames[static_cast<std::size_t>(i)].time_sec = i * config_.frame_duration_sec;
    }

    auto add_weight = [&](const Marker& marker, double weight) {
        const int start_idx =
            std::max(0, static_cast<int>(std::floor(marker.start_sec / config_.frame_duration_sec)));
        const int end_idx = std::min(
            frame_count - 1,
            static_cast<int>(std::ceil(marker.end_sec / config_.frame_duration_sec)));
        for (int i = start_idx; i <= end_idx; ++i) {
            frames[static_cast<std::size_t>(i)].score += weight * marker.score;
        }
    };

    for (const auto& marker : markers) {
        switch (marker.type) {
            case MarkerType::Click:
                add_weight(marker, config_.click_weight);
                break;
            case MarkerType::Breath:
                add_weight(marker, config_.breath_weight);
                break;
            case MarkerType::Pause:
                add_weight(marker, config_.pause_weight);
                break;
            default:
                break;
        }
    }

    double max_score = 0.0;
    for (const auto& frame : frames) {
        max_score = std::max(max_score, frame.score);
    }
    if (max_score > 0.0) {
        for (auto& frame : frames) {
            frame.score /= max_score;
        }
    }

    return frames;
}

}  // namespace audio_core
