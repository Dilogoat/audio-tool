# Audio Cleanup & Analysis Tool

Native C++ desktop application for detecting non-speech artifacts in long-form voice recordings and visualizing actionable cleanup markers.

## Features

- **Pause detection** via RMS energy thresholding
- **Click/plosive detection** via transient spike analysis
- **Breath detection** via spectral high-frequency heuristics
- **Composite score heatmap** weighting clicks (0.8), breaths (0.5), pauses (0.2)
- **Spectrogram overlay** and waveform visualization (Dear ImGui + ImPlot)
- **Marker export** to JSON
- **Spectral cleanup preview** and cleaned WAV export
- **ML scaffold** for future CRNN ONNX inference

## Requirements

- Windows 10/11
- Visual Studio 2022 Build Tools (C++ workload)
- CMake 3.24+
- Ninja
- Python 3 (for fixture generation)

## Build

```powershell
cd C:\Users\jonat\CODE\audio-tool
if (-not (Test-Path external\vcpkg\vcpkg.exe)) {
  git clone --depth 1 https://github.com/microsoft/vcpkg.git external\vcpkg
  .\external\vcpkg\bootstrap-vcpkg.bat -disableMetrics
}
python scripts\gen_fixtures.py
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Run the app:

```powershell
.\build\audio_tool.exe
```

## Command-line interface (batch processing)

`audio_tool_cli` is a headless build that shares the exact same DSP pipeline as
the GUI. It never modifies source files - all results are written to a separate
output folder.

```powershell
# Analyze a single file (outputs to <input_dir>/audio-tool-out)
audio_tool_cli analyze "D:\recordings\episode.wav"

# Bulk-process a folder, also writing cleaned WAVs
audio_tool_cli batch "D:\recordings" --out-dir "D:\out" --clean --recursive

# Use a saved preset, markers only
audio_tool_cli batch "D:\recordings" --config presets\podcast.json

# Physically cut pauses + breaths and re-stitch (writes <name>.edited.wav)
audio_tool_cli batch "D:\recordings" --out-dir "D:\out" --cut pauses,breaths

# Export Audacity labels for manual editing in a DAW
audio_tool_cli analyze "D:\recordings\episode.wav" --labels

# Write a default config you can edit
audio_tool_cli init-config mypreset.json

# See everything
audio_tool_cli help
```

Per input `<name>.<ext>` the CLI writes (into the output folder, never over the
source):

- `<name>.markers.json` - detected events timeline
- `<name>.report.json` - per-file summary (duration, counts, peak score)
- `<name>.cleaned.wav` - cleaned audio (only with `--clean`)
- `<name>.edited.wav` - cut + re-stitched audio (only with `--cut`)
- `<name>.labels.txt` - Audacity label track (only with `--labels`)
- `batch-summary.json` - whole-run summary (batch mode)

Key options: `--out-dir`, `--config`, `--clean`, `--cut <pauses,clicks,breaths>`,
`--crossfade-ms <ms>`, `--labels`, `-r/--recursive`, `--force` (overwrite previous
outputs), `--dry-run`, `--no-pauses/--no-clicks/--no-breaths`,
`--log-level <error|warn|info|debug>`, `--log-file <path>`.

`--clean` attenuates artifacts in place (same length); `--cut` physically removes the
flagged regions and crossfades the joins, producing a shorter file. The source file is
never modified.

Exit codes: `0` ok, `1` partial failure, `2` bad args, `3` fatal.

## Download / run (no build required)

A portable Windows x64 build is in the [`dist/`](dist/) folder. Download the
whole folder, keep the files together, and run `audio_tool.exe`. See
[`dist/README.txt`](dist/README.txt) for usage.

## Project layout

- `src/core/` - DSP engine and detectors
- `src/app/` - Desktop GUI
- `tests/unit/` - Catch2 unit tests
- `scripts/` - Synthetic fixture generator
- `ml/` - Future training/export pipeline
- `external/vcpkg/` - Dependency manager

## License note

FFTW3 is GPL. Suitable for private development; evaluate license implications before commercial distribution.
