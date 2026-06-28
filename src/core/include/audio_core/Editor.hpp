#pragma once

#include "audio_core/Markers.hpp"

#include <vector>

namespace audio_core {

// Controls which marker types are physically removed from the audio and how the
// resulting joins are smoothed. Cutting is destructive to the *copy* only; the
// source file is never modified by this module.
struct EditConfig {
    bool cut_pauses = true;
    bool cut_clicks = false;
    bool cut_breaths = false;
    double crossfade_ms = 5.0;
    // For pauses, optionally keep a short gap instead of removing entirely.
    double pause_keep_sec = 0.0;
};

struct EditResult {
    std::vector<float> samples;
    double original_sec = 0.0;
    double edited_sec = 0.0;
    double removed_sec = 0.0;
    int regions_cut = 0;
};

// Removes the selected marker regions and re-stitches the kept audio with a
// short equal-power-ish crossfade at each join to avoid clicks.
EditResult apply_cuts(const std::vector<float>& samples, int sample_rate,
                      const std::vector<Marker>& markers, const EditConfig& config = {});

}  // namespace audio_core
