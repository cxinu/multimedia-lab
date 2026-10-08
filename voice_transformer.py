#!/usr/bin/env python3
"""Voice transformer — record your voice, hear it in another AI voice.

Usage (shared venv, see README):
    uv sync                                  # one-time setup
    uv run voice_transformer.py --record 5 --voice Rachel --mock --play
    uv run voice_transformer.py --input myvoice.wav --voice Clyde --output out.mp3
    uv run voice_transformer.py --list-voices
    uv run voice_transformer.py --record 5 --voice Rachel   # real ElevenLabs STS (needs key)

Free-tier note: stock voices (Rachel, Clyde, …) are blocked via API on free
accounts. Clone a voice once (allowed on free tier), then use its voice_id:
    uv run voice_transformer.py --clone-voice myfriend --clone-samples friend1.mp3
    uv run voice_transformer.py --input media/audio_sophisticated.ogg --voice <voice_id>

Input:  mic (--record SECS) or existing file (--input FILE).
Output: converted audio (--output FILE) + optional playback (--play, default on).

Two modes:
  1. REAL (ElevenLabs speech-to-speech, keeps prosody):
       needs ELEVENLABS_API_KEY env var, --api-key flag, or .env file.
       POST https://api.elevenlabs.io/v1/speech-to-speech/{voice_id}
  2. MOCK (offline fallback, no key / no credits):
       --mock pitch-shifts locally with numpy/scipy so the lab demo never blocks.

Playback uses ffplay (already on your system). Recording tries sounddevice,
then falls back to ffmpeg (pulse/alsa) so no system packages are required.
"""

from __future__ import annotations

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import urllib.request
import wave
from pathlib import Path

# --------------------------------------------------------------------------
# Known ElevenLabs stock voice IDs (stable public IDs, free tier can use them).
# User can also pass a raw voice_id directly.
# --------------------------------------------------------------------------
VOICES = {
    "rachel": "21m00Tcm4TlvDq8ikWAM",   # warm female narration
    "clyde": "2EiwWnXFnvU5JabPnv8n",     # middle-aged male
    "domi": "AZnzlk1XvdvUeBnXmlld",      # strong female
    "bella": "EXAVITQu4vr4xnSDQMaL",     # soft female
    "antoni": "ErXwobaYiN019PkySvjV",    # well-rounded male
    "elli": "MF3mGyEYCl7XYWbV9V6O",      # emotional female
    "josh": "TxGEqnHWrfWFTfGW9XjX",      # deep young male
    "arnold": "VR6AewLTigWG4xSOl13",     # crisp middle-aged male
    "adam": "pNInz6obpgDQGcFmaJgB",      # deep narration male
    "sam": "yoZ06aMxZJJt9JmdXNzT",       # raspy young male
}

API_BASE = "https://api.elevenlabs.io/v1"


def load_dotenv(path: Path = Path(".env")) -> None:
    """Tiny .env loader (no dependency). Does not override existing env."""
    if not path.exists():
        return
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        k, v = k.strip(), v.strip().strip("'").strip('"')
        os.environ.setdefault(k, v)


def resolve_voice(name_or_id: str) -> str:
    if name_or_id in VOICES:
        return VOICES[name_or_id]
    low = name_or_id.lower()
    if low in VOICES:
        return VOICES[low]
    return name_or_id  # assume raw voice_id


def have(cmd: str) -> bool:
    return shutil.which(cmd) is not None


# --------------------------------------------------------------------------
# Record
# --------------------------------------------------------------------------
def record_sounddevice(path: Path, seconds: float, sr: int = 16000) -> bool:
    try:
        import sounddevice as sd  # optional; needs PortAudio system lib
        import numpy as np
    except (ImportError, OSError) as e:
        print(f"[record] sounddevice unavailable ({e.__class__.__name__}), "
              "trying next method ...")
        return False
    import numpy as np

    print(f"[record] mic via sounddevice: {seconds:.0f}s @ {sr}Hz ... speak now")
    try:
        audio = sd.rec(int(seconds * sr), samplerate=sr, channels=1, dtype="int16")
        sd.wait()
    except Exception as e:
        print(f"[record] sounddevice capture failed ({e}), trying next method ...")
        return False
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(audio.tobytes())
    print(f"[record] saved {path}")
    return True


