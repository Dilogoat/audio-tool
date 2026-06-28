#pragma once

#include "audio_core/Markers.hpp"

#include <string>
#include <vector>

namespace audio_core {

enum class EventClass {
    Speech,
    Breath,
    Click,
    Silence
};

struct MlInferenceConfig {
    std::string model_path;
    int fft_size = 512;
    int hop_size = 256;
    bool enabled = false;
};

class OnnxInference {
public:
    explicit OnnxInference(MlInferenceConfig config = {});

    bool is_available() const;
    std::vector<Marker> infer(const std::vector<float>& samples, int sample_rate) const;

private:
    MlInferenceConfig config_;
};

}  // namespace audio_core
