#!/usr/bin/env python3
"""Synthesises the sound effects of the Nautiloid prologue into res/sfx/*.wav.

Only the Python standard library is used. The samples are 8-bit mono WAVs at 13.3 kHz, the rate the
XGM2 driver plays PCM at (res/resources.res: `WAV name "sfx/x.wav" XGM2`; rescomp converts them).
Everything is generated from oscillators, noise and envelopes -- no third-party sound files. The
building blocks (tone, noise, envelopes, write_wav) come from the Wanderburg project.

Usage:
    generate_sfx.py            write the WAVs
    generate_sfx.py --list     only print names and lengths
"""
import argparse
import math
import os
import random
import struct
import wave

RATE = 13300
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "sfx")
TWO_PI = 2 * math.pi


class Rng:
    def __init__(self, seed):
        self.r = random.Random(seed)

    def noise(self):
        return self.r.uniform(-1.0, 1.0)


def n_samples(seconds):
    return int(seconds * RATE)


def env_exp(i, n, decay=5.0, attack=0.01):
    """Fast attack, exponential decay over the length of the sound."""
    t = i / n
    a = min(1.0, t / attack) if attack > 0 else 1.0
    return a * math.exp(-decay * t)


def lowpass(samples, k):
    """One-pole low-pass filter, k in (0, 1]: smaller = duller."""
    out = []
    y = 0.0
    for x in samples:
        y += k * (x - y)
        out.append(y)
    return out


def tone(seconds, f0, f1=None, shape="square", decay=4.0, duty=0.5, attack=0.005, vib=0.0):
    """A tone that glides from f0 to f1 (exponentially) with an exponential decay."""
    n = n_samples(seconds)
    f1 = f0 if f1 is None else f1
    phase = 0.0
    out = []
    for i in range(n):
        t = i / n
        f = f0 * (f1 / f0) ** t
        if vib:
            f *= 1.0 + vib * math.sin(TWO_PI * 30 * i / RATE)
        phase += f / RATE
        p = phase % 1.0
        if shape == "square":
            v = 1.0 if p < duty else -1.0
        elif shape == "saw":
            v = 2 * p - 1
        elif shape == "tri":
            v = 4 * abs(p - 0.5) - 1
        else:
            v = math.sin(TWO_PI * p)
        out.append(v * env_exp(i, n, decay, attack))
    return out


def noise(seconds, rng, decay=5.0, k=1.0, attack=0.003):
    n = n_samples(seconds)
    raw = [rng.noise() for _ in range(n)]
    if k < 1.0:
        raw = lowpass(raw, k)
    return [v * env_exp(i, n, decay, attack) for i, v in enumerate(raw)]


def mix(*parts):
    n = max(len(p) for p in parts)
    out = [0.0] * n
    for p in parts:
        for i, v in enumerate(p):
            out[i] += v
    return out


def gain(samples, g):
    return [v * g for v in samples]


def concat(*parts):
    out = []
    for p in parts:
        out += p
    return out


def silence(seconds):
    return [0.0] * n_samples(seconds)


def normalise(samples, peak=0.9):
    m = max(abs(v) for v in samples) or 1.0
    return [v * peak / m for v in samples]


