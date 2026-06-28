#!/usr/bin/env python3
"""Batch-analyze a folder of real recordings and emit a QA summary."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

AUDIO_EXTS = {
    ".wav", ".wave", ".mp3", ".flac", ".ogg", ".opus", ".aiff", ".aif", ".w64", ".rf64", ".caf"
}


def find_audio_files(root: Path, recursive: bool) -> list[Path]:
    pattern = "**/*" if recursive else "*"
    files = [p for p in root.glob(pattern) if p.is_file() and p.suffix.lower() in AUDIO_EXTS]
    return sorted(files)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run audio_tool_cli over real files and summarize results.")
    parser.add_argument("input", type=Path, help="Folder containing recordings")
    parser.add_argument("--cli", type=Path, default=Path("build/audio_tool_cli.exe"))
    parser.add_argument("--out", type=Path, default=Path("qa-out"))
    parser.add_argument("--config", type=Path, default=Path("presets/podcast.json"))
    parser.add_argument("--recursive", action="store_true")
    parser.add_argument("--export-markers", default="all")
    args = parser.parse_args()

    if not args.input.is_dir():
        print(f"input folder not found: {args.input}", file=sys.stderr)
        return 2
    if not args.cli.exists():
        print(f"CLI not found: {args.cli}", file=sys.stderr)
        return 2

    args.out.mkdir(parents=True, exist_ok=True)
    files = find_audio_files(args.input, args.recursive)
    if not files:
        print("no audio files found", file=sys.stderr)
        return 1

    summary = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "input": str(args.input),
        "files": [],
    }

    for path in files:
        cmd = [
            str(args.cli),
            "analyze",
            str(path),
            "--out-dir",
            str(args.out),
            "--force",
            "--export-markers",
            args.export_markers,
        ]
        if args.config.exists():
            cmd.extend(["--config", str(args.config)])

        proc = subprocess.run(cmd, capture_output=True, text=True)
        report_path = args.out / f"{path.stem}.report.json"
        report = {}
        if report_path.exists():
            report = json.loads(report_path.read_text(encoding="utf-8"))

        summary["files"].append(
            {
                "input": str(path),
                "ok": proc.returncode == 0,
                "exit_code": proc.returncode,
                "stdout": proc.stdout.strip(),
                "stderr": proc.stderr.strip(),
                "report": report,
            }
        )

    summary_path = args.out / "qa-summary.json"
    summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    md_lines = [
        "# Audio Tool QA Summary",
        "",
        f"- Generated: {summary['generated_at']}",
        f"- Input: `{summary['input']}`",
        f"- Files analyzed: {len(summary['files'])}",
        "",
        "| File | Duration (s) | Pauses | Clicks | Breaths | Peak score | OK |",
        "| --- | ---: | ---: | ---: | ---: | ---: | --- |",
    ]
    for item in summary["files"]:
        report = item.get("report", {})
        counts = report.get("counts", {})
        md_lines.append(
            f"| `{Path(item['input']).name}` | {report.get('duration_sec', '')} | "
            f"{counts.get('pauses', '')} | {counts.get('clicks', '')} | {counts.get('breaths', '')} | "
            f"{report.get('peak_score', '')} | {'yes' if item['ok'] else 'no'} |"
        )

    (args.out / "qa-summary.md").write_text("\n".join(md_lines) + "\n", encoding="utf-8")
    print(f"Wrote {summary_path}")
    print(f"Wrote {args.out / 'qa-summary.md'}")
    return 0 if all(item["ok"] for item in summary["files"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
