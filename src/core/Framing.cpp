#include "audio_core/Framing.hpp"

#include <cmath>

namespace audio_core {

std::vector<float> hann_window(int size) {
    std::vector<float> window(static_cast<std::size_t>(size));
    if (size <= 1) {
        if (!window.empty()) {
            window[0] = 1.0f;
        }
        return window;
    }
    for (int i = 0; i < size; ++i) {
        window[static_cast<std::size_t>(i)] =
            0.5f * (1.0f - static_cast<float>(std::cos(2.0 * M_PI * i / (size - 1))));
    }
    return window;
}

std::vector<float> apply_window(const std::vector<float>& frame, const std::vector<float>& window) {
    std::vector<float> out(frame.size());
    for (std::size_t i = 0; i < frame.size(); ++i) {
        out[i] = frame[i] * window[i];
    }
    return out;
}

}  // namespace audio_core