def write_wav(name, samples):
    samples = normalise(samples)
    # short fade at the end, so there is no click
    fade = min(len(samples), 60)
    for i in range(fade):
        samples[len(samples) - 1 - i] *= i / fade
    data = bytes(max(0, min(255, int(round(128 + 127 * v)))) for v in samples)
    path = os.path.join(OUT_DIR, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(1)
        w.setframerate(RATE)
        w.writeframes(data)
    return path, len(samples)


# ------------------------------------------------------------------ the sounds

def sfx_hit(rng):
    """A weapon hits: a metallic clang over a short thud."""
    return mix(tone(0.14, 880, 620, "square", decay=8.0, duty=0.2),
               gain(tone(0.10, 140, 60, "sine", decay=7.0), 0.8),
               gain(noise(0.06, rng, decay=10.0, k=0.9), 0.5))


def sfx_miss(rng):
    """A swing that misses: a whoosh, noise swelling and fading."""
    n = n_samples(0.18)
    out, y = [], 0.0
    for i in range(n):
        t = i / n
        y += (0.1 + 0.5 * t) * (rng.noise() - y)
        out.append(y * math.sin(math.pi * t))
    return out


def sfx_hurt(rng):
    """The party takes a hit: a heavy thud and a falling buzz."""
    return mix(tone(0.22, 240, 80, "saw", decay=4.0),
               gain(tone(0.16, 110, 45, "sine", decay=5.0), 0.9),
               gain(noise(0.10, rng, decay=8.0, k=0.5), 0.5))


def sfx_spell(rng):
    """A spell: a bright rising shimmer with vibrato."""
    return mix(tone(0.30, 500, 1500, "tri", decay=2.5, vib=0.03),
               gain(tone(0.30, 750, 2250, "square", decay=3.5, duty=0.15), 0.35))


def sfx_fire(rng):
    """Fire (Feuerpfeil, Brennende Hände, the burning sword): a roaring hiss."""
    return mix(noise(0.35, rng, decay=3.0, k=0.35, attack=0.04), gain(tone(0.35, 90, 60, "saw", decay=3.0), 0.3))


def sfx_heal(rng):
    """Healing: a soft rising arpeggio."""
    notes = (523, 659, 784, 1047)
    return concat(*[tone(0.07, f, f, "tri", decay=1.5) for f in notes[:-1]],
                  tone(0.25, notes[-1], notes[-1], "tri", decay=3.0, vib=0.01))


def sfx_explosion(rng):
    """The acid tank bursts: a wet blast and a low sweep."""
    return mix(noise(0.55, rng, decay=4.0, k=0.3), gain(tone(0.55, 120, 30, "sine", decay=3.5), 0.9))


def sfx_door(rng):
    """A sphincter door opens: a fleshy squelch -- a low wobbling glide and damp noise."""
    return mix(tone(0.35, 70, 160, "sine", decay=2.5, vib=0.12), gain(noise(0.3, rng, decay=4.0, k=0.15), 0.8))


def sfx_land(rng):
    """Lae'zel lands in front of you: a heavy thud, then a blade ringing out."""
    thud = mix(tone(0.2, 100, 40, "sine", decay=6.0), gain(noise(0.12, rng, decay=9.0, k=0.25), 0.9))
    ring = mix(tone(0.5, 1320, 1300, "tri", decay=3.0), gain(tone(0.5, 1980, 1960, "square", decay=5.0, duty=0.1), 0.3))
    return concat(thud, gain(ring, 0.7))


def sfx_dice(rng):
    """The d20 rolls: a rattle of short clicks, slowing down."""
    out = []
    for k in range(7):
        click = mix(tone(0.018, 2200 - k * 120, 1800, "square", decay=12.0, duty=0.3),
                    gain(noise(0.015, rng, decay=15.0), 0.6))
        out += click + silence(0.02 + k * 0.012)
    return out


def sfx_victory(rng):
    """A fight is won: a short fanfare."""
    return concat(tone(0.10, 523, 523, "square", decay=1.5, duty=0.25),
                  tone(0.10, 659, 659, "square", decay=1.5, duty=0.25),
                  tone(0.10, 784, 784, "square", decay=1.5, duty=0.25),
                  tone(0.40, 1047, 1047, "square", decay=2.5, duty=0.25, vib=0.01))


def sfx_gameover(rng):
    """Game over: a slow falling minor phrase."""
    return concat(tone(0.30, 440, 440, "tri", decay=1.5), tone(0.30, 415, 415, "tri", decay=1.5),
                  tone(0.30, 392, 392, "tri", decay=1.5), tone(0.8, 330, 300, "tri", decay=2.0))


def sfx_menu(rng):
    """Menu cursor: a soft tick."""
    return tone(0.03, 1200, 1100, "square", decay=6.0, duty=0.25)


def sfx_quake(rng):
    """The ship shudders and crashes: a long rumble."""
    return mix(noise(0.9, rng, decay=2.0, k=0.12, attack=0.05), gain(tone(0.9, 60, 30, "sine", decay=2.0), 0.9))


def sfx_item(rng):
    """Loot: a bright two-tone ping."""
    return concat(tone(0.06, 1318, 1318, "square", decay=2.0, duty=0.25),
                  tone(0.14, 1760, 1760, "square", decay=4.0, duty=0.25))


def sfx_intro(rng):
    """Intro logo sweep (taken over 1:1 from the Wanderburg project): a bright rising shimmer that ends in a soft chord."""
    n = n_samples(1.3)
    sweep = []
    phase = 0.0
    for i in range(n):
        t = i / n
        f = 300 * (12 ** t)                                  # glides up about 3.5 octaves
        phase += f / RATE
        sweep.append(math.sin(TWO_PI * phase) * math.sin(math.pi * t) ** 1.5)
    chord = mix(tone(0.6, 523, 523, "tri", decay=3.0, attack=0.01), tone(0.6, 659, 659, "tri", decay=3.0, attack=0.01),
                tone(0.6, 784, 784, "tri", decay=3.0, attack=0.01))
    return mix(sweep, concat(silence(0.75), gain(chord, 0.9)))


SOUNDS = {
    "sfx_hit": sfx_hit,
    "sfx_miss": sfx_miss,
    "sfx_hurt": sfx_hurt,
    "sfx_spell": sfx_spell,
    "sfx_fire": sfx_fire,
    "sfx_heal": sfx_heal,
    "sfx_explosion": sfx_explosion,
    "sfx_door": sfx_door,
    "sfx_land": sfx_land,
    "sfx_dice": sfx_dice,
    "sfx_victory": sfx_victory,
    "sfx_gameover": sfx_gameover,
    "sfx_menu": sfx_menu,
    "sfx_quake": sfx_quake,
    "sfx_item": sfx_item,
    "sfx_intro": sfx_intro,
}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--list", action="store_true", help="print names and lengths, write nothing")
    args = ap.parse_args()

    os.makedirs(OUT_DIR, exist_ok=True)
    total = 0
    for i, (name, fn) in enumerate(SOUNDS.items()):
        samples = fn(Rng(1000 + i))
        total += len(samples)
        if args.list:
            print(f"{name:16s} {len(samples) / RATE:5.2f} s  {len(samples):6d} bytes")
        else:
            path, n = write_wav(name, samples)
            print(f"wrote {os.path.relpath(path)}  {n / RATE:.2f} s, {n} bytes")
    print(f"total {total} bytes of PCM")


if __name__ == "__main__":
    main()
