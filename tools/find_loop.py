#!/usr/bin/env python3
"""Find the most seamless loop a track can support.

A sample-exact loop is only possible when the source literally repeats a
section.  Most music -- including this project's -- does not, so the tool
first reports whether an exact repeat exists, and then does the next best
thing:

  * estimate the tempo AND the downbeat, so the loop can start on a bar line
  * force the loop length to a whole number of bars, so the rhythm never
    stutters across the join
  * among those candidates, pick the one where the music after the end is
    most like the music after the start, judged on the spectrum rather than
    the raw waveform (the ear hears timbre and harmony at a seam, not phase)
  * match the loudness either side, so the loop does not jump level
  * align the joint to a zero crossing and apply a short equal-power fade,
    which removes the click without smearing the music

    tools/find_loop.py <input> <output.wav> [--min=S] [--max=S]
                       [--from=S] [--to=S]

--from/--to slice a region out of the source first, so one file holding two
pieces of music can be cut into two separate loops.
"""
import os, subprocess, sys, tempfile, wave
import numpy as np

RATE     = 22050
HOP      = 256
PEAK     = 0.89
FFMPEG   = os.environ.get("FFMPEG", "ffmpeg")
BPM_LO, BPM_HI = 70.0, 190.0


def decode(path):
    with tempfile.TemporaryDirectory() as tmp:
        wav = os.path.join(tmp, "d.wav")
        subprocess.run([FFMPEG, "-v", "error", "-y", "-i", path,
                        "-ac", "1", "-ar", str(RATE), "-f", "wav", wav], check=True)
        with wave.open(wav, "rb") as w:
            raw = w.readframes(w.getnframes())
    return np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0


