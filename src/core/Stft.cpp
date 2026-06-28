#include "audio_core/Stft.hpp"

#include "audio_core/Framing.hpp"

#include <fftw3.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace audio_core {

Stft::Stft(int sample_rate, int fft_size, int hop_size)
    : sample_rate_(sample_rate), fft_size_(fft_size), hop_size_(hop_size) {
    if (sample_rate_ <= 0 || fft_size_ <= 0 || hop_size_ <= 0) {
        throw std::invalid_argument("Invalid STFT parameters");
    }

    in_.resize(static_cast<std::size_t>(fft_size_));
    out_time_.resize(static_cast<std::size_t>(fft_size_));
    out_freq_.resize(static_cast<std::size_t>(fft_size_ / 2 + 1));

    plan_forward_ = fftw_plan_dft_r2c_1d(
        fft_size_, in_.data(), reinterpret_cast<fftw_complex*>(out_freq_.data()), FFTW_ESTIMATE);
    plan_inverse_ = fftw_plan_dft_c2r_1d(
        fft_size_, reinterpret_cast<fftw_complex*>(out_freq_.data()), out_time_.data(), FFTW_ESTIMATE);
}

Stft::~Stft() {
    if (plan_forward_) {
        fftw_destroy_plan(static_cast<fftw_plan>(plan_forward_));
    }
    if (plan_inverse_) {
        fftw_destroy_plan(static_cast<fftw_plan>(plan_inverse_));
    }
}

Spectrogram Stft::forward(const std::vector<float>& samples) {
    Spectrogram spec;
    spec.sample_rate = sample_rate_;
    spec.fft_size = fft_size_;
    spec.hop_size = hop_size_;

    if (samples.empty()) {
        return spec;
    }

    const auto window = hann_window(fft_size_);

    for (std::size_t start = 0; start < samples.size(); start += static_cast<std::size_t>(hop_size_)) {
        std::fill(in_.begin(), in_.end(), 0.0);
        const std::size_t copy_count =
            std::min(static_cast<std::size_t>(fft_size_), samples.size() - start);
        for (std::size_t i = 0; i < copy_count; ++i) {
            in_[i] = static_cast<double>(samples[start + i]) * window[static_cast<std::size_t>(i)];
        }

        fftw_execute_dft_r2c(static_cast<fftw_plan>(plan_forward_), in_.data(),
                             reinterpret_cast<fftw_complex*>(out_freq_.data()));

        spec.frames.push_back(out_freq_);
        spec.frame_times.push_back(static_cast<double>(start) / sample_rate_);
    }

    return spec;
}

std::vector<float> Stft::inverse(const Spectrogram& spec) {
    if (spec.frames.empty()) {
        return {};
    }

    const int fft_size = spec.fft_size;
    const int hop_size = spec.hop_size;
    const auto window = hann_window(fft_size);

    std::vector<double> acc(static_cast<std::size_t>((spec.frames.size() + 1) * hop_size + fft_size), 0.0);
    std::vector<double> norm(acc.size(), 0.0);

    for (std::size_t f = 0; f < spec.frames.size(); ++f) {
        for (std::size_t i = 0; i < out_freq_.size(); ++i) {
            out_freq_[i] = spec.frames[f][i];
        }
        fftw_execute_dft_c2r(static_cast<fftw_plan>(plan_inverse_), reinterpret_cast<fftw_complex*>(out_freq_.data()),
                               out_time_.data());

        const std::size_t start = static_cast<std::size_t>(f) * hop_size;
        for (int i = 0; i < fft_size; ++i) {
            const std::size_t idx = start + static_cast<std::size_t>(i);
            const double w = window[static_cast<std::size_t>(i)];
            acc[idx] += out_time_[static_cast<std::size_t>(i)] * w / static_cast<double>(fft_size);
            norm[idx] += w * w;
        }
    }

    std::vector<float> result(acc.size());
    for (std::size_t i = 0; i < acc.size(); ++i) {
        result[i] = norm[i] > 1e-12 ? static_cast<float>(acc[i] / norm[i]) : 0.0f;
    }
    return result;
}

}  // namespace audio_core