def record_pw_record(path: Path, seconds: float, sr: int = 16000) -> bool:
    """Native PipeWire recorder (no PortAudio/ALSA tools needed)."""
    if not have("pw-record") or not have("timeout"):
        return False
    cmd = ["timeout", str(seconds),
           "pw-record", "--rate", str(sr), "--channels", "1", str(path)]
    print(f"[record] mic via pw-record: {seconds:.0f}s @ {sr}Hz ... speak now")
    r = subprocess.run(cmd, capture_output=True, text=True)
    # `timeout` kills pw-record after N secs -> exit code 124 = success here.
    if r.returncode in (0, 124) and path.exists() and path.stat().st_size > 44:
        print(f"[record] saved {path}")
        return True
    print("[record] pw-record capture failed (no mic source?). "
          "Trying next method ...", file=sys.stderr)
    try:
        path.unlink(missing_ok=True)
    except OSError:
        pass
    return False


def record_ffmpeg(path: Path, seconds: float, sr: int = 16000) -> bool:
    if not have("ffmpeg"):
        return False
    # Try pipewire/pulse first, then alsa default.
    sources = [
        ["ffmpeg", "-y", "-f", "pulse", "-i", "default",
         "-t", str(seconds), "-ar", str(sr), "-ac", "1", str(path)],
        ["ffmpeg", "-y", "-f", "alsa", "-i", "default",
         "-t", str(seconds), "-ar", str(sr), "-ac", "1", str(path)],
    ]
    for cmd in sources:
        print(f"[record] mic via {' '.join(cmd[2:5])} ... speak now")
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode == 0 and path.exists():
            print(f"[record] saved {path}")
            return True
    print("[record] ffmpeg capture failed (no mic?). "
          "Tip: use --input existing.wav instead.", file=sys.stderr)
    return False


def do_record(out: Path, seconds: float, sr: int) -> Path:
    if record_sounddevice(out, seconds, sr):
        return out
    if record_pw_record(out, seconds, sr):
        return out
    if record_ffmpeg(out, seconds, sr):
        return out
    raise SystemExit(
        "Could not record from mic (tried sounddevice, pw-record, ffmpeg).\n"
        "Options:\n"
        "  - record on your phone/browser and pass it in:\n"
        "      uv run voice_transformer.py --input recording.m4a --mock\n"
        "  - check a mic is plugged in and visible to PipeWire (pw-record --list-targets)"
    )


# --------------------------------------------------------------------------
# Mock transform (offline pitch shift, no API key)
# --------------------------------------------------------------------------
def read_wav_mono(path: Path):
    with wave.open(str(path), "rb") as w:
        n, sw, fr, ch = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(ch)
    if sw == 1:
        import numpy as np
        x = np.frombuffer(raw, dtype=np.uint8).astype(np.float32) - 128.0
        x = x.reshape(-1, n).mean(axis=1) if n > 1 else x
    elif sw == 2:
        import numpy as np
        x = np.frombuffer(raw, dtype=np.int16).astype(np.float32)
        x = x.reshape(-1, n).mean(axis=1) if n > 1 else x
    else:
        raise SystemExit(f"Unsupported sample width {sw*8}-bit in {path}")
    return x, fr


def write_wav_mono(path: Path, x, sr: int) -> None:
    import numpy as np

    x = np.asarray(x, dtype=np.float64)
    peak = float((abs(x)).max()) or 1.0
    x = (x / peak * 30000.0).astype(np.int16)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(x.tobytes())


def pitch_shift(x, sr: int, semitones: float):
    """Crude but dependency-light pitch shift: resample up, then stretch back."""
    import numpy as np

    factor = 2.0 ** (semitones / 12.0)
    n = len(x)
    # 1) resample to shifted pitch (changes length)
    idx = (np.arange(int(n / factor)) * factor)
    idx = idx[idx < n - 1]
    lo = idx.astype(int)
    frac = idx - lo
    y = x[lo] * (1 - frac) + x[lo + 1] * frac
    # 2) stretch back to original length (keeps duration, keeps pitch shift)
    if len(y) == 0:
        return x
    pos = np.linspace(0, len(y) - 1, n)
    lo2 = pos.astype(int).clip(0, len(y) - 2)
    frac2 = pos - lo2
    out = y[lo2] * (1 - frac2) + y[lo2 + 1] * frac2
    return out


