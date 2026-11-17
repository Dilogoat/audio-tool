#pragma once

#include <complex>
#include <cstddef>
#include <functional>
#include <vector>

namespace audio_core {

struct Spectrogram {
    int sample_rate = 44100;
    int fft_size = 2048;
    int hop_size = 512;
    std::vector<std::vector<std::complex<double>>> frames;
    std::vector<double> frame_times;
};

class Stft {
public:
    Stft(int sample_rate, int fft_size, int hop_size);
    ~Stft();

    Stft(const Stft&) = delete;
    Stft& operator=(const Stft&) = delete;

    // Stores every frame. Memory grows with input length (~16 KB per frame at
    // fft 2048), so prefer for_each_frame() for long recordings.
    Spectrogram forward(const std::vector<float>& samples);

    // Streams frames without storing them. `fn(start_sample, bins)` is called
    // once per frame, where start_sample is the first input sample of the
    // window (the window centre is start_sample + fft_size / 2).
    void for_each_frame(const std::vector<float>& samples,
                        const std::function<void(std::size_t, const std::vector<std::complex<double>>&)>& fn);
    std::vector<float> inverse(const Spectrogram& spec);

    int fft_size() const { return fft_size_; }

private:
    int sample_rate_;
    int fft_size_;
    int hop_size_;
    std::vector<double> in_;
    std::vector<double> out_time_;
    std::vector<std::complex<double>> out_freq_;
    void* plan_forward_ = nullptr;
    void* plan_inverse_ = nullptr;
};

}  // namespace audio_core
