#!/usr/bin/env python3
"""Generate synthetic audio fixtures for unit/integration testing."""

from __future__ import annotations

import math
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests" / "fixtures"
SAMPLE_RATE = 44100


def write_wav(path: Path, samples: list[float]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    pcm = b"".join(struct.pack("<h", max(-32768, min(32767, int(s * 32767)))) for s in samples)
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(pcm)


def tone(seconds: float, freq: float, amp: float = 0.2) -> list[float]:
    n = int(seconds * SAMPLE_RATE)
    return [amp * math.sin(2 * math.pi * freq * i / SAMPLE_RATE) for i in range(n)]


def silence(seconds: float) -> list[float]:
    return [0.0] * int(seconds * SAMPLE_RATE)


def click_burst(samples: list[float], position: int, width: int = 20, amp: float = 0.9) -> None:
    for i in range(-width, width + 1):
        idx = position + i
        if 0 <= idx < len(samples):
            samples[idx] = amp


def breath_noise(seconds: float, amp: float = 0.03) -> list[float]:
    import random

    random.seed(42)
    n = int(seconds * SAMPLE_RATE)
    out = []
    for i in range(n):
        hf = math.sin(2 * math.pi * 7000 * i / SAMPLE_RATE)
        noise = (random.random() - 0.5) * 2.0
        out.append(amp * (0.6 * hf + 0.4 * noise))
    return out


def main() -> None:
    # Tone + 2s pause + tone
    speech_pause = tone(1.0, 220) + silence(2.0) + tone(1.0, 220)
    write_wav(FIXTURES / "speech_with_pause.wav", speech_pause)

    # Click in low-level bed
    click_track = [0.02] * SAMPLE_RATE
    click_burst(click_track, SAMPLE_RATE // 2)
    write_wav(FIXTURES / "click_burst.wav", click_track)

    # Breath-like segment between silences
    breath_track = silence(0.5) + breath_noise(0.8) + silence(0.5)
    write_wav(FIXTURES / "breath_segment.wav", breath_track)

    # Short stress file metadata only (avoid huge repo): 30s mixed
    mixed = tone(5, 180) + silence(1.9) + tone(3, 200)
    click_burst(mixed, len(mixed) // 2)
    mixed += breath_noise(1.0)
    write_wav(FIXTURES / "mixed_demo.wav", mixed)

    print(f"Wrote fixtures to {FIXTURES}")


if __name__ == "__main__":
    main()
