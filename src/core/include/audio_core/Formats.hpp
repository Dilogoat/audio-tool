#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace audio_core {

// Canonical list of input extensions libsndfile can decode in this build
// (WAV/AIFF/FLAC/OGG/Opus + MP3 via vcpkg external-libs + mpeg features).
class SupportedFormats {
public:
    static const std::vector<std::string>& input_extensions();

    // `ext` must already be lowercased and include the leading dot.
    static bool is_input_extension(std::string_view ext);

    // Human-readable list for UI copy, e.g. "WAV, MP3, FLAC, ..."
    static std::string display_list();

#ifdef _WIN32
    // Double-null-terminated filter string for OPENFILENAMEA.
    static std::string win32_open_filter();
#endif
};

}  // namespace audio_core