def mock_transform(src: Path, dst_wav: Path, semitones: float = 5.0) -> Path:
    x, sr = read_wav_mono(src)
    print(f"[mock] offline voice shift ({semitones:+.0f} semitones, no API key) ...")
    y = pitch_shift(x, sr, semitones)
    write_wav_mono(dst_wav, y, sr)
    return dst_wav


# --------------------------------------------------------------------------
# ElevenLabs speech-to-speech (stdlib only — no SDK needed)
# --------------------------------------------------------------------------
def _multipart_body(fields: dict, files: list[tuple[str, Path, str]],
                    boundary: str) -> bytes:
    """Build multipart/form-data body. fields: name->text, files: (field, path, mime)."""
    body = b""
    for name, value in fields.items():
        body += (f"--{boundary}\r\n"
                 f'Content-Disposition: form-data; name="{name}"\r\n\r\n'
                 f"{value}\r\n").encode()
    for field, path, mime in files:
        with open(path, "rb") as f:
            data = f.read()
        body += (f"--{boundary}\r\n"
                 f'Content-Disposition: form-data; name="{field}"; '
                 f'filename="{path.name}"\r\n'
                 f"Content-Type: {mime}\r\n\r\n").encode()
        body += data + b"\r\n"
    body += f"--{boundary}--\r\n".encode()
    return body


