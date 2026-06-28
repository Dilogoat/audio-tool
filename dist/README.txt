Audio Cleanup & Analysis Tool - Portable Build (Windows x64)
============================================================

WHAT'S INCLUDED
---------------
- audio_tool.exe      Desktop GUI (visual analysis + cleanup)
- audio_tool_cli.exe  Command-line tool for batch processing
- presets\            Example detector presets

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

  audio_tool_cli.exe analyze "C:\path\to\file.wav"
  audio_tool_cli.exe batch "C:\path\to\folder" --out-dir out --clean -r
  audio_tool_cli.exe help

The CLI never modifies your source files - results go to a separate
output folder (default: <input>\audio-tool-out).

USING IT
--------
1. Click "Open" and choose a WAV / FLAC / OGG / AIFF file.
2. Click "Analyze" to detect pauses, clicks, and breaths.
3. Pan/zoom any graph (drag / scroll) - all graphs stay time-synced.
4. Shift+Click a graph to move the playhead; press "Play" to listen.
5. Use the sidebar to tune detector sensitivity and review markers.
6. Export markers (JSON) or a cleaned WAV from the toolbar.

REQUIREMENTS
------------
- 64-bit Windows 10 or 11.
- No installation required.

This is a portable build. Source code and build instructions live in
the project repository.
