#!/usr/bin/env python3
"""Looks at the sound tools/emutest.sh recorded (out/emutest/audio.wav): per half second, the
loudness and the strongest frequencies, to check music and effects without listening.

Usage: tools/audiocheck.py [from_second [to_second]]
"""
import math
import os
import struct
import sys

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "out", "emutest", "audio.wav")


def read(path):
    d = open(path, "rb").read()
    pos, fmt, data = 12, None, None
    while pos + 8 <= len(d):
        cid, size = d[pos:pos + 4], struct.unpack("<I", d[pos + 4:pos + 8])[0]
        body = d[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            tag, ch, rate, _, _, bits = struct.unpack("<HHIIHH", body[:16])
            if tag == 0xFFFE:
                tag = struct.unpack("<H", body[24:26])[0]
            fmt = (tag, ch, rate, bits)
        elif cid == b"data":
            data = body
        pos += 8 + size + (size & 1)
    tag, ch, rate, bits = fmt
    if tag == 3:
        vals = struct.unpack("<%df" % (len(data) // 4), data[:len(data) // 4 * 4])
    else:
        vals = [v / 32768 for v in struct.unpack("<%dh" % (len(data) // 2), data[:len(data) // 2 * 2])]
    mono = [sum(vals[i:i + ch]) / ch for i in range(0, len(vals) - ch + 1, ch)]
    return rate, mono


def peaks(seg, rate, count=3):
    """Strongest frequencies (Hz) by a coarse DFT over 60-2000 Hz."""
    n = min(len(seg), 4096)
    seg = seg[:n]
    best = []
    for f in range(60, 2000, 10):
        w = 2 * math.pi * f / rate
        re = sum(seg[i] * math.cos(w * i) for i in range(0, n, 2))
        im = sum(seg[i] * math.sin(w * i) for i in range(0, n, 2))
        best.append((re * re + im * im, f))
    best.sort(reverse=True)
    out = []
    for p, f in best:
        if all(abs(f - g) > 30 for g in out):
            out.append(f)
        if len(out) == count:
            break
    return out


if __name__ == "__main__":
    rate, mono = read(PATH)
    a = float(sys.argv[1]) if len(sys.argv) > 1 else 0
    b = float(sys.argv[2]) if len(sys.argv) > 2 else len(mono) / rate
    print(f"{len(mono) / rate:.1f} s at {rate} Hz")
    t = a
    while t < b:
        seg = mono[int(t * rate):int((t + 0.5) * rate)]
        if not seg:
            break
        rms = math.sqrt(sum(v * v for v in seg) / len(seg))
        print(f"{t:6.1f}s  rms {rms:.3f}  " + (" ".join(f"{f}Hz" for f in peaks(seg, rate)) if rms > 0.005 else "silent"))
        t += 0.5