def sts_convert(src: Path, voice_id: str, api_key: str,
                model_id: str = "eleven_english_sts_v2") -> bytes:
    boundary = "----voicetransformer boundary"
    with open(src, "rb") as f:
        audio = f.read()
    fname = Path(src).name or "audio.wav"
    body = (
        f"--{boundary}\r\n".encode()
        + f'Content-Disposition: form-data; name="model_id"\r\n\r\n{model_id}\r\n'.encode()
        + f"--{boundary}\r\n".encode()
        + f'Content-Disposition: form-data; name="audio"; filename="{fname}"\r\n'
          f"Content-Type: audio/wav\r\n\r\n".encode()
        + audio + f"\r\n--{boundary}--\r\n".encode()
    )
    req = urllib.request.Request(
        f"{API_BASE}/speech-to-speech/{voice_id}",
        data=body,
        headers={"xi-api-key": api_key,
                 "Content-Type": f"multipart/form-data; boundary={boundary}"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=120) as r:
            return r.read()
    except Exception as e:
        # Try to surface the API's JSON error body.
        detail = ""
        try:
            import json
            detail = e.read().decode()  # type: ignore[attr-defined]
            try:
                detail = json.loads(detail).get("detail", {}).get("message", detail)
            except Exception:
                pass
        except Exception:
            detail = str(e)
        if "library voices" in detail.lower() or "not available for free" in detail.lower():
            raise SystemExit(
                f"ElevenLabs STS failed: {detail}\n\n"
                "Free-tier keys cannot use stock/library voices (Rachel, Clyde, …) via API.\n"
                "Workaround — clone a voice (allowed on free tier, usable via API):\n"
                "  1. get 1-3 min of clear single-speaker speech (with their permission)\n"
                "  2. uv run voice_transformer.py --clone-voice myfriend "
                "--clone-samples friend1.mp3 [friend2.mp3 ...]\n"
                "  3. use the printed voice_id with --voice, e.g.:\n"
                "       uv run voice_transformer.py --input media/audio_sophisticated.ogg "
                "--voice <voice_id> --output voice_out.mp3\n"
                "Or demo offline now: add --mock."
            )
        raise SystemExit(f"ElevenLabs STS failed: {detail or e}\n"
                         "Tip: check key/quota, or retry with --mock.")


def clone_voice(name: str, samples: list[Path], api_key: str,
                description: str = "") -> str:
    """Create an Instant Voice Clone from 1+ sample files. Returns voice_id.

    Needs ~1-3 min total of clear single-speaker speech (wav/mp3/m4a/ogg —
    converted to 16kHz mono wav first). Works on the free tier; the returned
    voice_id IS usable via API (unlike stock library voices).
    """
    import json

    conv: list[Path] = []
    tmpdir = Path(tempfile.mkdtemp(prefix="voice_clone_"))
    for s in samples:
        if not s.exists():
            raise SystemExit(f"Sample not found: {s}")
        dst = tmpdir / (s.stem + ".wav")
        if s.suffix.lower() == ".wav":
            shutil.copy(s, dst)
        elif have("ffmpeg"):
            r = subprocess.run(["ffmpeg", "-y", "-i", str(s),
                                "-ar", "16000", "-ac", "1", str(dst)],
                               capture_output=True, text=True)
            if r.returncode != 0:
                raise SystemExit(f"ffmpeg could not decode sample {s}")
        else:
            raise SystemExit(f"{s} is not .wav and ffmpeg is missing.")
        conv.append(dst)

    boundary = "----voicetransformer clone"
    body = _multipart_body({"name": name, "description": description},
                           [("files", p, "audio/wav") for p in conv], boundary)
    req = urllib.request.Request(
        f"{API_BASE}/voices/add",
        data=body,
        headers={"xi-api-key": api_key,
                 "Content-Type": f"multipart/form-data; boundary={boundary}"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=180) as r:
            data = json.loads(r.read().decode())
    except Exception as e:
        try:
            err = e.read().decode()  # type: ignore[attr-defined]
        except Exception:
            err = str(e)
        raise SystemExit(f"Voice cloning failed: {err}")
    vid = data.get("voice_id", "")
    if not vid:
        raise SystemExit(f"Voice cloning failed: unexpected response {data}")
    return vid


def save_cloned_voice_env(voice_id: str, path: Path = Path(".env")) -> None:
    """Persist CLONED_VOICE_ID so later runs can default to it."""
    lines = path.read_text(encoding="utf-8").splitlines() if path.exists() else []
    lines = [l for l in lines if not l.strip().startswith("CLONED_VOICE_ID=")]
    lines.append(f"CLONED_VOICE_ID={voice_id}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[clone] saved CLONED_VOICE_ID to {path}")


def list_voices(api_key: str) -> None:
    req = urllib.request.Request(f"{API_BASE}/voices",
                                 headers={"xi-api-key": api_key})
    with urllib.request.urlopen(req, timeout=30) as r:
        import json
        data = json.loads(r.read().decode())
    print("Available voices (name -> voice_id):")
    for v in data.get("voices", []):
        print(f"  {v['name']} -> {v['voice_id']}")


# --------------------------------------------------------------------------
# Output helpers
# --------------------------------------------------------------------------
def finalize_output(tmp_wav: Path, final: Path) -> Path:
    """Convert wav -> final extension (mp3 needs ffmpeg, else keep wav)."""
    final = Path(final)
    if final.suffix.lower() == ".wav" or not have("ffmpeg"):
        if final.suffix.lower() != ".wav" and not have("ffmpeg"):
            final = final.with_suffix(".wav")
            print("[warn] ffmpeg missing — saving .wav instead of "
                  f"{final.suffix}", file=sys.stderr)
        shutil.copy(tmp_wav, final)
        return final
    r = subprocess.run(["ffmpeg", "-y", "-i", str(tmp_wav), str(final)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        shutil.copy(tmp_wav, final.with_suffix(".wav"))
        return final.with_suffix(".wav")
    return final


def play(path: Path) -> None:
    if have("ffplay"):
        subprocess.run(["ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet",
                        str(path)])
        return
    try:
        import sounddevice as sd  # type: ignore
        import wave as wv
        import numpy as np
        with wv.open(str(path), "rb") as w:
            sr, n, sw = w.getframerate(), w.getnframes(), w.getsampwidth()
            raw = w.readframes(n)
        dtype = np.uint8 if sw == 1 else np.int16
        sd.play(np.frombuffer(raw, dtype=dtype), sr)
        sd.wait()
    except Exception:
        print(f"[play] no player found (install ffplay). File saved: {path}")


# --------------------------------------------------------------------------
def main() -> None:
    load_dotenv()
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--record", type=float, default=0, metavar="SECS",
                    help="record SECS seconds from mic")
    ap.add_argument("--input", type=Path, default=None, help="existing audio file")
    ap.add_argument("--output", type=Path, default=Path("voice_out.wav"))
    ap.add_argument("--voice", default=None,
                    help="voice name (Rachel/Clyde/...), raw voice_id, "
                         "or $CLONED_VOICE_ID if set (default: cloned id, else Rachel)")
    ap.add_argument("--clone-voice", default=None, metavar="NAME",
                    help="create an Instant Voice Clone called NAME from "
                         "--clone-samples, print + save its voice_id")
    ap.add_argument("--clone-samples", nargs="+", type=Path, default=[],
                    metavar="SAMPLE",
                    help="1+ sample files for --clone-voice (~1-3 min clear speech total)")
    ap.add_argument("--clone-description", default="",
                    help="optional description stored with the cloned voice")
    ap.add_argument("--api-key", default=os.getenv("ELEVENLABS_API_KEY", ""),
                    help="ElevenLabs key (or $ELEVENLABS_API_KEY / .env)")
    ap.add_argument("--model", default="eleven_english_sts_v2")
    ap.add_argument("--mock", action="store_true",
                    help="offline pitch-shift demo, no key needed")
    ap.add_argument("--mock-pitch", type=float, default=5.0, metavar="SEMI",
                    help="mock pitch shift in semitones (default 5)")
    ap.add_argument("--list-voices", action="store_true")
    ap.add_argument("--play", dest="play", action="store_true", default=True)
    ap.add_argument("--no-play", dest="play", action="store_false")
    ap.add_argument("--sr", type=int, default=16000)
    args = ap.parse_args()

    if args.clone_voice:
        if not args.clone_samples:
            print("--clone-voice needs --clone-samples FILE [FILE ...] "
                  "(~1-3 min clear single-speaker speech, with their permission).",
                  file=sys.stderr)
            sys.exit(2)
        if not args.api_key:
            sys.exit("Cloning needs an API key: set ELEVENLABS_API_KEY in .env.")
        print(f"[clone] creating Instant Voice Clone '{args.clone_voice}' "
              f"from {len(args.clone_samples)} sample(s) ...")
        vid = clone_voice(args.clone_voice, args.clone_samples,
                          args.api_key, args.clone_description)
        print(f"[clone] voice_id: {vid}")
        save_cloned_voice_env(vid)
        print("Use it: uv run voice_transformer.py --input <audio> "
              f"--voice {vid} --output voice_out.mp3")
        return

    # Default voice: explicit --voice > saved clone > Rachel (stock needs paid key).
    voice = args.voice or os.getenv("CLONED_VOICE_ID", "") or "Rachel"

    if args.list_voices:
        if not args.api_key:
            print("Known stock voices (no key needed to list these):")
            for k, v in VOICES.items():
                print(f"  {k} -> {v}")
            print("\nFor your full account list: pass --api-key KEY --list-voices")
            return
        list_voices(args.api_key)
        return

    # ---- input ----
    if args.input and args.record:
        print("Pass either --input or --record, not both.", file=sys.stderr)
        sys.exit(2)
    tmpdir = Path(tempfile.mkdtemp(prefix="voice_tr_"))
    if args.input:
        src = args.input
        if not src.exists():
            sys.exit(f"Input not found: {src}")
    elif args.record and args.record > 0:
        src = tmpdir / "mic.wav"
        do_record(src, args.record, args.sr)
    else:
        # Default demo: 5s mic recording.
        print("No --input/--record given → recording 5s from mic "
              "(Ctrl+C to abort, or pass --input FILE).")
        src = tmpdir / "mic.wav"
        do_record(src, 5.0, args.sr)

    # Normalize any format to wav for the API / mock path.
    src_wav = tmpdir / "src.wav"
    if src.suffix.lower() == ".wav":
        shutil.copy(src, src_wav)
    elif have("ffmpeg"):
        r = subprocess.run(["ffmpeg", "-y", "-i", str(src),
                            "-ar", "16000", "-ac", "1", str(src_wav)],
                           capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit(f"ffmpeg could not decode {src}")
    else:
        sys.exit(f"{src} is not .wav and ffmpeg is missing to convert it.")

    # ---- convert ----
    out_wav = tmpdir / "converted.wav"
    if args.mock or not args.api_key:
        if args.api_key == "" and not args.mock:
            print("[info] no API key → using --mock offline demo. "
                  "Set ELEVENLABS_API_KEY for the real AI voice.")
        mock_transform(src_wav, out_wav, args.mock_pitch)
    else:
        vid = resolve_voice(voice)
        print(f"[sts] converting via ElevenLabs voice '{voice}' ({vid}) ...")
        audio_bytes = sts_convert(src_wav, vid, args.api_key, args.model)
        out_wav.write_bytes(audio_bytes)

    final = finalize_output(out_wav, args.output)
    print(f"[done] saved {final} ({final.stat().st_size} bytes)")
    if args.play:
        print("[play] playing back ...")
        play(final)


if __name__ == "__main__":
    main()
