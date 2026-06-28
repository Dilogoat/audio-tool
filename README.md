# Audio Cleanup & Analysis Tool

[![Release](https://img.shields.io/github/v/release/Dilogoat/audio-tool?style=flat-square)](https://github.com/Dilogoat/audio-tool/releases)
[![CI](https://img.shields.io/github/actions/workflow/status/Dilogoat/audio-tool/ci.yml?branch=main&style=flat-square)](https://github.com/Dilogoat/audio-tool/actions)
[![Site source](https://img.shields.io/badge/site-source%20in%20docs%2F-6ee7ff?style=flat-square)](docs/)

Native C++ desktop application and CLI for detecting non-speech artifacts in long-form voice recordings — breaths, mouth clicks, and long pauses — with waveform visualization, batch processing, and surgical editing.

**Project site source:** [`docs/`](docs/) contains the polished landing page. GitHub Pages can publish it once the repository is public or the account plan supports Pages for private repositories.

## Features

| Area | What you get |
|------|----------------|
| **Detection** | Pause (RMS), click/plosive (transient), breath (spectral HF) |
| **Visualization** | Waveform, RMS envelope, composite heatmap, spectrogram (ImGui + ImPlot) |
| **Remediation** | Spectral attenuation (`--clean`) or physical cut + crossfade (`--cut`) |
| **Export** | JSON markers, Audacity labels, cleaned/edited WAV |
| **Batch** | Headless CLI with presets, logging, JSON reports, recursive folders |
| **Safety** | Source files are never modified — all outputs go to a separate folder |

## Supported formats

**Import** (via libsndfile + vcpkg codecs):

`WAV` · `MP3` · `FLAC` · `OGG` · `Opus` · `AIFF` · `CAF` · `W64` / `RF64`

**Export** (analysis outputs): lossless `WAV` (cleaned / edited), plus JSON and label sidecars.

> MP3 and Opus decoding require the `mpeg` and `external-libs` libsndfile features — already enabled in `vcpkg.json`.

## Download (no build required)

Grab the latest portable Windows x64 zip from [**Releases**](https://github.com/Dilogoat/audio-tool/releases), or use the tracked build in [`dist/`](dist/). Keep all files together and run `audio_tool.exe` (GUI) or `audio_tool_cli.exe help` (CLI). See [`dist/README.txt`](dist/README.txt).

## Quick start (GUI)

1. Open a voice recording (`WAV`, `MP3`, `FLAC`, etc.)
2. Click **Analyze**
3. Review markers on the waveform and heatmap
4. Export markers, labels, cleaned WAV, or edited (cut) WAV

Shift+Click any graph to seek; all plots stay time-synced.

## Command-line interface

`audio_tool_cli` shares the exact same DSP pipeline as the GUI.

```powershell
# Analyze a single file (outputs to <input_dir>/audio-tool-out)
audio_tool_cli analyze "D:\recordings\episode.mp3"

# Bulk-process a folder, also writing cleaned WAVs
audio_tool_cli batch "D:\recordings" --out-dir "D:\out" --clean --recursive

# Use a saved preset
audio_tool_cli batch "D:\recordings" --config presets\podcast.json

# Physically cut pauses + breaths and re-stitch
audio_tool_cli batch "D:\recordings" --out-dir "D:\out" --cut pauses,breaths

# Export Audacity labels for manual editing in a DAW
audio_tool_cli analyze "D:\recordings\episode.flac" --labels

# Write a default config you can edit
audio_tool_cli init-config mypreset.json

audio_tool_cli help
```

### Per-file outputs

For input `<name>.<ext>` (written to the output folder, never over the source):

| File | When |
|------|------|
| `<name>.markers.json` | Always |
| `<name>.report.json` | Always |
| `<name>.cleaned.wav` | `--clean` |
| `<name>.edited.wav` | `--cut` |
| `<name>.labels.txt` | `--labels` |
| `batch-summary.json` | Batch mode |

### Key CLI flags

`--out-dir` · `--config` · `--clean` · `--cut <pauses,clicks,breaths>` · `--crossfade-ms` · `--labels` · `-r/--recursive` · `--force` · `--dry-run` · `--no-pauses/--no-clicks/--no-breaths` · `--log-level` · `--log-file`

`--clean` attenuates artifacts in place (same length). `--cut` removes flagged regions and crossfades joins (shorter file).

**Exit codes:** `0` ok · `1` partial failure · `2` bad args · `3` fatal

## Build from source

**Requirements:** Windows 10/11, Visual Studio 2022 Build Tools (C++), CMake 3.24+, Ninja, Python 3 (fixtures)

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

Run: `.\build\audio_tool.exe` · `.\build\audio_tool_cli.exe help`

## GitHub Pages

The project site lives in [`docs/`](docs/) and is ready to deploy via [`.github/workflows/pages.yml`](.github/workflows/pages.yml).

**First-time setup:** Repo → Settings → Pages → Build and deployment → Source: **GitHub Actions**, then run the **Deploy GitHub Pages** workflow manually. GitHub Pages must be supported for the repository visibility (public repo, or a plan that supports Pages for private repos).

## Project layout

| Path | Purpose |
|------|---------|
| `src/core/` | DSP engine, detectors, pipeline, editor |
| `src/app/` | Desktop GUI |
| `src/cli/` | Headless batch CLI |
| `docs/` | GitHub Pages site |
| `tests/unit/` | Catch2 unit tests |
| `presets/` | Example JSON detector presets |
| `dist/` | Portable Windows build |
| `ml/` | Future ML training / ONNX export |
| `scripts/` | Synthetic fixture generator |

## License note

FFTW3 is GPL. libsndfile is LGPL. MP3 encode/decode pulls in LAME (LGPL). Suitable for private development; evaluate license implications before commercial distribution.