def onset_envelope(x):
    """Spectral flux: how much the spectrum brightens frame to frame."""
    n = 1024
    frames = 1 + (len(x) - n) // HOP
    win = np.hanning(n)
    mags = np.empty((frames, n // 2 + 1))
    for i in range(frames):
        mags[i] = np.abs(np.fft.rfft(x[i * HOP:i * HOP + n] * win))
    flux = np.maximum(0.0, np.diff(mags, axis=0)).sum(axis=1)
    flux -= flux.mean()
    return flux


def estimate_beat(env):
    """Beat period in seconds, from the strongest onset autocorrelation peak."""
    ac = np.correlate(env, env, mode="full")[len(env) - 1:]
    lo = int(60.0 / BPM_HI * RATE / HOP)
    hi = int(60.0 / BPM_LO * RATE / HOP)
    hi = min(hi, len(ac) - 1)
    if hi <= lo:
        return None
    lag = lo + int(np.argmax(ac[lo:hi]))
    return lag * HOP / RATE


def estimate_downbeat(env, beat, bars=4):
    """Where the bar line falls, in seconds.

    Slides a pulse train at the bar period over the onset envelope; the offset
    that collects the most onset energy is the downbeat.
    """
    barf = beat * bars * RATE / HOP
    if barf < 2 or barf >= len(env):
        return 0.0
    best, off = -1e18, 0
    for o in range(int(barf)):
        idx = np.arange(o, len(env), barf).astype(int)
        idx = idx[idx < len(env)]
        score = float(env[idx].sum())
        if score > best:
            best, off = score, o
    return off * HOP / RATE


def spectrogram(x):
    n = 2048
    frames = 1 + (len(x) - n) // HOP
    win = np.hanning(n)
    idx = np.arange(frames)[:, None] * HOP + np.arange(n)
    S = np.abs(np.fft.rfft(x[idx] * win, axis=1))
    return np.log1p(S)


def exact_repeat(x):
    """Best normalised waveform self-similarity: ~1.0 means a literal repeat."""
    n = 1 << int(np.ceil(np.log2(2 * len(x))))
    X = np.fft.rfft(x, n)
    ac = np.fft.irfft(X * np.conj(X), n)[:len(x)]
    ac /= ac[0]
    lo, hi = int(1.0 * RATE), min(int(28.0 * RATE), len(ac))
    if hi <= lo:
        return 0.0, 0.0
    i = int(np.argmax(ac[lo:hi]))
    return float(ac[lo + i]), (lo + i) / RATE


def seam_cost(x, s, e, w):
    """How different the music after `end` is from the music after `start`.

    When the loop wraps, the listener expects x[e:], and gets x[s:] instead.
    The closer those two are, the less there is to hear.
    """
    a, b = x[e:e + w], x[s:s + w]
    n = min(len(a), len(b))
    if n < w:
        return np.inf
    diff = a[:n] - b[:n]
    return float(np.sqrt((diff ** 2).mean()))


def refine(x, s, e, span, w):
    """Nudge both ends a few hundred samples to sharpen the join."""
    best = (seam_cost(x, s, e, w), s, e)
    for de in range(-span, span + 1, 8):
        c = seam_cost(x, s, e + de, w)
        if c < best[0]:
            best = (c, s, e + de)
    _, s, e = best
    for ds in range(-span, span + 1, 8):
        if s + ds < 0:
            continue
        c = seam_cost(x, s + ds, e, w)
        if c < best[0]:
            best = (c, s + ds, e)
    return best[1], best[2], best[0]


def find_loop(x, beat, downbeat, min_s, max_s):
    """Best whole-bar loop, scored on spectral continuity across the join."""
    total = len(x)
    S = spectrogram(x)
    S = (S - S.mean(0)) / (S.std(0) + 1e-9)
    ctx = max(4, int(round((beat or 0.5) * RATE / HOP)))   # ~1 beat of context

    def similarity(fs, fe):
        a, b = S[fe:fe + ctx], S[fs:fs + ctx]
        n = min(len(a), len(b))
        if n < ctx:
            return -1.0
        na, nb = np.linalg.norm(a[:n]), np.linalg.norm(b[:n])
        if na < 1e-9 or nb < 1e-9:
            return -1.0
        return float((a[:n] * b[:n]).sum() / (na * nb))

    def loudness(i, w):
        seg = x[max(0, i - w):i]
        return float(np.sqrt((seg ** 2).mean())) if len(seg) else 0.0

    bar = (beat or 0.5) * 4.0
    lengths = [int(round(bar * n * RATE)) for n in range(1, 129)
               if min_s <= bar * n <= max_s] or \
              [int(L * RATE) for L in np.arange(min_s, max_s, 0.5)]

    start0 = int(round(downbeat * RATE))
    step   = int(round(bar * RATE))            # start on a bar line
    win    = int(0.25 * RATE)

    best = (-1e18, start0, start0 + lengths[0])
    for L in lengths:
        if L + ctx * HOP >= total:
            continue
        s = start0
        while s + L + ctx * HOP < total:
            sim  = similarity(s // HOP, (s + L) // HOP)
            la, lb = loudness(s + L, win), loudness(s + win, win)
            level = 1.0 - min(1.0, abs(la - lb) / (max(la, lb) + 1e-9))
            # Longer loops repeat less often, which is worth a little.
            length_bonus = 0.05 * (L / RATE) / max_s
            score = sim + 0.35 * level + length_bonus
            if score > best[0]:
                best = (score, s, s + L)
            s += step
    return best[1], best[2], best[0]


def snap_zero(x, i, span=400):
    """Nudge an index to a rising zero crossing, so the joint has no step."""
    lo, hi = max(1, i - span), min(len(x) - 1, i + span)
    cands = [j for j in range(lo, hi) if x[j - 1] <= 0.0 < x[j]]
    return min(cands, key=lambda j: abs(j - i)) if cands else i


def equal_power_fade(x, n):
    """A very short crossfade -- only enough to kill a residual sample step."""
    if n <= 0 or len(x) < 2 * n:
        return x
    t = np.linspace(0.0, np.pi / 2, n)
    fin, fout = np.sin(t), np.cos(t)
    head = x[:n] * fin + x[-n:] * fout
    return np.concatenate([head, x[n:-n]])


def write_wav(path, x):
    pcm = (np.clip(x, -1.0, 1.0) * 32767.0).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE)
        w.writeframes(pcm.tobytes())


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    opts = {a.split("=")[0]: a.split("=")[1] for a in sys.argv[1:] if "=" in a}
    if len(args) < 2:
        print(__doc__); return 1
    src, dst = args[0], args[1]
    min_s = float(opts.get("--min", 10.0))
    max_s = float(opts.get("--max", 28.0))

    x = decode(src)
    print("source      : %.2fs" % (len(x) / RATE))

    # Optional slice, for a file that holds more than one piece of music.
    a = float(opts.get("--from", 0.0))
    b = float(opts.get("--to", 0.0))
    if a > 0.0 or b > 0.0:
        lo = int(a * RATE)
        hi = int(b * RATE) if b > 0.0 else len(x)
        x = x[lo:hi]
        print("slice       : %.2fs -> %s  (%.2fs)"
              % (a, ("%.2fs" % b) if b > 0 else "end", len(x) / RATE))

    env  = onset_envelope(x)
    beat = estimate_beat(env)
    downbeat = estimate_downbeat(env, beat) if beat else 0.0
    if beat:
        print("tempo       : %.1f BPM (beat %.3fs, bar %.3fs, downbeat at %.3fs)"
              % (60.0 / beat, beat, beat * 4, downbeat))
    else:
        print("tempo       : not detected, sweeping lengths instead")

    rep, rep_lag = exact_repeat(x)
    print("exact repeat: best waveform self-similarity %.3f at %.2fs" % (rep, rep_lag))
    print("              %s"
          % ("a sample-exact loop is possible" if rep > 0.95 else
             "no literal repeat in this track, so the loop is matched musically"))

    s, e, score = find_loop(x, beat, downbeat, min_s, max_s)
    s, e = snap_zero(x, s), snap_zero(x, e)

    loop = x[s:e].copy()
    bar = (beat or 0.5) * 4.0
    print("loop point  : %.3fs -> %.3fs  (%.3fs = %.0f bars)"
          % (s / RATE, e / RATE, (e - s) / RATE, round((e - s) / RATE / bar)))
    print("match score : %.3f  (spectral continuity + level match)" % score)
    print("joint step  : %.5f before fade (signal peak %.3f)"
          % (abs(float(x[e - 1]) - float(x[s])), float(np.abs(x).max())))

    # How long a crossfade the seam needs depends on how well the two ends
    # actually match.  A near-exact join only wants a few milliseconds; a
    # through-composed track that never returns needs a real blend, and
    # pretending otherwise just makes the repeat audible.
    fade_s = float(opts.get("--fade", -1))
    if fade_s < 0:
        fade_s = 0.012 if score > 0.75 else 0.60
    fade = int(fade_s * RATE)
    print("crossfade   : %.0f ms (%s)"
          % (fade_s * 1000,
             "ends match closely" if fade_s < 0.05 else "ends do not match, blending"))
    loop = equal_power_fade(loop, fade)
    p = float(np.abs(loop).max())
    if p > 0:
        loop *= PEAK / p

    os.makedirs(os.path.dirname(dst) or ".", exist_ok=True)
    write_wav(dst, loop)
    # Report what the join actually sounds like: a level jump is the thing a
    # listener notices most in a background loop.
    w = int(0.25 * RATE)
    head = float(np.sqrt((loop[:w] ** 2).mean()))
    tail = float(np.sqrt((loop[-w:] ** 2).mean()))
    print("level at join: %.4f -> %.4f  (%.2f dB step)"
          % (tail, head, 20 * np.log10(max(head, 1e-9) / max(tail, 1e-9))))
    print("wrote       : %s (%.2fs, %.0f KB)"
          % (dst, len(loop) / RATE, os.path.getsize(dst) / 1024))
    return 0


if __name__ == "__main__":
    sys.exit(main())
