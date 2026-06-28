#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace audio_core {

struct AudioData {
    std::vector<float> samples;
    int sample_rate = 44100;
    int channels = 1;
};

class AudioFile {
public:
    static AudioData load(const std::string& path);
    static void save(const std::string& path, const AudioData& data);
    static void save_mono(const std::string& path, const std::vector<float>& samples, int sample_rate);
};

}  // namespace audio_core
