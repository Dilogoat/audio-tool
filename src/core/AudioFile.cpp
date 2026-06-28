#include "audio_core/AudioFile.hpp"

#include <sndfile.h>

#include <algorithm>
#include <stdexcept>

namespace audio_core {

AudioData AudioFile::load(const std::string& path) {
    SF_INFO info{};
    SNDFILE* file = sf_open(path.c_str(), SFM_READ, &info);
    if (!file) {
        throw std::runtime_error(std::string("Failed to open audio file: ") + sf_strerror(nullptr));
    }

    std::vector<float> buffer(static_cast<std::size_t>(info.frames) * info.channels);
    const sf_count_t read = sf_readf_float(file, buffer.data(), info.frames);
    sf_close(file);

    if (read < 0) {
        throw std::runtime_error("Failed to read audio samples");
    }

    buffer.resize(static_cast<std::size_t>(read) * info.channels);

    AudioData data;
    data.sample_rate = info.samplerate;
    data.channels = info.channels;

    if (info.channels == 1) {
        data.samples = std::move(buffer);
    } else {
        data.samples.resize(static_cast<std::size_t>(read));
        for (sf_count_t i = 0; i < read; ++i) {
            float sum = 0.0f;
            for (int ch = 0; ch < info.channels; ++ch) {
                sum += buffer[static_cast<std::size_t>(i) * info.channels + ch];
            }
            data.samples[static_cast<std::size_t>(i)] = sum / static_cast<float>(info.channels);
        }
    }

    return data;
}

void AudioFile::save(const std::string& path, const AudioData& data) {
    SF_INFO info{};
    info.samplerate = data.sample_rate;
    info.channels = data.channels;
    info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* file = sf_open(path.c_str(), SFM_WRITE, &info);
    if (!file) {
        throw std::runtime_error(std::string("Failed to create audio file: ") + sf_strerror(nullptr));
    }

    const sf_count_t written = sf_writef_float(file, data.samples.data(),
                                               static_cast<sf_count_t>(data.samples.size() / data.channels));
    sf_close(file);

    if (written < 0) {
        throw std::runtime_error("Failed to write audio samples");
    }
}

void AudioFile::save_mono(const std::string& path, const std::vector<float>& samples, int sample_rate) {
    AudioData data;
    data.samples = samples;
    data.sample_rate = sample_rate;
    data.channels = 1;
    save(path, data);
}

}  // namespace audio_core
