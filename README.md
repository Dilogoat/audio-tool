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

## Project layout

- `src/core/` - DSP engine and detectors
- `src/app/` - Desktop GUI
- `tests/unit/` - Catch2 unit tests
- `scripts/` - Synthetic fixture generator
- `ml/` - Future training/export pipeline
- `external/vcpkg/` - Dependency manager

## License note

FFTW3 is GPL. Suitable for private development; evaluate license implications before commercial distribution.
