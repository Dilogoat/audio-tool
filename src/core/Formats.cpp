#include "audio_core/Formats.hpp"

#include <algorithm>
#include <sstream>

namespace audio_core {

namespace {

std::string ext_without_dot(std::string_view ext) {
    if (!ext.empty() && ext.front() == '.') {
        return std::string(ext.substr(1));
    }
    return std::string(ext);
}

}  // namespace

const std::vector<std::string>& SupportedFormats::input_extensions() {
    static const std::vector<std::string> exts = {
        ".wav", ".wave",
        ".mp3",
        ".flac",
        ".ogg",
        ".opus",
        ".aiff", ".aif",
        ".w64", ".rf64",
        ".caf",
    };
    return exts;
}

bool SupportedFormats::is_input_extension(std::string_view ext) {
    for (const auto& supported : input_extensions()) {
        if (supported == ext) {
            return true;
        }
    }
    return false;
}

std::string SupportedFormats::display_list() {
    // Curated labels for the UI — not every extension gets its own bullet.
    return "WAV, MP3, FLAC, OGG, Opus, AIFF, CAF, W64";
}

#ifdef _WIN32
std::string SupportedFormats::win32_open_filter() {
    std::ostringstream patterns;
    bool first = true;
    for (const auto& ext : input_extensions()) {
        if (!first) {
            patterns << ';';
        }
        patterns << '*' << ext;
        first = false;
    }

    const std::string pattern_str = patterns.str();
    return std::string("Audio Files (") + pattern_str + ")\0" + pattern_str +
           "\0All Files (*.*)\0*.*\0\0";
}
#endif

}  // namespace audio_core
