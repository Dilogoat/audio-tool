#pragma once

#include <vector>

namespace audio_core {

std::vector<float> hann_window(int size);
std::vector<float> apply_window(const std::vector<float>& frame, const std::vector<float>& window);

}  // namespace audio_core
