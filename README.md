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
| **Remediation** | Attenuation of breaths/clicks (`--clean`) or physical cut + crossfade (`--cut`) |
| **Export** | JSON markers, pro-editor sidecars (CSV, EDL, FCP XML, Reaper, Audition, Audacity), cleaned/edited WAV |
| **Batch** | Headless CLI with presets, logging, JSON reports, recursive folders |
| **Safety** | Source files are never modified — all outputs go to a separate folder |

## Supported formats

**Import** (via libsndfile + vcpkg codecs):

`WAV` · `MP3` · `FLAC` · `OGG` · `Opus` · `AIFF` · `CAF` · `W64` / `RF64`

**Export** (analysis outputs): mono 16-bit PCM `WAV` (cleaned / edited) at the source sample rate, plus JSON and label sidecars. Multi-channel sources are mixed down to mono for analysis and for these audio outputs.

> MP3 and Opus decoding require the `mpeg` and `external-libs` libsndfile features — already enabled in `vcpkg.json`.

## Download (no build required)

Grab the latest portable Windows x64 zip from [**Releases**](https://github.com/Dilogoat/audio-tool/releases), or use the tracked build in [`dist/`](dist/). Keep all files together and run `audio_tool.exe` (GUI) or `audio_tool_cli.exe help` (CLI). See [`dist/README.txt`](dist/README.txt).

## Quick start (GUI)

1. Open a voice recording (`WAV`, `MP3`, `FLAC`, etc.)
2. Click **Analyze**
3. Review markers on the waveform and heatmap
4. Preview each marker with Original / Cleaned / Edited playback
5. Accept or reject markers
6. Save a `.audiotool.json` session or export DAW/NLE marker sidecars, cleaned WAV, or edited (cut) WAV
7. Reopen a saved session later with **Load Session** to restore the source audio, detector settings, markers, and your accept/reject decisions

Rejected markers are excluded from every export — marker sidecars, cleaned WAV, and edited (cut) WAV — in both the GUI and the CLI. Shift+Click any graph to seek; all plots stay time-synced.

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

# Export all pro-editor marker sidecars (CSV, EDL, FCP XML, Reaper, Audition, Audacity)
audio_tool_cli analyze "D:\recordings\episode.wav" --export-markers all

# Watch an ingest folder; writes sessions, reports, and pro-editor markers automatically
audio_tool_cli watch "D:\recordings\inbox" --out-dir "D:\recordings\prepared" --export-markers all

# Scan once and exit (useful for Task Scheduler)
audio_tool_cli watch "D:\recordings\inbox" --once --settle-ms 0

# QA pass over a folder of real recordings
python scripts\qa_batch.py "D:\recordings" --out qa-out

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
| `<name>.audiotool.json` | Always; session (markers + review decisions + config), reopen via the GUI **Load Session** button. Re-runs carry over prior accept/reject decisions to markers that still overlap (same type, at least 50% overlap) |
| `<name>.audiotool.<timestamp>.bak.json` | Written before a re-run overwrites a session whose reviewed markers no longer all match a detection, so no review work is lost |
| `<name>.cleaned.wav` | `--clean` |
| `<name>.edited.wav` | `--cut` |
| `<name>.labels.txt` | `--labels` |
| `<name>.markers.csv` | `--export-markers` (includes `csv`) |
| `<name>.markers.edl` | `--export-markers` (includes `edl`) |
| `<name>.markers.fcpxml` | `--export-markers` (includes `fcpxml`) |
| `<name>.reaper-markers.csv` | `--export-markers` (includes `reaper`) |
| `<name>.audition-markers.csv` | `--export-markers` (includes `audition`) |
| `batch-summary.json` | Batch mode |
| `watch-state.json` | Watch mode dedupe/resume state |

### Key CLI flags

`--out-dir` · `--config` · `--clean` · `--cut <pauses,clicks,breaths>` · `--crossfade-ms` · `--labels` · `--export-markers <all|csv,edl,...>` · `--once` · `--poll-ms` · `--settle-ms` · `-r/--recursive` · `--force` · `--dry-run` · `--no-pauses/--no-clicks/--no-breaths` · `--log-level` · `--log-file`

`--clean` attenuates breath and click regions in place (same length) by `clean.attenuation`, with `clean.ramp_ms` fades just outside each region. `--cut` removes flagged regions and crossfades joins (shorter file).

With `-r/--recursive`, outputs mirror the input subfolders under the output folder (`in/a/ep.wav` -> `out/a/ep.*`). Inputs that would still share a name in one output folder (`ep.wav` + `ep.mp3`) get the extension appended: `ep_wav.*`, `ep_mp3.*`. Scans never descend into the output folder, so rendered WAVs are not picked up as new inputs.

Marker timecodes default to 29.97 fps non-drop-frame (FCP XML declares timebase 30 + NTSC). The EDL includes DaVinci Resolve `|C: |M: |D:` marker lines; the Audition list is tab-separated as Audition expects.

`watch` is the service-mode foundation: run it in a terminal, Task Scheduler, or a service wrapper. It waits for files to settle before processing and writes a state file so unchanged recordings are not processed twice.

**Exit codes:** `0` ok · `1` partial failure · `2` bad args · `3` fatal

## Build from source

**Requirements:** Windows 10/11, Visual Studio 2026 Build Tools (C++, MSVC 14.50+), CMake 3.24+, Ninja, Python 3 (fixtures)

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

**First-time setup:** Repo → Settings → Pages → Build and deployment → Source: **GitHub Actions**. The site auto-deploys on pushes to `main` that touch `docs/`.

**Branch safety:** Only you have admin access on the repo. To harden `main` further, enable branch protection in GitHub Settings → Branches (require the `build-test` CI check, block force pushes). A starter rules file lives in [`.github/branch-protection.json`](.github/branch-protection.json).

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
| `scripts/` | Synthetic fixtures, QA batch runner, release packaging |

## License note

FFTW3 is GPL. libsndfile is LGPL. MP3 encode/decode pulls in LAME (LGPL). Suitable for private development; evaluate license implications before commercial distribution.
