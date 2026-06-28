Audio Cleanup & Analysis Tool - Portable Build (Windows x64)
============================================================

WHAT'S INCLUDED
---------------
- audio_tool.exe      Desktop GUI (visual analysis + cleanup)
- audio_tool_cli.exe  Command-line tool for batch processing
- presets\            Example detector presets

SUPPORTED FORMATS (OPEN)
------------------------
WAV, MP3, FLAC, OGG, Opus, AIFF, CAF, W64

Exports are lossless WAV plus JSON / label sidecars.
Your original files are never modified.

HOW TO RUN (GUI)
----------------
1. Keep all files in this folder together (the .dll files must stay
   next to the .exe files).
2. Double-click audio_tool.exe.

If Windows SmartScreen warns about an unrecognized app, click
"More info" -> "Run anyway" (the app is unsigned).

HOW TO RUN (CLI / BULK)
-----------------------
Open a terminal in this folder, then e.g.:

  audio_tool_cli.exe analyze "C:\path\to\episode.mp3"
  audio_tool_cli.exe batch "C:\path\to\folder" --out-dir out --cut pauses -r
  audio_tool_cli.exe help

The CLI never modifies your source files - results go to a separate
output folder (default: <input>\audio-tool-out).

USING IT
--------
1. Click "Open" and choose WAV, MP3, FLAC, OGG, Opus, or AIFF.
2. Click "Analyze" to detect pauses, clicks, and breaths.
3. Pan/zoom any graph (drag / scroll) - all graphs stay time-synced.
4. Shift+Click a graph to move the playhead; press "Play" to listen.
5. Use the sidebar to tune detector sensitivity and review markers.
6. Export markers (all pro-editor formats), labels, cleaned WAV, or edited (cut) WAV.
7. Use Playback Source (Original / Cleaned / Edited) and double-click a marker to preview +/- 2s.

MORE INFO
---------
Project site: https://dilogoat.github.io/audio-tool/
Source + docs: see the GitHub repository README.

REQUIREMENTS
------------
- 64-bit Windows 10 or 11.
- No installation required.
