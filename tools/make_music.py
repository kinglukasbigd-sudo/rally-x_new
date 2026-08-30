#!/usr/bin/env python3
"""Build the looping background track from a source audio file.

Decodes with ffmpeg, takes a slice from the end, makes it loop seamlessly by
crossfading the tail back over the head, and writes 22050 Hz mono 16-bit PCM --
the exact format the game's audio device already runs at, so nothing has to be
resampled at run time.

    tools/make_music.py <input> <output.wav> [seconds]

`seconds` is how much to take from the end of the source; pass 0 (or "all")
to use the whole thing.
"""
import os, subprocess, sys, tempfile, wave
import numpy as np

RATE       = 22050          # matches AudioManager's device
CROSSFADE  = 0.35           # seconds folded back over the start of the loop
PEAK       = 0.89           # normalise headroom; runtime gain does the rest
FFMPEG     = os.environ.get("FFMPEG", "ffmpeg")


def decode(path):
    with tempfile.TemporaryDirectory() as tmp:
        wav = os.path.join(tmp, "decoded.wav")
        subprocess.run([FFMPEG, "-v", "error", "-y", "-i", path,
                        "-ac", "1", "-ar", str(RATE), "-f", "wav", wav],
                       check=True)
        with wave.open(wav, "rb") as w:
            frames = w.readframes(w.getnframes())
    return np.frombuffer(frames, dtype="<i2").astype(np.float32) / 32768.0


def envelope(x, buckets=12):
    """Peak level per slice -- used to spot a fade-out at the end."""
    step = max(1, len(x) // buckets)
    return [float(np.abs(x[i:i + step]).max()) for i in range(0, len(x), step)][:buckets]


def trim_fade(x, floor=0.12):
    """Drop a trailing fade-out, so the loop does not dip every time round."""
    win = int(0.05 * RATE)
    level = np.array([np.abs(x[i:i + win]).max() for i in range(0, len(x) - win, win)])
    if len(level) == 0:
        return x
    loud = np.flatnonzero(level > level.max() * floor)
    if len(loud) == 0:
        return x
    return x[: (loud[-1] + 1) * win]


def make_loop(x, seconds):
    if seconds and seconds > 0 and len(x) > seconds * RATE:
        tail = x[-int(seconds * RATE):]
    else:
        tail = x
    tail = trim_fade(tail)

    n = int(CROSSFADE * RATE)
    if len(tail) > 2 * n:
        # Fold the final `n` samples back over the first `n`, so the join at
        # the loop point is continuous instead of a click.
        fade = np.linspace(0.0, 1.0, n, dtype=np.float32)
        head, end = tail[:n].copy(), tail[-n:]
        tail = tail[:-n]
        tail[:n] = head * fade + end * (1.0 - fade)

    peak = float(np.abs(tail).max())
    if peak > 0:
        tail = tail * (PEAK / peak)
    return tail


def write_wav(path, x):
    pcm = np.clip(x, -1.0, 1.0)
    pcm = (pcm * 32767.0).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    src, dst = sys.argv[1], sys.argv[2]
    arg = sys.argv[3] if len(sys.argv) > 3 else "15"
    seconds = 0.0 if arg.lower() in ("all", "full", "0") else float(arg)

    audio = decode(src)
    print("source     : %.2fs" % (len(audio) / RATE))
    print("taking     : %s" % ("whole track" if seconds <= 0 else "last %.0fs" % seconds))
    print("envelope   : %s" % " ".join("%.2f" % v for v in envelope(audio)))

    loop = make_loop(audio, seconds)
    os.makedirs(os.path.dirname(dst) or ".", exist_ok=True)
    write_wav(dst, loop)

    print("loop       : %.2fs, %d frames" % (len(loop) / RATE, len(loop)))
    print("wrote      : %s (%.0f KB)" % (dst, os.path.getsize(dst) / 1024))
    return 0


if __name__ == "__main__":
    sys.exit(main())
