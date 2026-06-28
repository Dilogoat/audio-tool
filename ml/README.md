# ML Pipeline (Phase 2)

This directory is reserved for the CRNN training and ONNX export pipeline.

## Status

- **Blocked on labeled dataset.** Heuristic detectors in `src/core/detectors/` are the default until training data exists.
- `OnnxInference` in `src/core/ml/OnnxInference.cpp` is a scaffold ready for ONNX Runtime integration.

## Recommended workflow

1. Collect or label audio segments: `speech`, `breath`, `click`, `silence`.
2. Train a CRNN on log-mel spectrograms (PyTorch).
3. Export to ONNX: `torch.onnx.export(...)`.
4. Place model at `ml/export/crnn_events.onnx`.
5. Enable inference in app settings once ONNX Runtime is wired.

## Suggested architecture

- Input: `(batch, 1, time_frames, mel_bins)` spectrogram tensor
- Backbone: 2D CNN feature extractor
- Temporal: BiGRU or LSTM
- Output: per-frame softmax over 4 classes

## Training data sources

- Use `scripts/gen_fixtures.py` synthetic audio for smoke tests.
- Record real voice sessions and annotate with Audacity labels or a custom JSON schema matching `Markers`.
