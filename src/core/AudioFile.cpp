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

    AudioData data;
    data.sample_rate = info.samplerate;
    data.source_channels = info.channels;
    // Everything downstream works on a mono mixdown, so `channels` describes
    // the samples actually held here, not the source file.
    data.channels = 1;

    // Decode in chunks and mix down as we go, so a long stereo file never
    // needs a full interleaved copy alongside the mono result. info.frames is
    // only a hint for compressed formats, so read until the decoder stops.
    const int channels = (std::max)(1, info.channels);
    if (info.frames > 0) {
        data.samples.reserve(static_cast<std::size_t>(info.frames));
    }
    constexpr sf_count_t kChunkFrames = 65536;
    std::vector<float> chunk(static_cast<std::size_t>(kChunkFrames) * channels);
    for (;;) {
        const sf_count_t got = sf_readf_float(file, chunk.data(), kChunkFrames);
        if (got <= 0) {
            break;
        }
        if (channels == 1) {
            data.samples.insert(data.samples.end(), chunk.begin(), chunk.begin() + got);
            continue;
        }
        for (sf_count_t i = 0; i < got; ++i) {
            float sum = 0.0f;
            for (int ch = 0; ch < channels; ++ch) {
                sum += chunk[static_cast<std::size_t>(i) * channels + ch];
            }
            data.samples.push_back(sum / static_cast<float>(channels));
        }
    }
    const int err = sf_error(file);
    sf_close(file);
    if (err != SF_ERR_NO_ERROR) {
        // Don't hand back silently truncated audio from a corrupt file.
        throw std::runtime_error(std::string("Failed to read audio samples: ") + sf_error_number(err));
    }
    data.samples.shrink_to_fit();

    return data;
}

namespace {

void write_wav(const std::string& path, const float* samples, std::size_t count, int channels, int sample_rate) {
    if (channels <= 0 || count % static_cast<std::size_t>(channels) != 0) {
        throw std::runtime_error("Sample count is not a multiple of the channel count");
    }
    SF_INFO info{};
    info.samplerate = sample_rate;
    info.channels = channels;
    info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* file = sf_open(path.c_str(), SFM_WRITE, &info);
    if (!file) {
        throw std::runtime_error(std::string("Failed to create audio file: ") + sf_strerror(nullptr));
    }

    // Without this, float samples outside [-1, 1] wrap around when converted to
    // 16-bit PCM instead of clipping.
    sf_command(file, SFC_SET_CLIPPING, nullptr, SF_TRUE);

    const sf_count_t frames = static_cast<sf_count_t>(count / static_cast<std::size_t>(channels));
    const sf_count_t written = sf_writef_float(file, samples, frames);
    sf_close(file);

    if (written != frames) {
        throw std::runtime_error("Failed to write audio samples");
    }
}

}  // namespace

void AudioFile::save(const std::string& path, const AudioData& data) {
    write_wav(path, data.samples.data(), data.samples.size(), data.channels, data.sample_rate);
}

void AudioFile::save_mono(const std::string& path, const std::vector<float>& samples, int sample_rate) {
    // Writes straight from the caller's buffer; copying a long recording into
    // an AudioData first doubled peak memory during export.
    write_wav(path, samples.data(), samples.size(), 1, sample_rate);
}

}  // namespace audio_core
