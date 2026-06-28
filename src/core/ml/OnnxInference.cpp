#include "audio_core/ml/OnnxInference.hpp"

#include <fstream>

namespace audio_core {

OnnxInference::OnnxInference(MlInferenceConfig config) : config_(std::move(config)) {}

bool OnnxInference::is_available() const {
    if (!config_.enabled || config_.model_path.empty()) {
        return false;
    }
    std::ifstream in(config_.model_path, std::ios::binary);
    return in.good();
}

std::vector<Marker> OnnxInference::infer(const std::vector<float>& /*samples*/, int /*sample_rate*/) const {
    // Scaffold: ONNX Runtime integration is deferred until a trained CRNN model exists.
    // Heuristic detectors remain the default path.
    return {};
}

}  // namespace audio_core
