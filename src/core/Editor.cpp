#include "audio_core/Editor.hpp"

#include <algorithm>
#include <cmath>

namespace audio_core {

namespace {

struct Interval {
    long long start = 0;
    long long end = 0;
};

bool type_selected(MarkerType type, const EditConfig& cfg) {
    switch (type) {
        case MarkerType::Pause:
            return cfg.cut_pauses;
        case MarkerType::Click:
            return cfg.cut_clicks;
        case MarkerType::Breath:
            return cfg.cut_breaths;
        default:
            return false;
    }
}

}  // namespace

EditResult apply_cuts(const std::vector<float>& samples, int sample_rate,
                      const std::vector<Marker>& markers, const EditConfig& config) {
    EditResult result;
    const long long n = static_cast<long long>(samples.size());
    result.original_sec = sample_rate > 0 ? static_cast<double>(n) / sample_rate : 0.0;

    if (samples.empty() || sample_rate <= 0) {
        result.samples = samples;
        result.edited_sec = result.original_sec;
        return result;
    }

    const long long pause_keep = static_cast<long long>((std::max)(0.0, config.pause_keep_sec) * sample_rate);

    // Collect cut intervals from selected marker types.
    std::vector<Interval> cuts;
    for (const auto& m : markers) {
        if (!type_selected(m.type, config)) {
            continue;
        }
        long long s = static_cast<long long>(m.start_sec * sample_rate);
        long long e = static_cast<long long>(m.end_sec * sample_rate);
        s = (std::max)(0LL, (std::min)(s, n));
        e = (std::max)(0LL, (std::min)(e, n));
        if (m.type == MarkerType::Pause && pause_keep > 0) {
            // Leave a small gap centered in the pause.
            const long long mid = (s + e) / 2;
            s = (std::max)(s, mid - pause_keep / 2);
            e = (std::min)(e, mid + pause_keep / 2);
        }
        if (e > s) {
            cuts.push_back({s, e});
        }
    }

    if (cuts.empty()) {
        result.samples = samples;
        result.edited_sec = result.original_sec;
        return result;
    }

    std::sort(cuts.begin(), cuts.end(),
              [](const Interval& a, const Interval& b) { return a.start < b.start; });

    // Merge overlapping/adjacent cut intervals.
    std::vector<Interval> merged;
    merged.push_back(cuts.front());
    for (std::size_t i = 1; i < cuts.size(); ++i) {
        if (cuts[i].start <= merged.back().end) {
            merged.back().end = (std::max)(merged.back().end, cuts[i].end);
        } else {
            merged.push_back(cuts[i]);
        }
    }
    result.regions_cut = static_cast<int>(merged.size());

    // Build kept intervals as the complement of merged cuts.
    std::vector<Interval> kept;
    long long cursor = 0;
    for (const auto& c : merged) {
        if (c.start > cursor) {
            kept.push_back({cursor, c.start});
        }
        result.removed_sec += static_cast<double>(c.end - c.start) / sample_rate;
        cursor = c.end;
    }
    if (cursor < n) {
        kept.push_back({cursor, n});
    }

    const long long fade = (std::max)(0LL, static_cast<long long>(config.crossfade_ms / 1000.0 * sample_rate));

    std::vector<float>& out = result.samples;
    out.reserve(static_cast<std::size_t>(n));

    for (const auto& seg : kept) {
        const float* seg_data = samples.data() + seg.start;
        const long long seg_len = seg.end - seg.start;
        if (seg_len <= 0) {
            continue;
        }

        if (out.empty() || fade == 0) {
            out.insert(out.end(), seg_data, seg_data + seg_len);
            continue;
        }

        const long long blend = (std::min)({fade, static_cast<long long>(out.size()), seg_len});
        const long long out_start = static_cast<long long>(out.size()) - blend;
        for (long long i = 0; i < blend; ++i) {
            const double w = static_cast<double>(i + 1) / (blend + 1);
            const double a = out[static_cast<std::size_t>(out_start + i)];
            const double b = seg_data[i];
            out[static_cast<std::size_t>(out_start + i)] = static_cast<float>(a * (1.0 - w) + b * w);
        }
        out.insert(out.end(), seg_data + blend, seg_data + seg_len);
    }

    result.edited_sec = static_cast<double>(out.size()) / sample_rate;
    return result;
}

}  // namespace audio_core
