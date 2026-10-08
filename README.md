# multimedia-lab

Image / audio / video metadata extraction plus two Python tasks:
voice conversion and a photo-fix + multilingual OCR pipe.

| # | Task | Tool | Input → Output |
|---|------|------|----------------|
| 1–3 | Image, audio, video metadata extraction | `media_analyzer` (C) | media file → console report (dimensions, codecs, chunks/atoms …) |
| 4 | Voice transformer — hear your voice in another AI voice | `voice_transformer.py` (Python) | mic / audio file → converted audio + playback |
| 5 | Task pipe — fix blurry/dark/rotated photos, read text in 12 languages | `task_pipe.py` (Python) | image / folder → fixed image + `.txt` |

## Setup

Prerequisites: `gcc`/`make` (task 1–3), `uv`, `ffmpeg` + `ffplay` (audio record fallback / playback).

```bash
uv sync            # one-time: creates the shared .venv (Python 3.12, torch CPU, EasyOCR, …)
make               # builds ./media_analyzer
cp .env.example .env   # only needed for the real AI voice (task 4)
```

Put your ElevenLabs key in `.env` (`ELEVENLABS_API_KEY=…`, free tier works).
Never commit `.env` — it's git-ignored.

## Tasks 1–3 — media metadata (C)

```bash
make run FILE=media/jpg/wallhaven-xe73ro.jpg
make run FILE=media/mp3/*.mp3
make run FILE=media/mp4/animated.mp4
./media_analyzer <media_file>   # BMP, PNG, JPEG, WAV, MP3, AVI, MP4 (magic-byte dispatch)
```

Parsers live in `main.c` (`parse_bmp`, `parse_png`, `parse_jpeg`, `parse_wav`,
`parse_mp3`, `parse_mp4`, `parse_avi`). Samples under `media/`.

## Task 4 — voice transformer

```bash
uv run voice_transformer.py --record 5 --voice Rachel --mock --play   # offline demo, no key
uv run voice_transformer.py --record 5 --voice Rachel                 # real AI voice (needs key)
uv run voice_transformer.py --input myvoice.wav --voice Clyde --output voice_out.mp3
uv run voice_transformer.py --list-voices                             # stock IDs work without a key
```

- **Modes:** real = ElevenLabs speech-to-speech (`POST /v1/speech-to-speech/{voice_id}`,
  keeps your prosody); `--mock` = local pitch-shift so the demo never blocks on key/quota.
- **Voices:** name (`Rachel`, `Clyde`, `Domi`, `Bella`, `Antoni`, `Elli`, `Josh`,
  `Arnold`, `Adam`, `Sam`) or a raw `voice_id`. `--list-voices` with a key shows your full account list.
- **Audio:** mic via `sounddevice`, auto-fallback to `ffmpeg` (pulse/alsa); any input
  format is normalized to wav before conversion; output is mp3 if `ffmpeg` exists, else wav.
- Key resolution order: `--api-key` flag → `$ELEVENLABS_API_KEY` → `.env` → mock with a notice.

## Task 5 — task pipe (photo fix + OCR)

```bash
uv run task_pipe.py --input photo.jpg
uv run task_pipe.py --input photos/ --output-dir task_pipe_out/
uv run task_pipe.py --input photo.jpg --no-fix        # ablation: OCR without fixes
uv run task_pipe.py --input photo.jpg --langs en,hi,ar,ch_sim
```

Pipe per image (metrics printed for the lab report):

1. EXIF auto-rotate (phone photos)
2. Brighten if dark (mean-V < 125 → CLAHE + gamma lift toward ~130)
3. Denoise + sharpen if blurry (Laplacian-var < 300 → bilateral + unsharp mask)
4. Deskew (Hough median line angle, contour fallback → warp)
5. Upscale small images (h < 800 px) for OCR
6. EasyOCR — default 12 languages: `en, hi, ar, ch_sim, ch_tra, es, fr, de, ru, ja, ko, pt`

Outputs per input: `task_pipe_out/fixed_<name>.jpg` + `task_pipe_out/<name>.txt`.

Notes:

- First OCR run downloads detection + recognition models (~100 MB+, cached afterwards);
  full 12-language first run downloads more — start with `--langs en` to smoke-test.
- Pure pip (no system `tesseract`); `opencv-python-headless` is used so it works
  without X11/system libs. `--show` needs a display; without one it just prints the output path.
- `torch` is CPU-only here; pass `--gpu` only if CUDA works on your machine.

## references

- https://sxvyte.medium.com/image-processing-in-c-reading-writing-and-rotating-bmp-images-from-scratch-6eaa74c716cb
- https://www.libpng.org/pub/png/spec/1.2/PNG-Structure.html
- https://en.wikipedia.org/wiki/PNG
- https://docs.elevenlabs.io/api-reference/speech-to-speech
- https://github.com/JaidedAI/EasyOCR
